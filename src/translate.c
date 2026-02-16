#include "common.h"
#include <libxml/HTMLparser.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <ctype.h>

// Simple callback for curl write
// Dynamic write callback
size_t write_callback_dynamic(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t real_size = size * nmemb;
    struct {
        char *response;
        size_t size;
    } *mem = userdata;

    char *ptr_realloc = realloc(mem->response, mem->size + real_size + 1);
    if(ptr_realloc == NULL) return 0; // Out of memory

    mem->response = ptr_realloc;
    memcpy(&(mem->response[mem->size]), ptr, real_size);
    mem->size += real_size;
    mem->response[mem->size] = 0;

    return real_size;
}

char* llm_translate(const char *text, config_t *config, const char *context_string) {
    if (!text || strlen(text) < 1) return NULL;

    // Skip whitespace-only strings (including NBSP 0xC2 0xA0)
    int is_empty = 1;
    size_t len = strlen(text);
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)text[i];
        if (isspace(c)) continue;
        // Check for NBSP in UTF-8 (0xC2 0xA0)
        if (c == 0xC2 && i + 1 < len && (unsigned char)text[i+1] == 0xA0) {
            i++; // Skip the next byte too
            continue;
        }
        is_empty = 0;
        break;
    }
    if (is_empty) return NULL;

    // printf("DEBUG: Translating '%s'\n", text);

    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    struct curl_slist *headers = NULL;
    char auth_header[256];
    
    // Use configured API KEY or empty string if not provided (some local LLMs don't need it)
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", config->api_key ? config->api_key : "lm-studio");
    
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header);

    // Prepare JSON payload
    struct json_object *payload = json_object_new_object();
    json_object_object_add(payload, "model", json_object_new_string(config->model));
    
    struct json_object *messages = json_object_new_array();
    
    struct json_object *system_msg = json_object_new_object();
    json_object_object_add(system_msg, "role", json_object_new_string("system"));
    char system_prompt[8192]; // Increased buffer
    
    // Use the template from config
    snprintf(system_prompt, sizeof(system_prompt), 
        config->prompt_translation,
        config->target_language,
        (context_string && strlen(context_string) > 0) ? context_string : ""
    );
    json_object_object_add(system_msg, "content", json_object_new_string(system_prompt));
    json_object_array_add(messages, system_msg);

    struct json_object *user_msg = json_object_new_object();
    json_object_object_add(user_msg, "role", json_object_new_string("user"));
    json_object_object_add(user_msg, "content", json_object_new_string(text));
    json_object_array_add(messages, user_msg);

    json_object_object_add(payload, "messages", messages);
    // Add temperature to be deterministic if possible
    json_object_object_add(payload, "temperature", json_object_new_double(0.3));

    const char *post_fields = json_object_to_json_string(payload);

    // Dynamic buffer for response
    char *response_buffer = calloc(1, 1);
    size_t response_size = 0;

    // Use configured endpoint or default to OpenAI
    const char *url = config->api_endpoint ? config->api_endpoint : "https://api.openai.com/v1/chat/completions";

    // printf("DEBUG: Sending request to %s\n", url);
    // printf("DEBUG: Payload: %s\n", post_fields); // Less noise

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L); // 60 seconds timeout
    
    // Improved write callback with dynamic resizing
    struct {
        char *response;
        size_t size;
    } response_data = {response_buffer, response_size};

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback_dynamic);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

    CURLcode res = curl_easy_perform(curl);
    
    char *translated_text = NULL;

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    } else {
        // Parse JSON response
        struct json_object *parsed_response = json_tokener_parse(response_data.response);
        if (parsed_response) {
            struct json_object *choices;
            if (json_object_object_get_ex(parsed_response, "choices", &choices)) {
                struct json_object *first_choice = json_object_array_get_idx(choices, 0);
                if (first_choice) {
                    struct json_object *message;
                    if (json_object_object_get_ex(first_choice, "message", &message)) {
                        struct json_object *content;
                        if (json_object_object_get_ex(message, "content", &content)) {
                            translated_text = strdup(json_object_get_string(content));
                        }
                    }
                }
            }
            json_object_put(parsed_response);
        } else {
             fprintf(stderr, "Failed to parse JSON response: %s\n", response_data.response);
        }
    }

    // Cleanup
    free(response_data.response);
    json_object_put(payload);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return translated_text;
}

void translate_nodes(xmlNode *node, config_t *config, const char *context_string) {
    xmlNode *cur = NULL;
    for (cur = node; cur; cur = cur->next) {
        if (cur->type == XML_TEXT_NODE) {
            char *translated = llm_translate((char*)cur->content, config, context_string);
            if (translated) {
                xmlNodeSetContent(cur, (const xmlChar*)translated);
                free(translated);
            }
        }
        translate_nodes(cur->children, config, context_string);
    }
}

int translate_xhtml(const char *path, config_t *config, const char *context_string) {
    xmlDocPtr doc = htmlReadFile(path, "UTF-8", HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
    if (!doc) return -1;

    translate_nodes(xmlDocGetRootElement(doc), config, context_string);

    // Remove any existing XML declaration nodes (PIs) to avoid duplication
    // because xmlSaveFormatFileEnc adds its own.
    xmlNodePtr cur = doc->children;
    while (cur) {
        xmlNodePtr next = cur->next;
        if (cur->type == XML_PI_NODE && xmlStrcasecmp(cur->name, (const xmlChar*)"xml") == 0) {
            xmlUnlinkNode(cur);
            xmlFreeNode(cur);
        }
        cur = next;
    }

    xmlSaveFormatFileEnc(path, doc, "UTF-8", 1);
    xmlFreeDoc(doc);
    return 0;
}

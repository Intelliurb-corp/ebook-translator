#include "context.h"
#include <json-c/json.h>
#include <curl/curl.h>

// Reuse write callback from translate.c or duplicate it here for independence
// For now, duplicating to avoid complex refactor of translate.c
static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t real_size = size * nmemb;
    struct {
        char *response;
        size_t size;
    } *mem = userdata;

    char *ptr_realloc = realloc(mem->response, mem->size + real_size + 1);
    if(ptr_realloc == NULL) return 0;

    mem->response = ptr_realloc;
    memcpy(&(mem->response[mem->size]), ptr, real_size);
    mem->size += real_size;
    mem->response[mem->size] = 0;

    return real_size;
}

context_t* create_context() {
    context_t *ctx = calloc(1, sizeof(context_t));
    return ctx;
}

void free_context(context_t *ctx) {
    if (!ctx) return;
    free(ctx->summary);
    free(ctx->characters);
    free(ctx->locations);
    free(ctx->jargon);
    free(ctx);
}

int save_context(const char *filename, context_t *ctx) {
    if (!filename || !ctx) return -1;
    
    struct json_object *jobj = json_object_new_object();
    if (ctx->summary) json_object_object_add(jobj, "summary", json_object_new_string(ctx->summary));
    if (ctx->characters) json_object_object_add(jobj, "characters", json_object_new_string(ctx->characters));
    if (ctx->locations) json_object_object_add(jobj, "locations", json_object_new_string(ctx->locations));
    if (ctx->jargon) json_object_object_add(jobj, "jargon", json_object_new_string(ctx->jargon));
    
    const char *json_str = json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PRETTY);
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror("Failed to open context file for writing");
        json_object_put(jobj);
        return -1;
    }
    fprintf(fp, "%s", json_str);
    fclose(fp);
    json_object_put(jobj);
    return 0;
}

context_t* load_context(const char *filename) {
    if (!filename) return NULL;
    
    FILE *fp = fopen(filename, "r");
    if (!fp) return NULL; // No existing context is fine
    
    // Check file size
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (fsize <= 0) {
        fclose(fp);
        return NULL;
    }

    char *buffer = malloc(fsize + 1);
    fread(buffer, 1, fsize, fp);
    fclose(fp);
    buffer[fsize] = 0;
    
    struct json_object *jobj = json_tokener_parse(buffer);
    free(buffer);
    
    if (!jobj) return NULL;
    
    context_t *ctx = create_context();
    struct json_object *tmp;
    
    if (json_object_object_get_ex(jobj, "summary", &tmp))
        ctx->summary = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(jobj, "characters", &tmp))
        ctx->characters = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(jobj, "locations", &tmp))
        ctx->locations = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(jobj, "jargon", &tmp))
        ctx->jargon = strdup(json_object_get_string(tmp));
        
    json_object_put(jobj);
    return ctx;
}

// Internal helper for LLM calls
static char* perform_llm_request(const char *system_prompt, const char *user_content, config_t *config) {
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    struct curl_slist *headers = NULL;
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", config->api_key ? config->api_key : "lm-studio");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header);

    struct json_object *payload = json_object_new_object();
    json_object_object_add(payload, "model", json_object_new_string(config->model));
    json_object_object_add(payload, "temperature", json_object_new_double(0.1)); // Low temp for extraction

    struct json_object *messages = json_object_new_array();
    
    struct json_object *sys_msg = json_object_new_object();
    json_object_object_add(sys_msg, "role", json_object_new_string("system"));
    json_object_object_add(sys_msg, "content", json_object_new_string(system_prompt));
    json_object_array_add(messages, sys_msg);
    
    struct json_object *usr_msg = json_object_new_object();
    json_object_object_add(usr_msg, "role", json_object_new_string("user"));
    json_object_object_add(usr_msg, "content", json_object_new_string(user_content));
    json_object_array_add(messages, usr_msg);
    
    json_object_object_add(payload, "messages", messages);
    
    const char *post_fields = json_object_to_json_string(payload);
    
    char *response_buffer = calloc(1, 1);
    struct {
        char *response;
        size_t size;
    } response_data = {response_buffer, 0};

    const char *url = config->api_endpoint ? config->api_endpoint : "https://api.openai.com/v1/chat/completions";
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    CURLcode res = curl_easy_perform(curl);
    
    char *result = NULL;
    if (res == CURLE_OK) {
        struct json_object *parsed = json_tokener_parse(response_data.response);
        if (parsed) {
            struct json_object *choices, *choice, *message, *content;
            if (json_object_object_get_ex(parsed, "choices", &choices) &&
                (choice = json_object_array_get_idx(choices, 0)) &&
                json_object_object_get_ex(choice, "message", &message) &&
                json_object_object_get_ex(message, "content", &content)) {
                result = strdup(json_object_get_string(content));
            }
            json_object_put(parsed);
        }
    } else {
        fprintf(stderr, "Context LLM request failed: %s\n", curl_easy_strerror(res));
    }

    free(response_data.response);
    json_object_put(payload);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result;
}

int init_context_with_llm(context_t *ctx, const char *initial_text, config_t *config) {
    const char *prompt = 
        "You are a literary assistant. Analyze the following text (beginning of a book) "
        "and extract the following information in JSON format:\n"
        "{\n"
        "  \"summary\": \"Brief summary of the opening\",\n"
        "  \"characters\": \"List of characters mentioned with brief description\",\n"
        "  \"locations\": \"List of locations mentioned\",\n"
        "  \"jargon\": \"Specific terms, dates, or author style notes\"\n"
        "}\n"
        "Do not include any extra text.";
        
    char *json_response = perform_llm_request(prompt, initial_text, config);
    if (!json_response) return -1;
    
    // Parse the response
    struct json_object *jobj = json_tokener_parse(json_response);
    if (!jobj) {
        // Fallback: simple summary if JSON fails
        ctx->summary = json_response; // Take whatever we got
        return 0;
    }
    
    struct json_object *tmp;
    if (json_object_object_get_ex(jobj, "summary", &tmp)) ctx->summary = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(jobj, "characters", &tmp)) ctx->characters = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(jobj, "locations", &tmp)) ctx->locations = strdup(json_object_get_string(tmp));
    if (json_object_object_get_ex(jobj, "jargon", &tmp)) ctx->jargon = strdup(json_object_get_string(tmp));
    
    json_object_put(jobj);
    free(json_response);
    return 0;
}

int update_context_with_llm(context_t *ctx, const char *new_text, config_t *config) {
    char system_prompt[2048];
    snprintf(system_prompt, sizeof(system_prompt),
        "You are a helpful assistant maintaining the context of a book translation.\n"
        "Current Context:\n"
        "Summary: %s\n"
        "Characters: %s\n"
        "Locations: %s\n"
        "Jargon: %s\n"
        "\n"
        "Task: Read the NEW TEXT below and UPDATE the context. Add new characters, update plot summary, etc.\n"
        "Return the updated context in the SAME JSON format.",
        ctx->summary ? ctx->summary : "",
        ctx->characters ? ctx->characters : "",
        ctx->locations ? ctx->locations : "",
        ctx->jargon ? ctx->jargon : ""
    );
    
    char *json_response = perform_llm_request(system_prompt, new_text, config);
    if (!json_response) return -1;
    
    // Parse and replace
    struct json_object *jobj = json_tokener_parse(json_response);
    if (!jobj) {
        free(json_response);
        return -1;
    }
    
    struct json_object *tmp;
    if (json_object_object_get_ex(jobj, "summary", &tmp)) {
        free(ctx->summary);
        ctx->summary = strdup(json_object_get_string(tmp));
    }
    if (json_object_object_get_ex(jobj, "characters", &tmp)) {
        free(ctx->characters);
        ctx->characters = strdup(json_object_get_string(tmp));
    }
    if (json_object_object_get_ex(jobj, "locations", &tmp)) {
        free(ctx->locations);
        ctx->locations = strdup(json_object_get_string(tmp));
    }
    if (json_object_object_get_ex(jobj, "jargon", &tmp)) {
        free(ctx->jargon);
        ctx->jargon = strdup(json_object_get_string(tmp));
    }
    
    json_object_put(jobj);
    free(json_response);
    return 0;
}

char* format_context_for_prompt(context_t *ctx) {
    if (!ctx) return NULL;
    char buffer[4096];
    snprintf(buffer, sizeof(buffer),
        "--- STORY CONTEXT ---\n"
        "SUMMARY: %s\n"
        "CHARACTERS: %s\n"
        "LOCATIONS: %s\n"
        "TERMS: %s\n"
        "---------------------\n",
        ctx->summary ? ctx->summary : "N/A",
        ctx->characters ? ctx->characters : "N/A",
        ctx->locations ? ctx->locations : "N/A",
        ctx->jargon ? ctx->jargon : "N/A"
    );
    return strdup(buffer);
}

#include "common.h"
#include <json-c/json.h>

void free_config(config_t *config) {
    if (!config) return;
    free(config->llm_provider);
    free(config->model);
    free(config->api_key);
    free(config->target_language);
    free(config->context_strategy);
    free(config->tone);
    free(config->api_endpoint);
    free(config->context_file);
    free(config->prompt_context_init);
    free(config->prompt_context_update);
    free(config->prompt_translation);
    free(config);
}


// Helper to read prompt file
static char* read_prompt(const char *filename) {
    // Try local conf/ first
    char path[1024];
    snprintf(path, sizeof(path), "./conf/%s", filename);
    FILE *f = fopen(path, "r");
    if (!f) {
        // Try system path
        snprintf(path, sizeof(path), "/usr/local/etc/ebook-translator/%s", filename);
        f = fopen(path, "r");
    }
    
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buffer = malloc(length + 1);
    if (buffer) {
        fread(buffer, 1, length, f);
        buffer[length] = 0;
    }
    fclose(f);
    return buffer;
}

config_t* load_config(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror("Error opening config file");
        return NULL;
    }

    struct json_object *parsed_json;
    struct json_object *llm_provider, *model, *api_key, *target_language, *context_window, *context_strategy, *tone, *api_endpoint;

    char buffer[4096];
    fread(buffer, 1, sizeof(buffer), fp);
    fclose(fp);

    parsed_json = json_tokener_parse(buffer);
    if (!parsed_json) {
        fprintf(stderr, "Error parsing JSON config\n");
        return NULL;
    }

    config_t *config = calloc(1, sizeof(config_t));
    
    if (json_object_object_get_ex(parsed_json, "llm_provider", &llm_provider))
        config->llm_provider = strdup(json_object_get_string(llm_provider));
    
    if (json_object_object_get_ex(parsed_json, "model", &model))
        config->model = strdup(json_object_get_string(model));
    
    if (json_object_object_get_ex(parsed_json, "api_key", &api_key))
        config->api_key = strdup(json_object_get_string(api_key));
    
    if (json_object_object_get_ex(parsed_json, "target_language", &target_language))
        config->target_language = strdup(json_object_get_string(target_language));
    
    if (json_object_object_get_ex(parsed_json, "context_window", &context_window))
        config->context_window = json_object_get_int(context_window);
    
    if (json_object_object_get_ex(parsed_json, "context_strategy", &context_strategy))
        config->context_strategy = strdup(json_object_get_string(context_strategy));
    
    if (json_object_object_get_ex(parsed_json, "tone", &tone))
        config->tone = strdup(json_object_get_string(tone));

    if (json_object_object_get_ex(parsed_json, "api_endpoint", &api_endpoint))
        config->api_endpoint = strdup(json_object_get_string(api_endpoint));

    struct json_object *context_file;
    if (json_object_object_get_ex(parsed_json, "context_file", &context_file))
        config->context_file = strdup(json_object_get_string(context_file));

    json_object_put(parsed_json);

    config->prompt_context_init = read_prompt("prompt_context_init.md");
    config->prompt_context_update = read_prompt("prompt_context_update.md");
    config->prompt_translation = read_prompt("prompt_translation.md");

    // Defaults if files missing (hardcoded fallbacks)
    if (!config->prompt_context_init) config->prompt_context_init = strdup("You are a literary assistant. Analyze the text and extract: summary, characters, locations, jargon. JSON format.");
    if (!config->prompt_context_update) config->prompt_context_update = strdup("Update the context (summary, characters, locations, jargon) based on new text. Return JSON.");
    if (!config->prompt_translation) config->prompt_translation = strdup("Translate to %s. Preserve formatting. %s");

    return config;
}

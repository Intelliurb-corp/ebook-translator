#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    char *llm_provider;
    char *model;
    char *api_key;
    char *target_language;
    int context_window;
    char *context_strategy;
    char *tone;
    char *api_endpoint;
    char *context_file;
    char *prompt_context_init;
    char *prompt_context_update;
    char *prompt_translation;
} config_t;

config_t* load_config(const char *path);
void free_config(config_t *config);
int translate_xhtml(const char *path, config_t *config, const char *context_string);

#endif // COMMON_H

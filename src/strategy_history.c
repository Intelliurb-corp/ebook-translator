#include "context_strategy.h"
#include "context.h" // Reuse existing context logic structure
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// This strategy wraps the existing context.c logic
// Ideally we would move the logic fully here, but for now we wrap it.

typedef struct {
    context_t *ctx;
} HistoryState;

static void* history_init(config_t *config) {
    if (!config->context_file) return NULL;
    
    HistoryState *state = calloc(1, sizeof(HistoryState));
    // Try to load existing
    state->ctx = load_context(config->context_file);
    if (!state->ctx) {
        state->ctx = create_context();
        printf("Creating new context history at %s\n", config->context_file);
    } else {
        printf("Loaded context history from %s\n", config->context_file);
    }
    return state;
}

static char* history_get_prompt(void *state_ptr, config_t *config) {
    HistoryState *state = (HistoryState*)state_ptr;
    if (!state || !state->ctx) return NULL;
    
    // Use the function from context.c
    return format_context_for_prompt(state->ctx);
}

static void history_update(void *state_ptr, const char *text, config_t *config) {
    HistoryState *state = (HistoryState*)state_ptr;
    if (!state || !state->ctx) return;
    
    // Logic from main.c
    if (!state->ctx->summary || strlen(state->ctx->summary) == 0) {
         init_context_with_llm(state->ctx, text, config);
    } else {
         update_context_with_llm(state->ctx, text, config);
    }
    save_context(config->context_file, state->ctx);
}

static void history_cleanup(void *state_ptr) {
    HistoryState *state = (HistoryState*)state_ptr;
    if (!state) return;
    if (state->ctx) free_context(state->ctx);
    free(state);
}

ContextStrategy* create_history_strategy() {
    ContextStrategy *strategy = calloc(1, sizeof(ContextStrategy));
    strategy->name = "History";
    strategy->init = history_init;
    strategy->get_prompt = history_get_prompt;
    strategy->update = history_update;
    strategy->cleanup = history_cleanup;
    return strategy;
}

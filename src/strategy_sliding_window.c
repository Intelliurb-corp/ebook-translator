#include "context_strategy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *buffer;
    size_t size;
    size_t capacity;
} SlidingState;

static void* sliding_init(config_t *config) {
    if (config->sliding_window_size <= 0) return NULL;
    
    SlidingState *state = calloc(1, sizeof(SlidingState));
    state->capacity = config->sliding_window_size;
    state->buffer = calloc(1, state->capacity + 1);
    state->size = 0;
    
    printf("Initialized Sliding Window context (size: %zu)\n", state->capacity);
    return state;
}

static char* sliding_get_prompt(void *state_ptr, config_t *config) {
    SlidingState *state = (SlidingState*)state_ptr;
    if (!state || state->size == 0) return NULL;
    
    char *out = malloc(state->size + 100);
    snprintf(out, state->size + 100, "\n--- PREVIOUS TEXT ---\n%s\n---------------------\n", state->buffer);
    return out;
}

static void sliding_update(void *state_ptr, const char *text, config_t *config) {
    SlidingState *state = (SlidingState*)state_ptr;
    if (!state || !text) return;
    
    size_t text_len = strlen(text);
    
    // If new text is larger than capacity, take just the end
    if (text_len >= state->capacity) {
        memcpy(state->buffer, text + (text_len - state->capacity), state->capacity);
        state->buffer[state->capacity] = 0;
        state->size = state->capacity;
    } else {
        // Shift existing content to make room
        size_t available = state->capacity - state->size;
        
        if (available >= text_len) {
            // Append
            strcat(state->buffer, text);
            state->size += text_len;
        } else {
            // Shift left
            size_t shift = text_len - available;
            memmove(state->buffer, state->buffer + shift, state->size - shift);
            strcat(state->buffer, text);
            state->size = state->capacity; // Should be full now
        }
    }
}

static void sliding_cleanup(void *state_ptr) {
    SlidingState *state = (SlidingState*)state_ptr;
    if (!state) return;
    free(state->buffer);
    free(state);
}

ContextStrategy* create_sliding_window_strategy() {
    ContextStrategy *strategy = calloc(1, sizeof(ContextStrategy));
    strategy->name = "SlidingWindow";
    strategy->init = sliding_init;
    strategy->get_prompt = sliding_get_prompt;
    strategy->update = sliding_update;
    strategy->cleanup = sliding_cleanup;
    return strategy;
}

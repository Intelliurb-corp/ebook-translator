#ifndef CONTEXT_STRATEGY_H
#define CONTEXT_STRATEGY_H

#include "common.h"

// Forward declaration to avoid circular dependency
typedef struct ContextStrategy ContextStrategy;

struct ContextStrategy {
    const char *name;
    
    // Initialize the strategy
    // Returns a pointer to internal state, or NULL on failure
    void* (*init)(config_t *config);
    
    // Get the context prompt string to be injected
    // Caller must free the returned string
    char* (*get_prompt)(void *state, config_t *config);
    
    // Update the strategy with new text
    // text: The translated text (or original, depending on strategy needs)
    void (*update)(void *state, const char *text, config_t *config);
    
    // Cleanup internal state
    void (*cleanup)(void *state);
    
    // The internal state/data for this instance
    void *state;
};

// Constructor-like functions for specific strategies
ContextStrategy* create_history_strategy();
ContextStrategy* create_sliding_window_strategy();

#endif // CONTEXT_STRATEGY_H

#ifndef CONTEXT_H
#define CONTEXT_H

#include "common.h"

typedef struct {
    char *summary;     // Running summary of the plot
    char *characters;  // JSON list or text description of characters
    char *locations;   // JSON list or text description of locations
    char *jargon;      // Specific terminology
} context_t;

context_t* create_context();
void free_context(context_t *ctx);

// File I/O
int save_context(const char *filename, context_t *ctx);
context_t* load_context(const char *filename);

// LLM Interaction
// Initializes context from the first chunk of text (Author, chunks, etc)
int init_context_with_llm(context_t *ctx, const char *initial_text, config_t *config);

// Updates context based on new text segment
int update_context_with_llm(context_t *ctx, const char *new_text, config_t *config);

// Returns a formatted string to be injected into the translation prompt
char* format_context_for_prompt(context_t *ctx);

#endif // CONTEXT_H

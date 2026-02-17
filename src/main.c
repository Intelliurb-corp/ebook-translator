#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "epub.h"
#include "context.h"
#include "context_strategy.h"
#include <getopt.h>

#define MAX_STRATEGIES 5

void print_usage(const char *progname) {
    printf("Usage: %s [options] <input.epub> [output.epub]\n", progname);
    printf("Options:\n");
    printf("  -c, --config <file>    Path to config.json (default: ./conf/config.json)\n");
    printf("  -l, --lang <code>      Target language code (overrides config)\n");
    printf("  -m, --model <name>     LLM model name (overrides config)\n");
    printf("  -C, --context <file>   Context file path (overrides config)\n");
    printf("  -h, --help             Show this help message\n");
}

// Helper to read file content for context analysis
char* read_file_content(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size <= 0) { fclose(fp); return NULL; }
    char *buf = malloc(size + 1);
    fread(buf, 1, size, fp);
    buf[size] = 0;
    fclose(fp);
    return buf;
}

int main(int argc, char *argv[]) {
    char *config_path = "./conf/config.json";
    char *target_lang = NULL;
    char *model_name = NULL;
    char *context_file_arg = NULL;
    char *input_file = NULL;
    char *output_file = NULL;

    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"lang",   required_argument, 0, 'l'},
        {"model",  required_argument, 0, 'm'},
        {"context",required_argument, 0, 'C'},
        {"help",   no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "c:l:m:C:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c': config_path = optarg; break;
            case 'l': target_lang = optarg; break;
            case 'm': model_name = optarg; break;
            case 'C': context_file_arg = optarg; break;
            case 'h': print_usage(argv[0]); return 0;
            default: print_usage(argv[0]); return 1;
        }
    }

    if (optind < argc) {
        input_file = argv[optind++];
    }
    if (optind < argc) {
        output_file = argv[optind++];
    }

#include <unistd.h>
#include <limits.h>

// ... (keep print_usage and start of main)

    if (!input_file) {
        fprintf(stderr, "Error: Input EPUB file is required.\n");
        print_usage(argv[0]);
        return 1;
    }

    if (access(input_file, F_OK) == -1) {
        fprintf(stderr, "Error: Input file '%s' does not exist.\n", input_file);
        return 1;
    }

    // 1. Try CLI argument (already handled by getopt)
    
    // 2. Try default local path
    if (access(config_path, F_OK) != 0) {
        // 3. Try system-wide path
        if (access("/usr/local/etc/ebook-translator/config.json", F_OK) == 0) {
            config_path = "/usr/local/etc/ebook-translator/config.json";
        }
    }

    config_t *config = load_config(config_path);
    if (!config) {
        fprintf(stderr, "Warning: Could not load config from '%s'. Using default values.\n", config_path);
        config = calloc(1, sizeof(config_t));
    }

    // Apply CLI overrides
    if (target_lang) {
        free(config->target_language);
        config->target_language = strdup(target_lang);
    }
    if (model_name) {
        free(config->model);
        config->model = strdup(model_name);
    }
    if (context_file_arg) {
        free(config->context_file);
        config->context_file = strdup(context_file_arg);
    }

    if (!config->target_language) config->target_language = strdup("en");
    if (!config->model) config->model = strdup("gpt-4o");

    printf("--- Session Configuration ---\n");
    printf("Input:      %s\n", input_file);
    printf("Output:     %s\n", output_file ? output_file : "translated.epub");
    printf("Provider:   %s\n", config->llm_provider ? config->llm_provider : "unknown");
    printf("Model:      %s\n", config->model);
    printf("Target:     %s\n", config->target_language);
    printf("----------------------------\n");

    const char *temp_dir = "build/temp_epub";
    if (extract_epub(input_file, temp_dir) != 0) {
        fprintf(stderr, "Failed to extract EPUB\n");
        free_config(config);
        return 1;
    }

    epub_metadata_t *meta = parse_epub_metadata(temp_dir);
    if (!meta) {
        fprintf(stderr, "Failed to parse EPUB metadata\n");
        free_config(config);
        return 1;
    }

    // Initialize Strategies
    ContextStrategy *strategies[MAX_STRATEGIES];
    int strategy_count = 0;

    // 1. History Strategy
    if (config->context_file) {
        ContextStrategy *hist = create_history_strategy();
        hist->state = hist->init(config);
        if (hist->state) {
            strategies[strategy_count++] = hist;
            printf("Strategy Enabled: %s\n", hist->name);
        } else {
            free(hist);
        }
    }

    // 2. Sliding Window Strategy
    if (config->sliding_window_size > 0) {
        ContextStrategy *win = create_sliding_window_strategy();
        win->state = win->init(config);
        if (win->state) {
            strategies[strategy_count++] = win;
            printf("Strategy Enabled: %s\n", win->name);
        } else {
            free(win);
        }
    }

    // Iterate spine and translate each XHTML file
    for (int i = 0; i < meta->spine_count; i++) {
        char *idref = meta->spine[i];
        for (int j = 0; j < meta->manifest_count; j++) {
            if (strcmp(meta->manifest[j].name, idref) == 0) {
                char xhtml_path[PATH_MAX];
                if (meta->base_dir && strlen(meta->base_dir) > 0) {
                    snprintf(xhtml_path, sizeof(xhtml_path), "%s/%s/%s", temp_dir, meta->base_dir, meta->manifest[j].href);
                } else {
                    snprintf(xhtml_path, sizeof(xhtml_path), "%s/%s", temp_dir, meta->manifest[j].href);
                }
                
                printf("Processing chapter %d/%d: %s...\n", i+1, meta->spine_count, idref);

                // Prepare context string from ALL strategies
                // We'll concatenate them
                char *combined_context = calloc(1, 1);
                for (int s = 0; s < strategy_count; s++) {
                    char *prompt_chunk = strategies[s]->get_prompt(strategies[s]->state, config);
                    if (prompt_chunk) {
                        size_t new_len = strlen(combined_context) + strlen(prompt_chunk) + 2;
                        combined_context = realloc(combined_context, new_len);
                        strcat(combined_context, prompt_chunk);
                        strcat(combined_context, "\n"); // Separator
                        free(prompt_chunk);
                    }
                }

                // Translate
                if (translate_xhtml(xhtml_path, config, combined_context) != 0) {
                    fprintf(stderr, "Failed to translate %s\n", xhtml_path);
                }
                free(combined_context);

                // Update Strategies
                // We read the translated file? Or original? 
                // Currently context.c uses original for summary, but sliding window might want translated...
                // The current implementation of translate_xhtml replaces content IN PLACE.
                // So reading xhtml_path NOW gives us TRANSLATED content (mostly).
                // Actually, translate_xhtml writes to the file.
                
                char *content = read_file_content(xhtml_path); 
                if (content) {
                    for (int s = 0; s < strategy_count; s++) {
                        strategies[s]->update(strategies[s]->state, content, config);
                    }
                    free(content);
                }
                break;
            }
        }
    }

    // Cleanup Strategies
    for (int s = 0; s < strategy_count; s++) {
        strategies[s]->cleanup(strategies[s]->state);
        free(strategies[s]);
    }

    const char *final_output = output_file ? output_file : "translated.epub";
    if (archive_epub(final_output, temp_dir) != 0) {
        fprintf(stderr, "Failed to create output EPUB\n");
    } else {
        printf("Success! Translated EPUB saved to %s\n", final_output);
    }

    free_epub_metadata(meta);
    free_config(config);
    return 0;
}

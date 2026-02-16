#ifndef EPUB_H
#define EPUB_H

#include "common.h"
#include <zip.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

typedef struct {
    char *name;
    char *href;
    char *media_type;
} epub_item_t;

typedef struct {
    char *title;
    char *author;
    char **spine; // Array of item IDs
    int spine_count;
    epub_item_t *manifest;
    int manifest_count;
    char *base_dir; // Directory of the OPF file relative to EPUB root
} epub_metadata_t;

int extract_epub(const char *path, const char *dest_dir);
int archive_epub(const char *dest_path, const char *src_dir);
epub_metadata_t* parse_epub_metadata(const char *root_dir);
void free_epub_metadata(epub_metadata_t *meta);

#endif // EPUB_H

#include "epub.h"
#include <dirent.h>

static int add_dir_to_zip(struct zip *z, const char *dir_path, const char *base_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) return -1;

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        
        // Skip mimetype as it's added manually with specific compression rules
        if (strcmp(entry->d_name, "mimetype") == 0)
            continue;

        char full_path[1024];
        char zip_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        if (base_path && strlen(base_path) > 0)
            snprintf(zip_path, sizeof(zip_path), "%s/%s", base_path, entry->d_name);
        else
            snprintf(zip_path, sizeof(zip_path), "%s", entry->d_name);

        if (entry->d_type == DT_DIR) {
            zip_dir_add(z, zip_path, ZIP_FL_ENC_UTF_8);
            add_dir_to_zip(z, full_path, zip_path);
        } else {
            struct zip_source *s = zip_source_file(z, full_path, 0, 0);
            if (s) {
                zip_file_add(z, zip_path, s, ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8);
            }
        }
    }
    closedir(dir);
    return 0;
}

int archive_epub(const char *dest_path, const char *src_dir) {
    int err = 0;
    struct zip *z = zip_open(dest_path, ZIP_CREATE | ZIP_TRUNCATE, &err);
    if (!z) return -1;

    // EPUB requirement: 'mimetype' must be first and uncompressed
    struct zip_source *s = zip_source_buffer(z, "application/epub+zip", 20, 0);
    zip_int64_t index = zip_file_add(z, "mimetype", s, ZIP_FL_OVERWRITE);
    if (index >= 0) {
        zip_set_file_compression(z, index, ZIP_CM_STORE, 0);
    }
    // Note: In real production code, we'd ensure compression is disabled for mimetype

    add_dir_to_zip(z, src_dir, "");

    zip_close(z);
    return 0;
}

#include "epub.h"
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

#include <limits.h>
#include <stdlib.h>

int extract_epub(const char *path, const char *dest_dir) {
    int err = 0;
    struct zip *z = zip_open(path, 0, &err);
    if (!z) {
        fprintf(stderr, "Error opening zip file %s: %d\n", path, err);
        return -1;
    }

    mkdir(dest_dir, 0755);
    
    char resolved_dest[PATH_MAX];
    if (realpath(dest_dir, resolved_dest) == NULL) {
        fprintf(stderr, "Error resolving destination directory %s\n", dest_dir);
        zip_close(z);
        return -1;
    }

    zip_int64_t num_entries = zip_get_num_entries(z, 0);
    for (zip_int64_t i = 0; i < num_entries; i++) {
        struct zip_stat st;
        zip_stat_index(z, i, 0, &st);

        // ZIP Slip protection: Ensure extraction path is within dest_dir
        // Since we can't realpath the target file (it doesn't exist), we verify the entry name doesn't contain ".."
        // For distinct security, one might create directories first and realpath them, 
        // but checking for ".." is a strong heuristic for simple extraction like this.
        if (strstr(st.name, "..")) {
             fprintf(stderr, "Warning: Skipping potentially unsafe zip entry: %s\n", st.name);
             continue;
        }

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", dest_dir, st.name);

        // Create directories if needed
        char *p = strchr(full_path, '/');
        while (p) {
            *p = '\0';
            // Only try to mkdir if it looks like a path inside dest_dir? 
            // The full_path is constructed from dest_dir + st.name.
            // Since st.name has no "..", it should be safe nested dirs.
            mkdir(full_path, 0755);
            *p = '/';
            p = strchr(p + 1, '/');
        }

        if (st.name[strlen(st.name)-1] == '/') {
            mkdir(full_path, 0755);
            continue;
        }

        struct zip_file *f = zip_fopen_index(z, i, 0);
        if (!f) continue;

        FILE *fp = fopen(full_path, "wb");
        if (!fp) {
            zip_fclose(f);
            continue;
        }

        char buf[8192];
        zip_int64_t n;
        while ((n = zip_fread(f, buf, sizeof(buf))) > 0) {
            fwrite(buf, 1, n, fp);
        }

        fclose(fp);
        zip_fclose(f);
    }

    zip_close(z);
    return 0;
}

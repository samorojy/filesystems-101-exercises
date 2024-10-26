#include <solution.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>

void abspath(const char *path)
{
    char resolved_path[PATH_MAX];
    errno = 0;
    if (realpath(path, resolved_path) == NULL) {
        char parent[PATH_MAX];
        char *slash = strrchr(path, '/');

        if (slash != NULL) {
            size_t parent_len = slash - path;
            strncpy(parent, path, parent_len);
            parent[parent_len] = '\0';
        } else {
            strcpy(parent, "/");
        }

        report_error(parent, path, errno);
        exit(EXIT_FAILURE);
    }

    errno = 0;
    struct stat file_stat;
    if (stat(resolved_path, &file_stat) == 0) {
        if (S_ISDIR(file_stat.st_mode)) {
            strcat(resolved_path, "/");
        }
    } else {
        char parent[PATH_MAX];
        char *slash = strrchr(resolved_path, '/');
        if (slash != NULL) {
            size_t parent_len = slash - resolved_path;
            strncpy(parent, resolved_path, parent_len);
            parent[parent_len] = '\0';
        } else {
            strcpy(parent, "/");
        }

        report_error(parent, resolved_path, errno);
        exit(EXIT_FAILURE);
    }
    report_path(resolved_path);
}

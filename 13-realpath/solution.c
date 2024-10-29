#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <solution.h>

bool resolve(char *result, size_t *result_len, const char *path);

bool handle_symlink(char *result, size_t *result_len, const char *link_path) {
    char link_buf[PATH_MAX];
    ssize_t len = readlink(link_path, link_buf, sizeof(link_buf) - 1);
    if (len == -1) {
        report_error(result, link_path, errno);
        return false;
    }
    link_buf[len] = '\0';

    if (link_buf[0] == '/') {
        memcpy(result, "/", 2);
        *result_len = 1;
    }
    return resolve(result, result_len, link_buf);
}

bool process_segment(char *result, size_t *result_len, const char *seg_start, size_t seg_len) {
    if (seg_len == 0 || (seg_len == 1 && seg_start[0] == '.')) {
        return true;
    }
    if (seg_len == 2 && seg_start[0] == '.' && seg_start[1] == '.') {
        while (*result_len > 1 && result[*result_len - 1] == '/') {
            (*result_len)--;
        }
        while (*result_len > 1 && result[*result_len - 1] != '/') {
            (*result_len)--;
        }
        result[*result_len] = '\0';
        return true;
    }

    size_t temp_len = *result_len;
    if (temp_len > 1) {
        result[temp_len++] = '/';
    }
    memcpy(result + temp_len, seg_start, seg_len);
    temp_len += seg_len;
    result[temp_len] = '\0';

    struct stat st;
    if (lstat(result, &st) == 0) {
        if (S_ISLNK(st.st_mode)) {
            return handle_symlink(result, result_len, result);
        }
    } else if (errno == ENOENT) {
        result[*result_len] = '\0';
        report_error(result, seg_start, ENOENT);
        return false;
    }

    *result_len = temp_len;
    return true;
}

bool resolve(char *result, size_t *result_len, const char *path) {
    const char *p = path;
    if (*p == '/') {
        memcpy(result, "/", 2);
        *result_len = 1;
        p++;
    } else {
        *result_len = strlen(result);
    }

    while (*p) {
        while (*p == '/') {
            p++;
        }
        const char *seg_start = p;
        size_t seg_len = 0;
        while (*p && *p != '/') {
            seg_len++;
            p++;
        }
        if (!process_segment(result, result_len, seg_start, seg_len)) {
            return false;
        }
    }
    return true;
}

void abspath(const char *path) {
    char resolved_path[PATH_MAX];
    size_t result_len = 0;
    errno = 0;
    if (resolve(resolved_path, &result_len, path)) {
        struct stat st;
        if (stat(resolved_path, &st) == 0 && S_ISDIR(st.st_mode) && resolved_path[result_len - 1] != '/') {
            resolved_path[result_len] = '/';
            resolved_path[++result_len] = '\0';
        }
        report_path(resolved_path);
    }
}
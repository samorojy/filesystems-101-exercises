#include <solution.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

int custom_realpath(const char* path, char* resolved_path, char* error_path)
{
    char temp_path[PATH_MAX];
    size_t path_len = strlen(path);
    if (path_len >= PATH_MAX)
    {
        errno = ENAMETOOLONG;
        if (error_path) strncpy(error_path, path, PATH_MAX - 1);
        return -1;
    }
    memcpy(temp_path, path, path_len);
    temp_path[path_len] = '\0';

    struct stat path_stat;
    char link_target[PATH_MAX];
    char* buffer;
    resolved_path[0] = '\0';

    if (temp_path[0] == '/')
    {
        strcpy(resolved_path, "/");
    }

    char* token = strtok(temp_path, "/");
    while (token != NULL)
    {
        if (strcmp(token, ".") == 0)
        {
            // Skip current directory token
        }
        else if (strcmp(token, "..") == 0)
        {
            if (strcmp(resolved_path, "/") != 0)
            {
                buffer = strrchr(resolved_path, '/');
                if (buffer != NULL && buffer != resolved_path)
                {
                    *buffer = '\0';
                }
                else
                {
                    strcpy(resolved_path, "/");
                }
            }
        }
        else
        {
            if (strcmp(resolved_path, "/") != 0)
            {
                size_t len = strlen(resolved_path);
                if (len + 1 < PATH_MAX)
                {
                    resolved_path[len] = '/';
                    resolved_path[len + 1] = '\0';
                }
            }
            size_t token_len = strlen(token);
            size_t res_len = strlen(resolved_path);
            if (res_len + token_len < PATH_MAX)
            {
                memcpy(resolved_path + res_len, token, token_len);
                resolved_path[res_len + token_len] = '\0';
            }
            else
            {
                errno = ENAMETOOLONG;
                if (error_path) strncpy(error_path, resolved_path, PATH_MAX - 1);
                return -1;
            }

            if (lstat(resolved_path, &path_stat) != 0)
            {
                if (error_path) strncpy(error_path, resolved_path, PATH_MAX - 1);
                return -1;
            }

            if (S_ISLNK(path_stat.st_mode))
            {
                ssize_t len = readlink(resolved_path, link_target, sizeof(link_target) - 1);
                if (len == -1)
                {
                    if (error_path) strncpy(error_path, resolved_path, PATH_MAX - 1);
                    return -1;
                }

                link_target[len] = '\0';

                if (link_target[0] == '/')
                {
                    size_t link_len = strlen(link_target);
                    if (link_len < PATH_MAX)
                    {
                        memcpy(resolved_path, link_target, link_len);
                        resolved_path[link_len] = '\0';
                    }
                    else
                    {
                        errno = ENAMETOOLONG;
                        if (error_path) strncpy(error_path, link_target, PATH_MAX - 1);
                        return -1;
                    }
                }
                else
                {
                    buffer = strrchr(resolved_path, '/');
                    if (buffer != NULL)
                    {
                        *buffer = '\0';
                    }
                    size_t res_len = strlen(resolved_path);
                    if (res_len + 1 < PATH_MAX)
                    {
                        resolved_path[res_len] = '/';
                        resolved_path[res_len + 1] = '\0';
                        res_len++;
                    }
                    size_t link_len = strlen(link_target);
                    if (res_len + link_len < PATH_MAX)
                    {
                        memcpy(resolved_path + res_len, link_target, link_len);
                        resolved_path[res_len + link_len] = '\0';
                    }
                    else
                    {
                        errno = ENAMETOOLONG;
                        if (error_path) strncpy(error_path, resolved_path, PATH_MAX - 1);
                        return -1;
                    }
                }

                res_len = strlen(resolved_path);
                memcpy(temp_path, resolved_path, res_len);
                temp_path[res_len] = '\0';
                token = strtok(temp_path, "/");
                resolved_path[0] = '\0';
                if (temp_path[0] == '/')
                {
                    strcpy(resolved_path, "/");
                }
                continue;
            }
        }
        token = strtok(NULL, "/");
    }

    return 0;
}

void abspath(const char* path)
{
    char resolved_path[PATH_MAX];
    char error_path[PATH_MAX];
    errno = 0;
    if (custom_realpath(path, resolved_path, error_path) != 0)
    {
        char parent[PATH_MAX];
        char* slash = strrchr(error_path, '/');

        if (slash != NULL)
        {
            size_t parent_len = slash - error_path;
            if (parent_len < PATH_MAX)
            {
                memcpy(parent, error_path, parent_len);
                parent[parent_len] = '\0';
            }
            else
            {
                strncpy(parent, error_path, PATH_MAX - 1);
                parent[PATH_MAX - 1] = '\0';
            }
        }
        else
        {
            strncpy(parent, error_path, PATH_MAX - 1);
            parent[PATH_MAX - 1] = '\0';
        }

        report_error(parent, error_path, errno);
        return;
    }

    errno = 0;
    struct stat file_stat;
    if (stat(resolved_path, &file_stat) == 0)
    {
        if (S_ISDIR(file_stat.st_mode))
        {
            size_t len = strlen(resolved_path);
            if (resolved_path[len - 1] != '/')
            {
                if (len + 1 < PATH_MAX)
                {
                    resolved_path[len] = '/';
                    resolved_path[len + 1] = '\0';
                }
            }
        }
    }
    else
    {
        char parent[PATH_MAX];
        char* slash = strrchr(resolved_path, '/');
        if (slash != NULL)
        {
            size_t parent_len = slash - resolved_path;
            if (parent_len < PATH_MAX)
            {
                memcpy(parent, resolved_path, parent_len);
                parent[parent_len] = '\0';
            }
            else
            {
                strncpy(parent, resolved_path, PATH_MAX - 1);
                parent[PATH_MAX - 1] = '\0';
            }
        }
        else
        {
            strncpy(parent, resolved_path, PATH_MAX - 1);
            parent[PATH_MAX - 1] = '\0';
        }

        report_error(parent, resolved_path, errno);
        return;
    }
    report_path(resolved_path);
}

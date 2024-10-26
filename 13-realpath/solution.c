#include <solution.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

int custom_realpath(const char* path, char* resolved_path)
{
    char temp_path[PATH_MAX];
    strncpy(temp_path, path, PATH_MAX);

    struct stat path_stat;
    char link_target[PATH_MAX];
    char* token;
    char* buffer;
    resolved_path[0] = '\0';

    if (temp_path[0] == '/')
    {
        strcpy(resolved_path, "/");
    }

    token = strtok(temp_path, "/");
    while (token != NULL)
    {
        if (strcmp(token, ".") == 0)
        {
            // chill
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
                strncat(resolved_path, "/", PATH_MAX - strlen(resolved_path) - 1);
            }
            strncat(resolved_path, token, PATH_MAX - strlen(resolved_path) - 1);
            if (lstat(resolved_path, &path_stat) != 0)
            {
                return -1;
            }

            if (S_ISLNK(path_stat.st_mode))
            {
                ssize_t len = readlink(resolved_path, link_target, sizeof(link_target) - 1);
                if (len == -1)
                {
                    return -1;
                }

                link_target[len] = '\0';

                if (link_target[0] == '/')
                {
                    strncpy(resolved_path, link_target, PATH_MAX);
                }
                else
                {
                    buffer = strrchr(resolved_path, '/');
                    if (buffer != NULL)
                    {
                        *buffer = '\0';
                    }
                    strncat(resolved_path, "/", PATH_MAX - strlen(resolved_path) - 1);
                    strncat(resolved_path, link_target, PATH_MAX - strlen(resolved_path) - 1);
                }
            }
        }
        token = strtok(NULL, "/");
    }

    return 0;
}

void abspath(const char* path)
{
    char resolved_path[PATH_MAX];
    errno = 0;
    if (realpath(path, resolved_path) != 0)
    {
        char parent[PATH_MAX];
        char* slash = strrchr(path, '/');

        if (slash != NULL)
        {
            size_t parent_len = slash - path;
            strncpy(parent, path, parent_len);
            parent[parent_len] = '\0';
        }
        else
        {
            strcpy(parent, "/");
        }

        report_error(parent, path, errno);
        exit(EXIT_FAILURE);
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
                strncat(resolved_path, "/", PATH_MAX - len - 1);
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
            strncpy(parent, resolved_path, parent_len);
            parent[parent_len] = '\0';
        }
        else
        {
            strcpy(parent, "/");
        }

        report_error(parent, resolved_path, errno);
        exit(EXIT_FAILURE);
    }
    report_path(resolved_path);
}

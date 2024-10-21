#include <solution.h>

#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

void lsof(void) {
    struct dirent *entry;
    DIR *proc_dir = opendir("/proc");

    if (!proc_dir) {
        report_error("/proc", errno);
        return;
    }

    while ((entry = readdir(proc_dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            const char* pid = entry->d_name;
            char fd_dir_path[PATH_MAX];
            snprintf(fd_dir_path, sizeof(fd_dir_path), "/proc/%s/fd", pid);
            DIR *fd_dir = opendir(fd_dir_path);

            if (!fd_dir) {
                report_error(fd_dir_path, errno);
                continue;
            }

            struct dirent *fd_entry;
            while ((fd_entry = readdir(fd_dir)) != NULL) {
                if (fd_entry->d_type == DT_LNK) {
                    char link_path[PATH_MAX];
                    char file_path[PATH_MAX];

                    snprintf(link_path, sizeof(link_path), "/proc/%s/fd/%s", pid, fd_entry->d_name);
                    ssize_t len = readlink(link_path, file_path, sizeof(file_path) - 1);

                    if (len != -1) {
                        file_path[len] = '\0';
                        report_file(file_path);
                    } else {
                        report_error(link_path, errno);
                    }
                }
            }

            closedir(fd_dir);
        }
    }

    closedir(proc_dir);
}
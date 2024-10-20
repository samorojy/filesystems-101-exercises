#include <solution.h>

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>

void ps(void) {
    DIR *proc_dir = opendir("/proc");
    if (!proc_dir) {
        report_error("/proc", errno);
        return;
    }

    struct dirent *entry;

    while ((entry = readdir(proc_dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            int pid = atoi(entry->d_name);
            if (pid > 0) {
                char path[256];
                snprintf(path, sizeof(path), "/proc/%d/exe", pid);

                char exe_path[256];
                ssize_t len = readlink(path, exe_path, sizeof(exe_path) - 1);
                if (len == -1) {
                    report_error(path, errno);
                    continue;
                }
                exe_path[len] = '\0';

                snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
                FILE *cmdline_file = fopen(path, "r");
                if (!cmdline_file) {
                    report_error(path, errno);
                    continue;
                }

                char *argv[256] = {0};
                size_t argc = 0;
                char buffer[4096];
                size_t total_read = fread(buffer, 1, sizeof(buffer), cmdline_file);
                fclose(cmdline_file);

                if (total_read > 0) {
                    size_t i, j = 0;
                    for (i = 0; i < total_read; i++) {
                        if (buffer[i] == '\0') {
                            argv[argc++] = &buffer[j];
                            j = i + 1;
                        }
                    }
                } else {
                    argv[0] = exe_path;
                    argc = 1;
                }

                snprintf(path, sizeof(path), "/proc/%d/environ", pid);
                FILE *env_file = fopen(path, "r");
                if (!env_file) {
                    report_error(path, errno);
                    continue;
                }

                char *envp[256] = {0};
                size_t envc = 0;
                total_read = fread(buffer, 1, sizeof(buffer), env_file);
                fclose(env_file);

                if (total_read > 0) {
                    size_t i, j = 0;
                    for (i = 0; i < total_read; i++) {
                        if (buffer[i] == '\0') {
                            envp[envc++] = &buffer[j];
                            j = i + 1;
                        }
                    }
                }

                report_process(pid, exe_path, argv, envp);
            }
        }
    }
    closedir(proc_dir);
}
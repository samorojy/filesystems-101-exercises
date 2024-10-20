#include <solution.h>

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

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

                char cmdline_buffer[1024*1024];
                size_t total_read = fread(cmdline_buffer, 1, sizeof(cmdline_buffer), cmdline_file);
                fclose(cmdline_file);

                size_t argc = 0;
                for (size_t i = 0; i < total_read; i++) {
                    if (cmdline_buffer[i] == '\0') {
                        argc++;
                    }
                }

                char **argv = (char **)malloc((argc + 1) * sizeof(char *));
                size_t j = 0;
                argc = 0;
                for (size_t i = 0; i < total_read; i++) {
                    if (cmdline_buffer[i] == '\0') {
                        argv[argc++] = strdup(&cmdline_buffer[j]);
                        j = i + 1;
                    }
                }
                argv[argc] = NULL;

                snprintf(path, sizeof(path), "/proc/%d/environ", pid);
                FILE *env_file = fopen(path, "r");
                if (!env_file) {
                    report_error(path, errno);
                    for (size_t i = 0; i < argc; i++) {
                        free(argv[i]);
                    }
                    free(argv);
                    continue;
                }

                char env_buffer[1024*1024];
                total_read = fread(env_buffer, 1, sizeof(env_buffer), env_file);
                fclose(env_file);

                size_t envc = 0;
                for (size_t i = 0; i < total_read; i++) {
                    if (env_buffer[i] == '\0') {
                        envc++;
                    }
                }

                char **envp = (char **)malloc((envc + 1) * sizeof(char *));
                j = 0;
                envc = 0;
                for (size_t i = 0; i < total_read; i++) {
                    if (env_buffer[i] == '\0') {
                        envp[envc++] = strdup(&env_buffer[j]);
                        j = i + 1;
                    }
                }
                envp[envc] = NULL;

                report_process(pid, exe_path, argv, envp);

                for (size_t i = 0; i < argc; i++) {
                    free(argv[i]);
                }
                free(argv);
                for (size_t i = 0; i < envc; i++) {
                    free(envp[i]);
                }
                free(envp);
            }
        }
    }
    closedir(proc_dir);
}
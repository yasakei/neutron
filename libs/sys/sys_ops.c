#include "sys_ops.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

// File operations
bool sys_cp_c(const char* src, const char* dest) {
    FILE* source = fopen(src, "rb");
    if (!source) return false;
    FILE* destination = fopen(dest, "wb");
    if (!destination) {
        fclose(source);
        return false;
    }

    char buffer[4096];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        if (fwrite(buffer, 1, n, destination) != n) {
            fclose(source);
            fclose(destination);
            return false;
        }
    }

    fclose(source);
    fclose(destination);
    return true;
}

bool sys_mv_c(const char* src, const char* dest) {
    return rename(src, dest) == 0;
}

bool sys_rm_c(const char* path) {
    return remove(path) == 0;
}

bool sys_mkdir_c(const char* path) {
    return mkdir(path, 0777) == 0;
}

bool sys_rmdir_c(const char* path) {
    return rmdir(path) == 0;
}

bool sys_exists_c(const char* path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

char* sys_read_c(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = (char*)malloc(length + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    if (fread(buffer, 1, length, file) != length) {
        fclose(file);
        free(buffer);
        return NULL;
    }

    buffer[length] = '\0';
    fclose(file);
    return buffer;
}

bool sys_write_c(const char* path, const char* content) {
    FILE* file = fopen(path, "wb");
    if (!file) return false;

    if (fwrite(content, 1, strlen(content), file) != strlen(content)) {
        fclose(file);
        return false;
    }

    fclose(file);
    return true;
}

bool sys_append_c(const char* path, const char* content) {
    FILE* file = fopen(path, "ab");
    if (!file) return false;

    if (fwrite(content, 1, strlen(content), file) != strlen(content)) {
        fclose(file);
        return false;
    }

    fclose(file);
    return true;
}

// System info
char* sys_cwd_c() {
    char* buffer = (char*)malloc(1024);
    if (getcwd(buffer, 1024) != NULL) {
        return buffer;
    }
    free(buffer);
    return NULL;
}

bool sys_chdir_c(const char* path) {
    return chdir(path) == 0;
}

char* sys_env_c(const char* name) {
    return getenv(name);
}

char* sys_info_c() {
    // For now, just return a dummy string
    return strdup("{\"platform\": \"linux\", \"arch\": \"x86_64\"}");
}

char* sys_input_c(const char* prompt) {
    printf("%s", prompt);
    char* line = NULL;
    size_t len = 0;
    if (getline(&line, &len, stdin) == -1) {
        return NULL;
    }
    return line;
}

// Process control
void sys_exit_c(int code) {
    exit(code);
}

struct ExecResult* sys_exec_c(const char* command) {
    FILE* pipe = popen(command, "r");
    if (!pipe) return NULL;

    char* output = (char*)malloc(1);
    output[0] = '\0';
    size_t size = 1;

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        size_t len = strlen(buffer);
        char* new_output = (char*)realloc(output, size + len);
        if (!new_output) {
            free(output);
            pclose(pipe);
            return NULL;
        }
        output = new_output;
        strcat(output, buffer);
        size += len;
    }

    int status = pclose(pipe);

    struct ExecResult* result = (struct ExecResult*)malloc(sizeof(struct ExecResult));
    result->output = output;
    result->exit_code = WEXITSTATUS(status);

    return result;
}

void free_exec_result(struct ExecResult* result) {
    if (result) {
        free(result->output);
        free(result);
    }
}

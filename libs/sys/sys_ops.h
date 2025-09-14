#ifndef NEUTRON_LIBS_SYS_SYS_OPS_H
#define NEUTRON_LIBS_SYS_SYS_OPS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// File operations
bool sys_cp_c(const char* src, const char* dest);
bool sys_mv_c(const char* src, const char* dest);
bool sys_rm_c(const char* path);
bool sys_mkdir_c(const char* path);
bool sys_rmdir_c(const char* path);
bool sys_exists_c(const char* path);
char* sys_read_c(const char* path);
bool sys_write_c(const char* path, const char* content);
bool sys_append_c(const char* path, const char* content);

// System info
char* sys_cwd_c();
bool sys_chdir_c(const char* path);
char* sys_env_c(const char* name);
// sys_args_c is not implemented as it requires access to argc and argv
char* sys_info_c();
char* sys_input_c(const char* prompt);

// Process control
void sys_exit_c(int code);
struct ExecResult {
    char* output;
    int exit_code;
};
struct ExecResult* sys_exec_c(const char* command);
void free_exec_result(struct ExecResult* result);

#ifdef __cplusplus
}
#endif

#endif // NEUTRON_LIBS_SYS_SYS_OPS_H

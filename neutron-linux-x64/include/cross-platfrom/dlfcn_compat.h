// Minimal dlopen/dlsym/dlclose compatibility header for Windows
#pragma once

#ifdef _WIN32
#include <windows.h>
extern "C" {
    typedef void* (*dlopen_t)(const char* filename, int flags);
}

// Provide prototypes matching POSIX dlfcn.h
// RTLD flags (values are placeholders; the shim ignores them)
#ifndef RTLD_LAZY
#define RTLD_LAZY 1
#endif
#ifndef RTLD_NOW
#define RTLD_NOW 2
#endif
#ifndef RTLD_GLOBAL
#define RTLD_GLOBAL 4
#endif

extern "C" {
    void* dlopen(const char* filename, int flags);
    void* dlsym(void* handle, const char* symbol);
    int dlclose(void* handle);
    char* dlerror();
}

#else
#include <dlfcn.h>
#endif

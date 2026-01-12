// Minimal implementation of dlopen/dlsym/dlclose for Windows

// Windows macro undefs - must be before any includes that might include Windows headers
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    // Undefine Windows macros that conflict with C++ code
    // NOTE: Do NOT undefine FAR, NEAR, IN, OUT as they are needed by Windows headers
    #undef TRUE
    #undef FALSE
    #undef DELETE
    #undef ERROR
    #undef OPTIONAL
    #undef interface
    #undef small
    #undef max
    #undef min
#endif

#include "../include/cross-platfrom/dlfcn_compat.h"
#include <string>
#include <mutex>

static thread_local std::string last_error;

void* dlopen(const char* filename, int /*flags*/) {
    HMODULE h = LoadLibraryA(filename);
    if (!h) {
        DWORD err = GetLastError();
        last_error = "LoadLibrary failed: " + std::to_string(err);
    } else {
        last_error.clear();
    }
    return (void*)h;
}

void* dlsym(void* handle, const char* symbol) {
    if (!handle) return nullptr;
    FARPROC proc = GetProcAddress((HMODULE)handle, symbol);
    if (!proc) {
        DWORD err = GetLastError();
        last_error = "GetProcAddress failed: " + std::to_string(err);
    } else {
        last_error.clear();
    }
    return (void*)proc;
}

int dlclose(void* handle) {
    if (!handle) return -1;
    BOOL res = FreeLibrary((HMODULE)handle);
    if (!res) {
        DWORD err = GetLastError();
        last_error = "FreeLibrary failed: " + std::to_string(err);
        return -1;
    }
    last_error.clear();
    return 0;
}

char* dlerror() {
    if (last_error.empty()) return nullptr;
    // return C string; caller shouldn't free it
    return const_cast<char*>(last_error.c_str());
}

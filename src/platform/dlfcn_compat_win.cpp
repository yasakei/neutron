/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 * 
 * This software is distributed under the Neutron Permissive License (NPL) 1.1.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, for both open source and commercial purposes.
 * 
 * Conditions:
 * 
 * 1. The above copyright notice and this permission notice shall be included
 *    in all copies or substantial portions of the Software.
 * 
 * 2. Attribution is appreciated but NOT required.
 *    Suggested (optional) credit:
 *    "Built using Neutron Programming Language (c) yasakei"
 * 
 * 3. The name "Neutron" and the name of the copyright holder may not be used
 *    to endorse or promote products derived from this Software without prior
 *    written permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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

cmake_minimum_required(VERSION 3.15)
project(Neutron VERSION 1.1.2 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform detection
if(WIN32)
    add_definitions(-DNEUTRON_WINDOWS)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS) # Common MSVC warning suppression
elseif(APPLE)
    add_definitions(-DNEUTRON_MACOS)
elseif(UNIX)
    add_definitions(-DNEUTRON_LINUX)
endif()

# =============================================================================
# Build Options
# =============================================================================
# NEUTRON_UNSAFE_OPTIMIZATIONS: Enable aggressive performance optimizations that
# may reduce security. Currently this disables MSVC stack buffer security checks
# (/GS-). Only enable for benchmarking or performance-critical builds where the
# security trade-off is acceptable. Default: OFF
option(NEUTRON_UNSAFE_OPTIMIZATIONS "Enable unsafe performance optimizations (disables /GS- security checks)" OFF)

# Compiler options
if(MSVC)
    # Set multithreaded runtime library (required for _beginthreadex and std::thread)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    # Optimization flags (avoid /O2 and /Ox to prevent /Ob2 vs /Ob3 conflict)
    # Use individual optimization flags instead of /O2 or /Ox
    # /Oi: Enable intrinsic functions
    # /Ot: Favor fast code over small code
    # /fp:fast: Fast floating-point model
    # /GL: Whole program optimization (works with LTO)
    # /Gy: Enable function-level linking
    # /permissive-: Disable non-conforming code (helps with standard library)
    # Note: /Og is deprecated in newer MSVC versions, removed
    # Suppress common warnings: C4267 (size_t conversion), C4244 (type conversion), C4100 (unreferenced parameter), C4458 (declaration hides member), C4273 (inconsistent dll linkage), C4101 (unreferenced local variable)6)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" && cd C:\Users\yasakei\.temp\test && neutron build
    add_compile_options(/W4 /utf-8 /EHsc /Oi /Ot /fp:fast /GL /Gy /permissive- /wd4267 /wd4244 /wd4100 /wd4458 /wd4273 /wd4101)
    
    # Ensure threading support is enabled
    add_compile_definitions(_WIN32_WINNT=0x0601)  # Windows 7 or later for threading
    
    # /GS-: Disable stack buffer security checks (small speedup, security trade-off)
    # Only enabled when NEUTRON_UNSAFE_OPTIMIZATIONS is ON
    if(NEUTRON_UNSAFE_OPTIMIZATIONS)
        message(STATUS "NEUTRON_UNSAFE_OPTIMIZATIONS enabled: Adding /GS- (disabling stack security checks)")
        add_compile_options(/GS-)
    endif()
    # Check if MSVC supports /Ob3 (VS2019 16.3+) and avoid conflicts
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag(/Ob3 HAS_OB3)
    if(HAS_OB3)
        # Remove any existing /Ob flags and add /Ob3
        string(REGEX REPLACE "/Ob[0-9]" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
        add_compile_options($<$<CONFIG:Release>:/Ob3>)
    else()
        # Remove any existing /Ob flags and add /Ob2
        string(REGEX REPLACE "/Ob[0-9]" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
        add_compile_options($<$<CONFIG:Release>:/Ob2>)
    endif()
    # Linker optimizations
    add_link_options(/LTCG /OPT:REF /OPT:ICF)
else()
    add_compile_options(-Wall -Wextra -O3 -flto)
    # Only use -march=native for single-architecture builds (not universal binaries)
    if(NOT CMAKE_OSX_ARCHITECTURES OR CMAKE_OSX_ARCHITECTURES STREQUAL "")
        add_compile_options(-march=native)
    endif()
endif()

# Enable Interprocedural Optimization (LTO/GL)
include(CheckIPOSupported)
check_ipo_supported(RESULT result OUTPUT output)
if(result)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(WARNING "IPO is not supported: ${output}")
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/core
    ${CMAKE_SOURCE_DIR}/libs
)

# Dependencies
include(FetchContent)

# Threading support (required for process system)
find_package(Threads REQUIRED)

# JSONCPP
find_package(jsoncpp CONFIG QUIET)
if(NOT jsoncpp_FOUND)
    # Try to find jsoncpp using pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(JSONCPP jsoncpp IMPORTED_TARGET)
    if(NOT JSONCPP_FOUND)
        message(FATAL_ERROR "Could not find jsoncpp library via CMake or pkg-config")
    endif()
    # Use the imported target
    set(JSONCPP_LIBRARIES PkgConfig::JSONCPP)
else()
    set(JSONCPP_LIBRARIES JsonCpp::JsonCpp)
endif()

# CURL (optional - HTTP module will be disabled if not found)
find_package(CURL QUIET)
if(NOT CURL_FOUND)
    # Try CONFIG mode (some vcpkg packages prefer this)
    find_package(CURL CONFIG QUIET)
endif()
if(NOT CURL_FOUND)
    # Use pkg-config on Unix systems
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(CURL libcurl IMPORTED_TARGET)
    endif()
endif()

# Debug output
message(STATUS "CURL_FOUND: ${CURL_FOUND}")
message(STATUS "CURL_INCLUDE_DIRS: ${CURL_INCLUDE_DIRS}")
message(STATUS "CURL_LIBRARIES: ${CURL_LIBRARIES}")
if(TARGET CURL::libcurl)
    message(STATUS "CURL::libcurl target available")
    set(HAVE_CURL TRUE)
endif()
if(TARGET PkgConfig::CURL)
    message(STATUS "PkgConfig::CURL target available")
    set(HAVE_CURL TRUE)
endif()
if(CURL_FOUND)
    set(HAVE_CURL TRUE)
endif()

if(HAVE_CURL)
    message(STATUS "HTTP module enabled")
    # Ensure CURL include directories are available
    if(CURL_INCLUDE_DIRS)
        include_directories(${CURL_INCLUDE_DIRS})
        message(STATUS "Added CURL include directories: ${CURL_INCLUDE_DIRS}")
    endif()
else()
    message(STATUS "CURL not found - HTTP module will be disabled")
endif()

# Function to add a new module easily
function(add_neutron_module MODULE_NAME)
    set(MODULE_PATH "libs/${MODULE_NAME}/native.cpp")
    if(EXISTS ${MODULE_PATH})
        list(APPEND NEUTRON_ALL_MODULE_SOURCES ${MODULE_PATH})
        set(NEUTRON_ALL_MODULE_SOURCES ${NEUTRON_ALL_MODULE_SOURCES} PARENT_SCOPE)
        message(STATUS "Added module: ${MODULE_NAME}")
    else()
        message(WARNING "Module ${MODULE_NAME} not found at ${MODULE_PATH}")
    endif()
endfunction()

# Function to add a new core component
function(add_neutron_component COMPONENT_NAME COMPONENT_PATH)
    if(EXISTS ${CMAKE_SOURCE_DIR}/${COMPONENT_PATH})
        set(NEUTRON_CORE_SOURCES ${NEUTRON_CORE_SOURCES} ${CMAKE_SOURCE_DIR}/${COMPONENT_PATH} PARENT_SCOPE)
        message(STATUS "Added component: ${COMPONENT_NAME} at ${COMPONENT_PATH}")
    else()
        message(WARNING "Component ${COMPONENT_NAME} not found at ${CMAKE_SOURCE_DIR}/${COMPONENT_PATH}")
    endif()
endfunction()

# Built-in module source files
set(NEUTRON_BUILTIN_MODULE_SOURCES
    libs/sys/native.cpp
    libs/fmt/native.cpp
    libs/json/native.cpp
    libs/math/native.cpp
    libs/http/native.cpp
    libs/time/native.cpp
    libs/arrays/native.cpp
    libs/async/native.cpp
    libs/regex/native.cpp
    libs/process/native.cpp
    libs/crypto/native.cpp
    libs/path/native.cpp
)

# All module-related sources (internal and built-in)
set(NEUTRON_ALL_MODULE_SOURCES
    ${NEUTRON_BUILTIN_MODULE_SOURCES}
)

# Compiler source files
set(NEUTRON_COMPILER_SOURCES
    src/compiler/scanner.cpp
    src/compiler/parser.cpp
    src/compiler/compiler.cpp
    src/compiler/bytecode.cpp
)

# Type system source files
set(NEUTRON_TYPES_SOURCES
    src/types/value.cpp
    src/types/array.cpp
    src/types/buffer.cpp
    src/types/json_object.cpp
    src/types/json_array.cpp
    src/types/return.cpp
    src/types/version.cpp
    src/types/obj_string.cpp
    src/types/string_method_registry.cpp
    src/types/unicode_handler.cpp
    src/types/string_formatter.cpp
    src/types/string_error.cpp
    src/types/string_search_methods.cpp
)

# Runtime source files
set(NEUTRON_RUNTIME_SOURCES
    src/runtime/environment.cpp
    src/runtime/error_handler.cpp
    src/runtime/runtime.cpp
    src/runtime/debug.cpp
    src/runtime/process.cpp
)

# Module system source files
set(NEUTRON_MODULE_SOURCES
    src/modules/module.cpp
    src/modules/module_loader.cpp
    src/modules/module_registry.cpp
    src/modules/module_utils.cpp
)

# Project system source files
set(NEUTRON_PROJECT_SOURCES
    src/project/project_config.cpp
    src/project/project_manager.cpp
    src/project/project_builder.cpp
)

# Platform abstraction source files
set(NEUTRON_PLATFORM_SOURCES_LIST
    src/platform/platform.cpp
)

# Core system source files (excluding main.cpp which is only for the executable)
set(NEUTRON_CORE_SOURCES
    src/core/vm.cpp
    src/core/checkpoint.cpp
    src/core/capi.cpp
    src/core/formatter.cpp
    src/token.cpp
    ${NEUTRON_COMPILER_SOURCES}
    ${NEUTRON_TYPES_SOURCES}
    ${NEUTRON_RUNTIME_SOURCES}
    ${NEUTRON_MODULE_SOURCES}
    ${NEUTRON_PROJECT_SOURCES}
    ${NEUTRON_PLATFORM_SOURCES_LIST}
)

# Executable source files (only main.cpp for the executable)
set(NEUTRON_EXECUTABLE_SOURCES
    src/main.cpp
)

# Additional components that can be added by features/extensions
set(NEUTRON_FEATURE_SOURCES "")
set(NEUTRON_EXTENSION_SOURCES "")
set(NEUTRON_UTILS_SOURCES "")

# Function to add feature sources
function(add_neutron_feature FEATURE_NAME FEATURE_PATH)
    if(EXISTS ${CMAKE_SOURCE_DIR}/${FEATURE_PATH})
        list(APPEND NEUTRON_FEATURE_SOURCES ${CMAKE_SOURCE_DIR}/${FEATURE_PATH})
        set(NEUTRON_FEATURE_SOURCES ${NEUTRON_FEATURE_SOURCES} PARENT_SCOPE)
        message(STATUS "Added feature: ${FEATURE_NAME}")
    else()
        message(WARNING "Feature ${FEATURE_NAME} not found at ${CMAKE_SOURCE_DIR}/${FEATURE_PATH}")
    endif()
endfunction()

# Function to add extension sources
function(add_neutron_extension EXTENSION_NAME EXTENSION_PATH)
    if(EXISTS ${CMAKE_SOURCE_DIR}/${EXTENSION_PATH})
        list(APPEND NEUTRON_EXTENSION_SOURCES ${CMAKE_SOURCE_DIR}/${EXTENSION_PATH})
        set(NEUTRON_EXTENSION_SOURCES ${NEUTRON_EXTENSION_SOURCES} PARENT_SCOPE)
        message(STATUS "Added extension: ${EXTENSION_NAME}")
    else()
        message(WARNING "Extension ${EXTENSION_NAME} not found at ${CMAKE_SOURCE_DIR}/${EXTENSION_PATH}")
    endif()
endfunction()

# Function to add utility sources
function(add_neutron_utility UTILITY_NAME UTILITY_PATH)
    if(EXISTS ${CMAKE_SOURCE_DIR}/${UTILITY_PATH})
        list(APPEND NEUTRON_UTILS_SOURCES ${CMAKE_SOURCE_DIR}/${UTILITY_PATH})
        set(NEUTRON_UTILS_SOURCES ${NEUTRON_UTILS_SOURCES} PARENT_SCOPE)
        message(STATUS "Added utility: ${UTILITY_NAME}")
    else()
        message(WARNING "Utility ${UTILITY_NAME} not found at ${CMAKE_SOURCE_DIR}/${UTILITY_PATH}")
    endif()
endfunction()

# Add component interface sources to utilities
list(APPEND NEUTRON_UTILS_SOURCES 
    ${CMAKE_SOURCE_DIR}/src/utils/component_interface.cpp
)

# Platform-specific sources - declare and initialize
set(NEUTRON_PLATFORM_SOURCES_LIST "")

set( DLFCN_WIN "")

if(WIN32)
    # Try to find dlfcn-win32, but do not require it. If it's missing, we'll compile
    # a small compatibility shim that wraps LoadLibrary/GetProcAddress.
    find_package(dlfcn-win32 CONFIG)
    if (dlfcn-win32_FOUND)
        set( DLFCN_WIN dlfcn-win32::dl)
    else()
        message(STATUS "dlfcn-win32 not found; using internal Windows shim for dlopen/dlsym/dlclose")
        # Add the shim implementation to the platform sources
        list(APPEND NEUTRON_PLATFORM_SOURCES_LIST src/platform/dlfcn_compat_win.cpp)
    endif()
    # jsoncpp is already found above
endif()

# Add all component sources together (excluding main.cpp) - must be done after platform sources are determined
set(NEUTRON_ALL_SOURCES
    ${NEUTRON_CORE_SOURCES}
    ${NEUTRON_TYPE_SOURCES}
    ${NEUTRON_FEATURE_SOURCES}
    ${NEUTRON_EXTENSION_SOURCES}
    ${NEUTRON_UTILS_SOURCES}
    ${NEUTRON_PLATFORM_SOURCES_LIST}
)

# Library sources (including modules) - must be defined after all sources are determined
set(NEUTRON_LIBRARY_SOURCES
    ${NEUTRON_ALL_SOURCES}
    ${NEUTRON_ALL_MODULE_SOURCES}
)

# Create static runtime library for executable linking
add_library(neutron_runtime STATIC
    ${NEUTRON_LIBRARY_SOURCES}
)

# On Unix-like systems, static libraries need -fPIC if they'll be linked into shared libraries
if(NOT WIN32)
    set_target_properties(neutron_runtime PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )
endif()

target_link_libraries(neutron_runtime
    ${JSONCPP_LIBRARIES}
    ${DLFCN_WIN}
    Threads::Threads
)

# Windows-specific libraries for socket functions
if(WIN32)
    target_link_libraries(neutron_runtime ws2_32)
endif()

if(HAVE_CURL)
    if(TARGET CURL::libcurl)
        target_link_libraries(neutron_runtime CURL::libcurl)
        target_compile_definitions(neutron_runtime PRIVATE HAVE_CURL)
    elseif(TARGET PkgConfig::CURL)
        target_link_libraries(neutron_runtime PkgConfig::CURL)
        target_compile_definitions(neutron_runtime PRIVATE HAVE_CURL)
    elseif(CURL_FOUND)
        target_include_directories(neutron_runtime PRIVATE ${CURL_INCLUDE_DIRS})
        target_link_libraries(neutron_runtime ${CURL_LIBRARIES})
        target_compile_definitions(neutron_runtime PRIVATE HAVE_CURL)
    endif()
endif()

# Set output names
if(WIN32)
    set_target_properties(neutron_runtime PROPERTIES
        OUTPUT_NAME neutron_runtime
    )
else()
    set_target_properties(neutron_runtime PROPERTIES
        OUTPUT_NAME neutron_runtime
        PREFIX "lib"
    )
endif()

# Create shared library for modules to link against
# This library contains the C API functions that external modules need
add_library(neutron_shared SHARED
    ${NEUTRON_LIBRARY_SOURCES}
)

target_link_libraries(neutron_shared
    ${JSONCPP_LIBRARIES}
    ${DLFCN_WIN}
)

if(HAVE_CURL)
    if(TARGET CURL::libcurl)
        target_link_libraries(neutron_shared CURL::libcurl)
        target_compile_definitions(neutron_shared PRIVATE HAVE_CURL)
    elseif(TARGET PkgConfig::CURL)
        target_link_libraries(neutron_shared PkgConfig::CURL)
        target_compile_definitions(neutron_shared PRIVATE HAVE_CURL)
    elseif(CURL_FOUND)
        target_include_directories(neutron_shared PRIVATE ${CURL_INCLUDE_DIRS})
        target_link_libraries(neutron_shared ${CURL_LIBRARIES})
        target_compile_definitions(neutron_shared PRIVATE HAVE_CURL)
    endif()
endif()

# Link threading library on all platforms
target_link_libraries(neutron_shared Threads::Threads)

# Windows-specific libraries for socket functions
if(WIN32)
    target_link_libraries(neutron_shared ws2_32)
endif()

# Define BUILDING_NEUTRON so C API functions are exported properly
target_compile_definitions(neutron_shared PRIVATE BUILDING_NEUTRON)

# Set output names
if(WIN32)
    set_target_properties(neutron_shared PROPERTIES
        OUTPUT_NAME neutron_shared
    )
else()
    set_target_properties(neutron_shared PROPERTIES
        OUTPUT_NAME neutron_shared
        PREFIX "lib"
    )
endif()

# Create executable
add_executable(neutron src/core/main.cpp)

# Define BUILDING_NEUTRON for the main executable so functions are exported
target_compile_definitions(neutron PRIVATE BUILDING_NEUTRON)

# Link executable against shared library
target_link_libraries(neutron neutron_shared)

# On Windows, also link static library to resolve missing symbols
if(WIN32)
    target_link_libraries(neutron neutron_runtime)
endif()

# Ensure threading support for main executable
target_link_libraries(neutron Threads::Threads)

# On macOS, also link jsoncpp to the main executable to ensure the library can be found at runtime
if(APPLE)
    target_link_libraries(neutron ${JSONCPP_LIBRARIES})
endif()

# Platform-specific settings
if(UNIX AND NOT APPLE)
    target_link_libraries(neutron_runtime dl)
    # Export symbols from the executable so modules can access C API functions
    target_link_options(neutron PRIVATE "-Wl,--export-dynamic")
    set_target_properties(neutron PROPERTIES
        BUILD_RPATH "$ORIGIN:$ORIGIN/../lib:/usr/local/lib"
        INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib:/usr/local/lib"
        ENABLE_EXPORTS ON
    )
elseif(APPLE)
    target_link_libraries(neutron_runtime "-framework CoreFoundation")
    set_target_properties(neutron PROPERTIES
        BUILD_RPATH "@executable_path:@executable_path/../lib:/usr/local/lib"
        INSTALL_RPATH "@executable_path:@executable_path/../lib:/usr/local/lib"
    )
elseif(WIN32)
    # Windows-specific settings
    target_compile_definitions(neutron_runtime PRIVATE _CRT_SECURE_NO_WARNINGS)

    # Export symbols for native modules via the shared library
    set_target_properties(neutron PROPERTIES ENABLE_EXPORTS ON)

    # Link with Windows library for dynamic loading (LoadLibrary, GetProcAddress, etc.)
    # Note: ws2_32 is already linked above for socket functions
    target_link_libraries(neutron_runtime kernel32)
endif()

# Install targets
install(TARGETS neutron neutron_runtime neutron_shared
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib-native
    ARCHIVE DESTINATION lib-native
)

# Build the main Box package manager from the nt-box directory
# Add the nt-box as a subdirectory to incorporate it into the build
if(EXISTS "${CMAKE_SOURCE_DIR}/nt-box/CMakeLists.txt")
    add_subdirectory(nt-box)
endif()


# Copy binaries to project root for convenience
if (NOT WIN32)
    add_custom_command(TARGET neutron POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:neutron> ${CMAKE_SOURCE_DIR}/$<TARGET_FILE_NAME:neutron>
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/build/libneutron_runtime.a ${CMAKE_SOURCE_DIR}/libneutron_runtime.a
        COMMENT "Copying binaries to project root"
    )
elseif(WIN32 AND MINGW)
    # Copy MinGW runtime DLLs to build directory so the executable works outside MSYS
    # Also copy neutron executable and static library to project root
    add_custom_command(TARGET neutron POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:neutron> ${CMAKE_SOURCE_DIR}/$<TARGET_FILE_NAME:neutron>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CXX_COMPILER}/../libgcc_s_seh-1.dll
            ${CMAKE_CXX_COMPILER}/../libwinpthread-1.dll
            ${CMAKE_CXX_COMPILER}/../libstdc++-6.dll
            $<TARGET_FILE_DIR:neutron>
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/build/libneutron_runtime.a ${CMAKE_SOURCE_DIR}/libneutron_runtime.a
        COMMENT "Copying MinGW runtime DLLs and binaries to project root"
    )
elseif(WIN32)
    # For MSVC Windows, just copy the executable and static library to project root
    add_custom_command(TARGET neutron POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:neutron> ${CMAKE_SOURCE_DIR}/$<TARGET_FILE_NAME:neutron>
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:neutron_runtime> ${CMAKE_SOURCE_DIR}/neutron_runtime.lib
        COMMENT "Copying binaries to project root"
    )
endif()

# Optional: Add tests
enable_testing()
add_test(NAME feature_test COMMAND neutron ${CMAKE_SOURCE_DIR}/dev_tests/feature_test.nt)
add_test(NAME control_flow_test COMMAND neutron ${CMAKE_SOURCE_DIR}/dev_tests/test_control_flow.nt)
add_test(NAME operators_test COMMAND neutron ${CMAKE_SOURCE_DIR}/dev_tests/test_operators.nt)
add_test(NAME arrays_test COMMAND neutron ${CMAKE_SOURCE_DIR}/dev_tests/test_arrays_objects.nt)
add_test(NAME functions_test COMMAND neutron ${CMAKE_SOURCE_DIR}/dev_tests/test_functions_modules.nt)

# --------------------------------------------------------
# LSP Server
# --------------------------------------------------------
add_executable(neutron-lsp 
    lsp/main.cpp 
    lsp/server.cpp 
    lsp/protocol.cpp
)

target_link_libraries(neutron-lsp 
    neutron_runtime
    ${JSONCPP_LIBRARIES}
)

if(WIN32)
    set_target_properties(neutron-lsp PROPERTIES
        OUTPUT_NAME neutron-lsp
    )
    target_link_libraries(neutron-lsp ${DLFCN_WIN})
endif()

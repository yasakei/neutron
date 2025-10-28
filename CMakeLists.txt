cmake_minimum_required(VERSION 3.15)
project(Neutron VERSION 1.0.4 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform detection
if(WIN32)
    add_definitions(-DNEUTRON_WINDOWS)
elseif(APPLE)
    add_definitions(-DNEUTRON_MACOS)
elseif(UNIX)
    add_definitions(-DNEUTRON_LINUX)
endif()

# Compiler options
if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -O2)
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/libs
)

# Find required packages
find_package(CURL REQUIRED)
# find_package(PkgConfig REQUIRED)
# pkg_check_modules(JSONCPP REQUIRED jsoncpp)

if(APPLE)
    find_library(JSONCPP_LIBRARY
        NAMES jsoncpp
        PATHS /opt/homebrew/lib /usr/local/lib
    )
    find_path(JSONCPP_INCLUDE_DIR
        NAMES json/json.h
        PATHS /opt/homebrew/include /usr/local/include
    )
    set(JSONCPP_LIBRARIES ${JSONCPP_LIBRARY})
    set(JSONCPP_INCLUDE_DIRS ${JSONCPP_INCLUDE_DIR})
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(JSONCPP REQUIRED jsoncpp)
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
    src/types/json_object.cpp
    src/types/json_array.cpp
    src/types/return.cpp
    src/types/version.cpp
)

# Runtime source files
set(NEUTRON_RUNTIME_SOURCES
    src/runtime/environment.cpp
    src/runtime/error_handler.cpp
    src/runtime/runtime.cpp
    src/runtime/debug.cpp
)

# Module system source files
set(NEUTRON_MODULE_SOURCES
    src/modules/module.cpp
    src/modules/module_loader.cpp
    src/modules/module_registry.cpp
    src/modules/module_utils.cpp
)

# Platform abstraction source files
set(NEUTRON_PLATFORM_SOURCES_LIST
    src/platform/platform.cpp
)

# Core system source files (excluding main.cpp which is only for the executable)
set(NEUTRON_CORE_SOURCES
    src/vm.cpp
    src/token.cpp
    src/capi.cpp
    ${NEUTRON_COMPILER_SOURCES}
    ${NEUTRON_TYPES_SOURCES}
    ${NEUTRON_RUNTIME_SOURCES}
    ${NEUTRON_MODULE_SOURCES}
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

# Add all component sources together (excluding main.cpp)
set(NEUTRON_ALL_SOURCES
    ${NEUTRON_CORE_SOURCES}
    ${NEUTRON_TYPE_SOURCES}
    ${NEUTRON_FEATURE_SOURCES}
    ${NEUTRON_EXTENSION_SOURCES}
    ${NEUTRON_UTILS_SOURCES}
    ${NEUTRON_PLATFORM_SOURCES_LIST}
)

# Library sources (including modules)
set(NEUTRON_LIBRARY_SOURCES
    ${NEUTRON_ALL_SOURCES}
    ${NEUTRON_ALL_MODULE_SOURCES}
)

# Platform-specific sources (updated variable name)
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
    find_package(jsoncpp CONFIG REQUIRED)
    set( JSONCPP_LIBRARIES JsonCpp::JsonCpp)
endif()

# Create static runtime library for executable linking
add_library(neutron_runtime STATIC
    ${NEUTRON_LIBRARY_SOURCES}
)

# Create shared runtime library for module developers
add_library(neutron_runtime_shared SHARED
    ${NEUTRON_LIBRARY_SOURCES}
)

target_link_libraries(neutron_runtime
    CURL::libcurl
    ${JSONCPP_LIBRARIES}
    ${DLFCN_WIN}
)

target_link_libraries(neutron_runtime_shared
    CURL::libcurl
    ${JSONCPP_LIBRARIES}
    ${DLFCN_WIN}
)

target_include_directories(neutron_runtime PUBLIC
    ${JSONCPP_INCLUDE_DIRS}
)

target_include_directories(neutron_runtime_shared PUBLIC
    ${JSONCPP_INCLUDE_DIRS}
)

# Set output names
set_target_properties(neutron_runtime PROPERTIES
    OUTPUT_NAME neutron_runtime
    PREFIX "lib"
)

set_target_properties(neutron_runtime_shared PROPERTIES
    OUTPUT_NAME neutron_runtime
    PREFIX "lib"
    POSITION_INDEPENDENT_CODE ON
)

# Create executable
add_executable(neutron src/main.cpp)

# Link with whole-archive to ensure C API symbols are included (Linux only)
if(UNIX AND NOT APPLE)
    target_link_libraries(neutron -Wl,--whole-archive neutron_runtime -Wl,--no-whole-archive)
else()
    target_link_libraries(neutron neutron_runtime)
endif()

# On macOS, also link jsoncpp to the main executable to ensure the library can be found at runtime
if(APPLE)
    target_link_libraries(neutron ${JSONCPP_LIBRARIES})
endif()

# Platform-specific settings
if(UNIX AND NOT APPLE)
    target_link_libraries(neutron_runtime dl)
    target_link_libraries(neutron_runtime_shared dl)
    # Export symbols from the executable so modules can access C API functions
    target_link_options(neutron PRIVATE "-Wl,--export-dynamic")
    set_target_properties(neutron PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
        ENABLE_EXPORTS ON
    )
    set_target_properties(neutron_runtime_shared PROPERTIES
        VERSION 1.0.4
        SOVERSION 1
    )
elseif(APPLE)
    target_link_libraries(neutron_runtime "-framework CoreFoundation")
    target_link_libraries(neutron_runtime_shared "-framework CoreFoundation")
    # Ensure jsoncpp library is explicitly linked for macOS shared library
    # Use the full LDFLAGS from pkg-config to ensure proper linking on macOS
    target_link_libraries(neutron_runtime_shared ${JSONCPP_LDFLAGS})
    set_target_properties(neutron PROPERTIES
        BUILD_RPATH "@executable_path"
        INSTALL_RPATH "@executable_path"
    )
    set_target_properties(neutron_runtime_shared PROPERTIES
        VERSION 1.0.4
        SOVERSION 1
    )
elseif(WIN32)
    # Windows-specific settings
    target_compile_definitions(neutron_runtime PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(neutron_runtime_shared PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Install targets
install(TARGETS neutron neutron_runtime neutron_runtime_shared
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)


# Copy binaries to project root for convenience
if (NOT WIN32)
    add_custom_command(TARGET neutron POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:neutron> ${CMAKE_SOURCE_DIR}/$<TARGET_FILE_NAME:neutron>
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:neutron_runtime_shared> ${CMAKE_SOURCE_DIR}/$<TARGET_FILE_NAME:neutron_runtime_shared>
        COMMENT "Copying binaries to project root"
    )
elseif(WIN32 AND MINGW)
    # Copy MinGW runtime DLLs to build directory so the executable works outside MSYS
    add_custom_command(TARGET neutron POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CXX_COMPILER}/../libgcc_s_seh-1.dll
            ${CMAKE_CXX_COMPILER}/../libwinpthread-1.dll
            ${CMAKE_CXX_COMPILER}/../libstdc++-6.dll
            $<TARGET_FILE_DIR:neutron>
        COMMENT "Copying MinGW runtime DLLs to build directory"
    )
endif()

# Optional: Add tests
enable_testing()
add_test(NAME feature_test COMMAND neutron ${CMAKE_SOURCE_DIR}/dev_tests/feature_test.nt)
add_test(NAME control_flow_test COMMAND neutron ${CMAKE_SOURCE_DIR}/dev_tests/test_control_flow.nt)
add_test(NAME operators_test COMMAND neutron ${CMAKE_SOURCE_DIR}/dev_tests/test_operators.nt)
add_test(NAME arrays_test COMMAND neutron ${CMAKE_SOURCE_DIR}/dev_tests/test_arrays_objects.nt)
add_test(NAME functions_test COMMAND neutron ${CMAKE_SOURCE_DIR}/dev_tests/test_functions_modules.nt)
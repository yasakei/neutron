cmake_minimum_required(VERSION 3.15)
project(Neutron VERSION 1.0.3 LANGUAGES CXX)

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
    ${CMAKE_SOURCE_DIR}/box
)

# Find required packages
find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

# Core source files
set(NEUTRON_CORE_SOURCES
    src/compiler/scanner.cpp
    src/compiler/parser.cpp
    src/compiler/compiler.cpp
    src/compiler/bytecode.cpp
    src/types/value.cpp
    src/runtime/environment.cpp
    src/vm.cpp
    src/runtime/runtime.cpp
    src/runtime/debug.cpp
    src/token.cpp
    src/capi.cpp
    src/modules/module.cpp
    src/modules/module_loader.cpp
    src/modules/module_registry.cpp
    src/modules/module_utils.cpp
    src/platform/platform.cpp
)

# Type sources
set(NEUTRON_TYPE_SOURCES
    src/types/array.cpp
    src/types/json_object.cpp
    src/types/json_array.cpp
    src/types/return.cpp
)

# Module sources
set(NEUTRON_MODULE_SOURCES
    libs/sys/native.cpp
    libs/convert/native.cpp
    libs/json/native.cpp
    libs/math/native.cpp
    libs/http/native.cpp
    libs/time/native.cpp
)

# Create shared runtime library
add_library(neutron_runtime
    ${NEUTRON_CORE_SOURCES}
    ${NEUTRON_TYPE_SOURCES}
    ${NEUTRON_MODULE_SOURCES}
)

set( DLFCN_WIN "")

if(WIN32)
    # Try to find dlfcn-win32, but do not require it. If it's missing, we'll compile
    # a small compatibility shim that wraps LoadLibrary/GetProcAddress.
    find_package(dlfcn-win32 CONFIG)
    if (dlfcn-win32_FOUND)
        set( DLFCN_WIN dlfcn-win32::dl)
    else()
        message(STATUS "dlfcn-win32 not found; using internal Windows shim for dlopen/dlsym/dlclose")
        # Add the shim implementation to the runtime sources
        target_sources(neutron_runtime PRIVATE src/platform/dlfcn_compat_win.cpp)
    endif()
    find_package(jsoncpp CONFIG REQUIRED)
    set( JSONCPP_LIBRARIES JsonCpp::JsonCpp)
endif()

target_link_libraries(neutron_runtime
    CURL::libcurl
    ${JSONCPP_LIBRARIES}
    ${DLFCN_WIN}
)

target_include_directories(neutron_runtime PUBLIC
    ${JSONCPP_INCLUDE_DIRS}
)

# Set output name
set_target_properties(neutron_runtime PROPERTIES
    OUTPUT_NAME neutron_runtime
    PREFIX "lib"
)

# Create executable
add_executable(neutron src/main.cpp)

# Link with whole-archive to ensure C API symbols are included
if(UNIX)
    target_link_libraries(neutron -Wl,--whole-archive neutron_runtime -Wl,--no-whole-archive)
else()
    target_link_libraries(neutron neutron_runtime)
endif()

# Platform-specific settings
if(UNIX AND NOT APPLE)
    target_link_libraries(neutron_runtime dl)
    # Export symbols from the executable so modules can access C API functions
    target_link_options(neutron PRIVATE "-Wl,--export-dynamic")
    set_target_properties(neutron PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
        ENABLE_EXPORTS ON
    )
elseif(APPLE)
    target_link_libraries(neutron_runtime "-framework CoreFoundation")
    set_target_properties(neutron PROPERTIES
        BUILD_RPATH "@executable_path"
        INSTALL_RPATH "@executable_path"
    )
elseif(WIN32)
    # Windows-specific settings
    target_compile_definitions(neutron_runtime PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Install targets
install(TARGETS neutron neutron_runtime
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)


# Copy binaries to project root for convenience
if (NOT WIN32)
    add_custom_command(TARGET neutron POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:neutron> ${CMAKE_SOURCE_DIR}/$<TARGET_FILE_NAME:neutron>
        # COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:neutron_runtime> ${CMAKE_SOURCE_DIR}/libneutron_runtime.so (no need as we compile the runtime as a static lib atm)
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

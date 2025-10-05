cmake_minimum_required(VERSION 3.15)
project(Neutron VERSION 1.0.0 LANGUAGES CXX)

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
add_library(neutron_runtime SHARED
    ${NEUTRON_CORE_SOURCES}
    ${NEUTRON_TYPE_SOURCES}
    ${NEUTRON_MODULE_SOURCES}
)

target_link_libraries(neutron_runtime
    CURL::libcurl
    ${JSONCPP_LIBRARIES}
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

target_link_libraries(neutron neutron_runtime)

# Platform-specific settings
if(UNIX AND NOT APPLE)
    target_link_libraries(neutron_runtime dl)
    set_target_properties(neutron PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
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
add_custom_command(TARGET neutron POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:neutron> ${CMAKE_SOURCE_DIR}/neutron
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:neutron_runtime> ${CMAKE_SOURCE_DIR}/libneutron_runtime.so
    COMMENT "Copying binaries to project root"
)

# Optional: Add tests
enable_testing()
add_test(NAME feature_test COMMAND neutron ${CMAKE_SOURCE_DIR}/dev_tests/feature_test.nt)
add_test(NAME control_flow_test COMMAND neutron ${CMAKE_SOURCE_DIR}/dev_tests/test_control_flow.nt)
add_test(NAME operators_test COMMAND neutron ${CMAKE_SOURCE_DIR}/dev_tests/test_operators.nt)
add_test(NAME arrays_test COMMAND neutron ${CMAKE_SOURCE_DIR}/dev_tests/test_arrays_objects.nt)
add_test(NAME functions_test COMMAND neutron ${CMAKE_SOURCE_DIR}/dev_tests/test_functions_modules.nt)

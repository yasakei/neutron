cmake_minimum_required(VERSION 3.15)
project(Neutron VERSION 1.0.0 LANGUAGES CXX)

# ----------------------------
# C++ standard & compiler flags
# ----------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /bigobj)
else()
    add_compile_options(-Wall -Wextra -O2)
endif()

# ----------------------------
# Include directories
# ----------------------------
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/libs
    ${CMAKE_SOURCE_DIR}/box
)

# ----------------------------
# Source files
# ----------------------------
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
)

set(NEUTRON_TYPE_SOURCES
    src/types/array.cpp
    src/types/json_object.cpp
    src/types/json_array.cpp
    src/types/return.cpp
)

set(NEUTRON_MODULE_SOURCES
    libs/sys/native.cpp
    libs/convert/native.cpp
    libs/json/native.cpp
    libs/math/native.cpp
    libs/http/native.cpp
    libs/time/native.cpp
)

# ----------------------------
# Create shared runtime library
# ----------------------------
add_library(neutron_runtime STATIC
    ${NEUTRON_CORE_SOURCES}
    ${NEUTRON_TYPE_SOURCES}
    ${NEUTRON_MODULE_SOURCES}
)

# ----------------------------
# Find and link packages
# ----------------------------
find_package(CURL QUIET)

if(WIN32)
    # JSONCPP (vcpkg target)
    find_package(jsoncpp CONFIG REQUIRED)
    target_link_libraries(neutron_runtime PRIVATE JsonCpp::JsonCpp CURL::libcurl)

    # dlfcn-win32
    find_package(dlfcn-win32 CONFIG REQUIRED)
    target_link_libraries(neutron_runtime PRIVATE dlfcn-win32::dl)

    # Suppress MSVC warnings
    target_compile_definitions(neutron_runtime PRIVATE _CRT_SECURE_NO_WARNINGS)

else()
    # Unix / macOS
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(JSONCPP REQUIRED jsoncpp)
    target_include_directories(neutron_runtime PRIVATE ${JSONCPP_INCLUDE_DIRS})
    target_link_libraries(neutron_runtime PRIVATE ${JSONCPP_LIBRARIES} CURL::libcurl)

    if(UNIX AND NOT APPLE)
        target_link_libraries(neutron_runtime PRIVATE dl)
    elseif(APPLE)
        target_link_libraries(neutron_runtime PRIVATE "-framework CoreFoundation")
    endif()
endif()

# ----------------------------
# Create executable
# ----------------------------
add_executable(neutron src/main.cpp)

# Ensure library builds before executable
add_dependencies(neutron neutron_runtime)

# Link runtime library
target_link_libraries(neutron PRIVATE neutron_runtime)


# ----------------------------
# Install targets
# ----------------------------
install(TARGETS neutron neutron_runtime
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)

# ----------------------------
# Optional: add tests
# ----------------------------
enable_testing()
add_test(NAME feature_test COMMAND neutron ${CMAKE_SOURCE_DIR}/dev_tests/feature_test.nt)
add_test(NAME control_flow_test COMMAND neutron ${CMAKE_SOURCE_DIR}/dev_tests/test_control_flow.nt)
add_test(NAME operators_test COMMAND neutron ${CMAKE_SOURCE_DIR}/dev_tests/test_operators.nt)
add_test(NAME arrays_test COMMAND neutron ${CMAKE_SOURCE_DIR}/dev_tests/test_arrays_objects.nt)
add_test(NAME functions_test COMMAND neutron ${CMAKE_SOURCE_DIR}/dev_tests/test_functions_modules.nt)

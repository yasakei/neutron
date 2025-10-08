cmake_minimum_required(VERSION 3.15)
project(box VERSION 1.0.0 LANGUAGES CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform detection
if(WIN32)
    add_definitions(-DPLATFORM_WINDOWS)
elseif(APPLE)
    add_definitions(-DPLATFORM_MACOS)
elseif(UNIX)
    add_definitions(-DPLATFORM_LINUX)
endif()

# Source files
set(BOX_SOURCES
    src/main.cpp
    src/platform.cpp
    src/registry.cpp
    src/installer.cpp
    src/builder.cpp
)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Create executable
add_executable(box ${BOX_SOURCES})

# Platform-specific linking
if(WIN32)
    # Windows: Link WinINet for HTTP requests
    target_link_libraries(box wininet)
else()
    # Unix-like: Link libcurl for HTTP requests
    find_package(CURL REQUIRED)
    target_link_libraries(box ${CURL_LIBRARIES})
    target_include_directories(box PRIVATE ${CURL_INCLUDE_DIR})
endif()

# Installation
install(TARGETS box DESTINATION bin)

# Copy to project root for convenience
add_custom_command(TARGET box POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:box> ${CMAKE_SOURCE_DIR}/../box${CMAKE_EXECUTABLE_SUFFIX}
    COMMENT "Copying box binary to project root"
)

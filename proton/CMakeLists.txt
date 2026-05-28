cmake_minimum_required(VERSION 3.15)
project(proton LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(PROTON_COMMON_SRCS
    util.c
    parse.c
    abi.c
    cfg.c
    mem.c
    ssa.c
    alias.c
    load.c
    copy.c
    fold.c
    gvn.c
    gcm.c
    simpl.c
    ifopt.c
    live.c
    spill.c
    rega.c
    emit.c
)

set(PROTON_AMD64_SRCS
    amd64/targ.c
    amd64/sysv.c
    amd64/isel.c
    amd64/emit.c
)

set(PROTON_ARM64_SRCS
    arm64/targ.c
    arm64/abi.c
    arm64/isel.c
    arm64/emit.c
)

set(PROTON_RV64_SRCS
    rv64/targ.c
    rv64/abi.c
    rv64/isel.c
    rv64/emit.c
)

add_library(proton STATIC
    libproton.c
    ${PROTON_COMMON_SRCS}
    ${PROTON_AMD64_SRCS}
    ${PROTON_ARM64_SRCS}
    ${PROTON_RV64_SRCS}
)

# Detect host architecture and select QBE target backend
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64|arm64")
    if(APPLE)
        target_compile_definitions(proton PRIVATE Deftgt=T_arm64_apple)
    else()
        target_compile_definitions(proton PRIVATE Deftgt=T_arm64)
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64|X86_64")
    if(APPLE)
        target_compile_definitions(proton PRIVATE Deftgt=T_amd64_apple)
    else()
        target_compile_definitions(proton PRIVATE Deftgt=T_amd64_sysv)
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64|RISCV64|rv64")
    target_compile_definitions(proton PRIVATE Deftgt=T_rv64)
else()
    message(WARNING "Unknown architecture ${CMAKE_SYSTEM_PROCESSOR}, defaulting to amd64_sysv")
    target_compile_definitions(proton PRIVATE Deftgt=T_amd64_sysv)
endif()

target_include_directories(proton PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(proton PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/amd64
    ${CMAKE_CURRENT_SOURCE_DIR}/arm64
    ${CMAKE_CURRENT_SOURCE_DIR}/rv64
)

if(MSVC)
    foreach(src ${PROTON_COMMON_SRCS} ${PROTON_AMD64_SRCS} ${PROTON_ARM64_SRCS} ${PROTON_RV64_SRCS} libproton.c)
        set_source_files_properties(${src} PROPERTIES LANGUAGE CXX)
    endforeach()
    target_compile_options(proton PRIVATE /W0)
else()
    target_compile_options(proton PRIVATE -w)
endif()

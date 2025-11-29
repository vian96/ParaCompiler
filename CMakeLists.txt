cmake_minimum_required(VERSION 3.28)
project(ParaCL LANGUAGES CXX)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16.0)
        message(FATAL_ERROR "CRITICAL: Clang version ${CMAKE_CXX_COMPILER_VERSION} is too old. C++ Modules require Clang 16+ (Recommended: 18+).")
    else()
        message(STATUS "CHECK: Clang version ${CMAKE_CXX_COMPILER_VERSION} - OK")
    endif()
else()
    message(WARNING "CHECK: You are not using Clang. C++ Modules support might be unstable.")
endif()

find_program(ANTLR_EXECUTABLE NAMES antlr4)
if(ANTLR_EXECUTABLE)
    execute_process(COMMAND ${ANTLR_EXECUTABLE}
        OUTPUT_VARIABLE ANTLR_VER_OUTPUT
        ERROR_VARIABLE ANTLR_VER_ERROR
    )
    string(REGEX MATCH "Version ([0-9]+\\.[0-9]+\\.[0-9]+)" _ ${ANTLR_VER_OUTPUT})
    set(ANTLR_VERSION ${CMAKE_MATCH_1})

    if(ANTLR_VERSION VERSION_LESS "4.8")
        message(WARNING "CHECK: ANTLR version ${ANTLR_VERSION} is detected. Versions older than 4.9.3 are known to have C++ generation issues. Please upgrade.")
    elseif(ANTLR_VERSION STREQUAL "")
        message(STATUS "CHECK: Could not detect ANTLR version automatically (it's okay if build succeeds).")
    else()
        message(STATUS "CHECK: ANTLR version ${ANTLR_VERSION} - OK")
    endif()
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM Libs: ${LLVM_LIBRARY_DIRS}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
link_directories(${LLVM_LIBRARY_DIRS})

find_package(antlr4-runtime QUIET)
if (NOT antlr4-runtime_FOUND)
    message(STATUS "Standard ANTLR package not found, searching manually...")
    find_path(ANTLR4_INCLUDE_DIR "antlr4-runtime.h" PATH_SUFFIXES antlr4-runtime)
    find_library(ANTLR4_LIBRARY NAMES antlr4-runtime)

    if (ANTLR4_INCLUDE_DIR AND ANTLR4_LIBRARY)
        message(STATUS "Found ANTLR headers: ${ANTLR4_INCLUDE_DIR}")
        message(STATUS "Found ANTLR lib: ${ANTLR4_LIBRARY}")
        add_library(antlr4_runtime UNKNOWN IMPORTED)
        set_target_properties(antlr4_runtime PROPERTIES
            IMPORTED_LOCATION "${ANTLR4_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${ANTLR4_INCLUDE_DIR}"
        )
    else()
        message(FATAL_ERROR "ANTLR4 Runtime not found!")
    endif()
else()
    if(NOT TARGET antlr4_runtime)
        add_library(antlr4_runtime ALIAS antlr4_runtime::antlr4_runtime)
    endif()
endif()

add_subdirectory(src)

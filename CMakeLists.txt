cmake_minimum_required(VERSION 3.28)
project(ParaCL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING "Recommended to use Clang for C++ Modules.")
endif()

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

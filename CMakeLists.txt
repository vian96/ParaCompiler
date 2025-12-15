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

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Java REQUIRED COMPONENTS Runtime)

set(ANTLR_VERSION "4.13.1")
set(ANTLR_JAR_PATH "${CMAKE_BINARY_DIR}/antlr-${ANTLR_VERSION}-complete.jar")

if(NOT EXISTS "${ANTLR_JAR_PATH}")
    message(STATUS "Downloading ANTLR4 Generator ${ANTLR_VERSION}...")
    file(DOWNLOAD "https://www.antlr.org/download/antlr-${ANTLR_VERSION}-complete.jar" "${ANTLR_JAR_PATH}"
        SHOW_PROGRESS
    )
endif()

get_filename_component(WORKSPACE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(ANTLR4_LOCAL_ROOT "${WORKSPACE_ROOT}/external/antlr4")

if(EXISTS "${ANTLR4_LOCAL_ROOT}/runtime/Cpp/CMakeLists.txt")
    message(STATUS "CHECK: Found ANTLR4 runtime sources in ${ANTLR4_LOCAL_ROOT}")

    set(ANTLR4_INSTALL OFF CACHE BOOL "" FORCE)
    set(WITH_DEMO OFF CACHE BOOL "" FORCE)
    set(ANTLR_BUILD_CPP_TESTS OFF CACHE BOOL "" FORCE)

    add_subdirectory("${ANTLR4_LOCAL_ROOT}/runtime/Cpp" "external/antlr4_runtime")
else()
    message(FATAL_ERROR "CRITICAL: ANTLR4 sources not found at ${ANTLR4_LOCAL_ROOT}.\nDid you run 'repo sync' or clone the external dependency?")
endif()

find_package(LLVM REQUIRED CONFIG)
if(${LLVM_PACKAGE_VERSION} VERSION_LESS 20.0)
    message(FATAL_ERROR
        "CRITICAL: Found LLVM ${LLVM_PACKAGE_VERSION}, but version 20.0+ is required. "
        "Please install a newer LLVM or check your LLVM_DIR path."
    )
else()
    message(STATUS "CHECK: LLVM version ${LLVM_PACKAGE_VERSION} - OK")
endif()
message(STATUS "LLVM Libs: ${LLVM_LIBRARY_DIRS}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
link_directories(${LLVM_LIBRARY_DIRS})

add_subdirectory(src)

cmake_minimum_required(VERSION 3.28)
project(ParaCL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- 1. Compiler Checks ---
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16.0)
        message(FATAL_ERROR "CRITICAL: Clang too old.")
    else()
        message(STATUS "CHECK: Clang version ${CMAKE_CXX_COMPILER_VERSION} - OK")
    endif()
endif()

# --- 2. ANTLR Generator Setup (The Tool) ---
# We need the JAR to generate C++ code. This replaces the system 'antlr4' command.
find_package(Java REQUIRED COMPONENTS Runtime)

set(ANTLR_VERSION "4.13.1")
set(ANTLR_JAR_URL "https://www.antlr.org/download/antlr-${ANTLR_VERSION}-complete.jar")
set(ANTLR_JAR_PATH "${CMAKE_BINARY_DIR}/antlr-${ANTLR_VERSION}-complete.jar")

if(NOT EXISTS "${ANTLR_JAR_PATH}")
    message(STATUS "Downloading ANTLR4 Generator ${ANTLR_VERSION}...")
    file(DOWNLOAD "${ANTLR_JAR_URL}" "${ANTLR_JAR_PATH}"
        SHOW_PROGRESS
    )
endif()

# --- 3. ANTLR Runtime Setup (The C++ Library) ---
include(FetchContent)

set(ANTLR4_INSTALL OFF CACHE BOOL "" FORCE)
set(WITH_DEMO OFF CACHE BOOL "" FORCE)
set(ANTLR_BUILD_CPP_TESTS OFF CACHE BOOL "" FORCE)
set(DISABLE_WARNINGS ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  antlr4_runtime
  GIT_REPOSITORY https://github.com/antlr/antlr4.git
  GIT_TAG        ${ANTLR_VERSION} # Keep versions in sync
  SOURCE_SUBDIR  runtime/Cpp
)

FetchContent_MakeAvailable(antlr4_runtime)

# --- 4. LLVM Setup ---
find_package(LLVM REQUIRED CONFIG)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
link_directories(${LLVM_LIBRARY_DIRS})

add_subdirectory(src)

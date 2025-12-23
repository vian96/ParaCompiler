cmake_minimum_required(VERSION 3.30)

set(CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE_API "2182bf5c-ef0d-489a-91da-49dbc3090d2a")
set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 1)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")

project(ParaCL LANGUAGES CXX)

list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../external/dist")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 18.0)
        message(FATAL_ERROR "CRITICAL: Clang 18+ required.")
    endif()
    message(STATUS "CHECK: Clang version ${CMAKE_CXX_COMPILER_VERSION} - OK")

    add_compile_options(-stdlib=libc++)
    add_link_options(-stdlib=libc++ -lc++abi)
else()
    message(WARNING "CHECK: You are not using Clang. C++ Modules support might be unstable.")
endif()

find_package(Java REQUIRED COMPONENTS Runtime)

set(ANTLR_VERSION "4.13.1")
set(ANTLR_JAR_PATH "${CMAKE_BINARY_DIR}/antlr-${ANTLR_VERSION}-complete.jar")

if(NOT EXISTS "${ANTLR_JAR_PATH}")
    message(STATUS "Downloading ANTLR4 Generator ${ANTLR_VERSION}...")
    file(DOWNLOAD "https://www.antlr.org/download/antlr-${ANTLR_VERSION}-complete.jar" "${ANTLR_JAR_PATH}"
        SHOW_PROGRESS
    )
endif()

find_package(antlr4-runtime REQUIRED)

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
add_subdirectory(tests)

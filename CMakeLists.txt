cmake_minimum_required(VERSION 3.30)

set(CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE_API "2182bf5c-ef0d-489a-91da-49dbc3090d2a")
set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 1)

if(NOT CMAKE_VERSION VERSION_EQUAL "4.2.0")
    message(WARNING
        "Build environment mismatch: You are using CMake ${CMAKE_VERSION}, "
        "but build was thoroughly tested only on 4.2.0."
    )
endif()

unset(_paracl_std_uuid)
set(_paracl_default_std_uuid "d0edc3af-4c50-42ea-a356-e2862fe7a444")

if (
    (CMAKE_VERSION VERSION_GREATER_EQUAL "4.0.3" AND CMAKE_VERSION VERSION_LESS_EQUAL "4.2.1") OR
    (CMAKE_VERSION VERSION_GREATER_EQUAL "3.31.8" AND CMAKE_VERSION VERSION_LESS_EQUAL "3.31.10")
)
    set(_paracl_std_uuid "d0edc3af-4c50-42ea-a356-e2862fe7a444")

elseif (CMAKE_VERSION VERSION_GREATER_EQUAL "4.0.0" AND CMAKE_VERSION VERSION_LESS_EQUAL "4.0.2")
    set(_paracl_std_uuid "a9e1cf81-9932-4810-974b-6eccaf14e457")

elseif (CMAKE_VERSION VERSION_GREATER_EQUAL "3.30.0" AND CMAKE_VERSION VERSION_LESS_EQUAL "3.31.7")
    set(_paracl_std_uuid "0e5b6991-d74f-4b3d-a41c-cf096e0b2508")

else()
    message(WARNING
        "CMake ${CMAKE_VERSION}: Unknown version for C++23 'import std'. "
        "Experimental features might fail."
    )
endif()

if(_paracl_std_uuid)
    message(STATUS "Enabled experimental C++23 'import std' for CMake ${CMAKE_VERSION} (UUID: ${_paracl_std_uuid})")
else()
    set(_paracl_std_uuid "${_paracl_default_std_uuid}")
    message(WARNING
        "CMake ${CMAKE_VERSION}: Unknown version for C++23 'import std'. "
        "Applying default UUID for that feature (for versions 4.0.3-4.2.1, ${_paracl_std_uuid}). "
        "Experimental features may not be configured and build may fail."
    )
endif()

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "${_paracl_std_uuid}")


project(ParaCL LANGUAGES CXX)

list(PREPEND CMAKE_PREFIX_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/dist"
    "/opt/paracl-toolchain"
)

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

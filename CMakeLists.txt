get_filename_component(WORKSPACE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(LOCAL_ANTLR "${WORKSPACE_ROOT}/external/antlr4")
set(LOCAL_LLVM  "${WORKSPACE_ROOT}/external/llvm-project")

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found System LLVM ${LLVM_PACKAGE_VERSION}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

if(EXISTS "${LOCAL_LLVM}/llvm/include")
    message(STATUS "Using Bleeding Edge LLVM Headers from ${LOCAL_LLVM}")
    include_directories(BEFORE "${LOCAL_LLVM}/llvm/include")
else()
    include_directories(${LLVM_INCLUDE_DIRS})
endif()

add_definitions(${LLVM_DEFINITIONS})
link_directories(${LLVM_LIBRARY_DIRS})

if(EXISTS "${LOCAL_ANTLR}/runtime/Cpp/CMakeLists.txt")
    message(STATUS "Building ANTLR Runtime from source (${LOCAL_ANTLR})")

    set(ANTLR4_INSTALL OFF CACHE BOOL "" FORCE)
    set(WITH_DEMO OFF CACHE BOOL "" FORCE)
    set(ANTLR_BUILD_CPP_TESTS OFF CACHE BOOL "" FORCE)

    add_subdirectory("${LOCAL_ANTLR}/runtime/Cpp" "external/antlr4_runtime")

    if(NOT TARGET antlr4_runtime)
        add_library(antlr4_runtime ALIAS antlr4_static)
    endif()
else()
    message(STATUS "Standard Mode: Searching for system ANTLR package...")
    find_package(antlr4-runtime QUIET)
    if (NOT antlr4-runtime_FOUND)
        find_path(ANTLR4_INCLUDE_DIR "antlr4-runtime.h" PATH_SUFFIXES antlr4-runtime)
        find_library(ANTLR4_LIBRARY NAMES antlr4-runtime)

        if (ANTLR4_INCLUDE_DIR AND ANTLR4_LIBRARY)
            message(STATUS "Found ANTLR headers: ${ANTLR4_INCLUDE_DIR}")
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
endif()

add_subdirectory(src)

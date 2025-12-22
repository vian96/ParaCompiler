set(GRAMMAR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/grammar)
set(GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated)
set(GRAMMAR_FILE ${GRAMMAR_DIR}/ParaCL.g4)

file(MAKE_DIRECTORY ${GEN_DIR})

set(ANTLR_OUTPUTS
    ${GEN_DIR}/ParaCLLexer.cpp
    ${GEN_DIR}/ParaCLLexer.h
    ${GEN_DIR}/ParaCLParser.cpp
    ${GEN_DIR}/ParaCLParser.h
    ${GEN_DIR}/ParaCLVisitor.cpp
    ${GEN_DIR}/ParaCLVisitor.h
    ${GEN_DIR}/ParaCLBaseVisitor.cpp
    ${GEN_DIR}/ParaCLBaseVisitor.h
)

add_custom_command(
    OUTPUT ${ANTLR_OUTPUTS}
    COMMAND ${Java_JAVA_EXECUTABLE} -jar ${ANTLR_JAR_PATH} -Dlanguage=Cpp -visitor -no-listener -o ${GEN_DIR} ${GRAMMAR_FILE}
    DEPENDS ${GRAMMAR_FILE} ${ANTLR_JAR_PATH}
    COMMENT "Generating ANTLR C++ parser (v${ANTLR_VERSION})..."
)

add_library(paracl_parser STATIC ${ANTLR_OUTPUTS})
target_include_directories(paracl_parser PUBLIC ${GEN_DIR})

target_include_directories(paracl_parser PUBLIC
    "${WORKSPACE_ROOT}/external/antlr4/runtime/Cpp/runtime/src"
)

target_link_libraries(paracl_parser PUBLIC antlr4_static)

target_compile_options(paracl_parser PRIVATE -frtti)

set(CORE_HEADERS
    ast.hpp
    visitor.hpp
    antlr_parser.hpp
    dump_visitor.hpp
    default_visitor.hpp
)

set(MODULE_SOURCES
    # modules/Compiler.ixx
)

add_library(paracl_core STATIC ${CORE_HEADERS})

if(MODULE_SOURCES)
    target_sources(paracl_core
        PUBLIC
        FILE_SET CXX_MODULES FILES ${MODULE_SOURCES}
    )
endif()

target_include_directories(paracl_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(paracl_core PUBLIC paracl_parser)

find_program(LLVM_CONFIG_PATH "llvm-config" HINTS ${LLVM_TOOLS_BINARY_DIR})

if(LLVM_CONFIG_PATH)
    message(STATUS "Found llvm-config: ${LLVM_CONFIG_PATH}")

    execute_process(
        COMMAND ${LLVM_CONFIG_PATH} --link-shared --libs core support irreader target native
        OUTPUT_VARIABLE LLVM_LINK_LIBS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    string(REPLACE "\n" " " LLVM_LINK_LIBS "${LLVM_LINK_LIBS}")
    string(STRIP "${LLVM_LINK_LIBS}" LLVM_LINK_LIBS)

    message(STATUS "LLVM Link Flags: ${LLVM_LINK_LIBS}")
else()
    message(FATAL_ERROR "llvm-config not found! Cannot determine link flags.")
endif()

target_link_libraries(paracl_core PUBLIC ${LLVM_LINK_LIBS})

target_compile_options(paracl_core PRIVATE -frtti)

add_executable(paracl main.cpp)
target_link_libraries(paracl PRIVATE paracl_core)
target_compile_options(paracl PRIVATE -frtti)

add_library(parastdlib SHARED parastdlib.cpp)
target_link_libraries(parastdlib PRIVATE ${LLVM_LINK_LIBS})
set_target_properties(parastdlib PROPERTIES OUTPUT_NAME "parastdlib")

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
target_include_directories(paracl_parser SYSTEM PUBLIC "${CMAKE_SOURCE_DIR}/../external/dist/include/antlr4-runtime")

target_link_libraries(paracl_parser PUBLIC antlr4_shared)

target_compile_options(paracl_parser PRIVATE -frtti)

set(CORE_HEADERS)

set(MODULE_SOURCES
    types.cppm
    symbol.cppm
    visitor.cppm
    ast.cppm
    type_checker.cppm
    default_visitor.cppm
    dump_visitor.cppm
    llvm_emitter.cppm
    antlr_parser.cppm
    compiler.cppm
)

add_library(paracl_core STATIC ${CORE_HEADERS})

target_sources(paracl_core
    PUBLIC
    FILE_SET CXX_MODULES FILES ${MODULE_SOURCES}
)
set_property(TARGET paracl_core PROPERTY CXX_MODULE_STD ON)

target_include_directories(paracl_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(paracl_core PUBLIC paracl_parser)

find_program(LLVM_CONFIG_PATH "llvm-config" HINTS ${LLVM_TOOLS_BINARY_DIR})

if(LLVM_CONFIG_PATH)
    message(STATUS "Found llvm-config: ${LLVM_CONFIG_PATH}")

    execute_process(
        COMMAND ${LLVM_CONFIG_PATH} --link-static --libs core support irreader target native
        OUTPUT_VARIABLE LLVM_LINK_LIBS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    string(REPLACE "\n" " " LLVM_LINK_LIBS "${LLVM_LINK_LIBS}")
    string(STRIP "${LLVM_LINK_LIBS}" LLVM_LINK_LIBS)

    execute_process(
        COMMAND ${LLVM_CONFIG_PATH} --link-static --system-libs
        OUTPUT_VARIABLE LLVM_SYSTEM_LIBS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(REPLACE "\n" " " LLVM_SYSTEM_LIBS "${LLVM_SYSTEM_LIBS}")

    message(STATUS "LLVM Link Flags: ${LLVM_LINK_LIBS} ${LLVM_SYSTEM_LIBS}")
else()
    message(FATAL_ERROR "llvm-config not found! Cannot determine link flags.")
endif()

target_link_libraries(paracl_core
    PUBLIC
    paracl_parser
    ${LLVM_LINK_LIBS}
    ${LLVM_SYSTEM_LIBS}
)

target_compile_options(paracl_core PRIVATE -frtti)

add_executable(paracl main.cpp)
target_link_libraries(paracl PRIVATE paracl_core)
set_property(TARGET paracl PROPERTY CXX_MODULE_STD ON)
target_compile_options(paracl PRIVATE -frtti)

add_library(parastdlib SHARED parastdlib.cpp)
target_link_libraries(parastdlib PRIVATE ${LLVM_LINK_LIBS})
set_target_properties(parastdlib PROPERTIES OUTPUT_NAME "parastdlib")

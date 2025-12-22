find_program(LIT_COMMAND NAMES lit lit.py llvm-lit)

if(NOT LIT_COMMAND)
    message(WARNING "LIT not found. Test targets will not be available.")
else()
    message(STATUS "Found LIT: ${LIT_COMMAND}")

    add_custom_target(check-lit
        COMMAND ${LIT_COMMAND} -v ${CMAKE_CURRENT_SOURCE_DIR}/lit
        DEPENDS paracl
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running Regression Tests (LIT)..."
        USES_TERMINAL
    )

    add_custom_target(check-gen
        COMMAND ${LIT_COMMAND} -v ${CMAKE_CURRENT_SOURCE_DIR}/gen/pcl
        DEPENDS paracl
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running Generated Tests (Fuzzing)..."
        USES_TERMINAL
    )

    add_custom_target(check-all
        COMMAND ${LIT_COMMAND} -v ${CMAKE_CURRENT_SOURCE_DIR}/lit ${CMAKE_CURRENT_SOURCE_DIR}/gen/pcl
        DEPENDS paracl
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running ALL Tests..."
        USES_TERMINAL
    )
endif()

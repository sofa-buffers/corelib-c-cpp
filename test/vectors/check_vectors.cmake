# Regenerates the vectors to a temporary file and compares against the committed
# JSON, so CI fails if test_vectors.json drifts from the generator.
#
# Invoked by CTest with -DGEN=<generator exe> -DEXPECTED=<committed json>.

set(actual "${CMAKE_CURRENT_BINARY_DIR}/test_vectors.generated.json")

execute_process(
    COMMAND "${GEN}"
    OUTPUT_FILE "${actual}"
    RESULT_VARIABLE gen_result
)

if(NOT gen_result EQUAL 0)
    message(FATAL_ERROR "generator failed with exit code ${gen_result}")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files "${actual}" "${EXPECTED}"
    RESULT_VARIABLE diff_result
)

if(NOT diff_result EQUAL 0)
    message(FATAL_ERROR
        "test_vectors.json is out of date. Regenerate it with:\n"
        "  cmake --build <build-dir> --target generate-vectors")
endif()

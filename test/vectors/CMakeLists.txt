# --- test-vector generator ---
#
# Builds a small host program that replays a set of messages through the
# SofaBuffers C encoder and emits the JSON test-vector file. The committed
# test_vectors.json is the output of this program; regenerate it with the
# `generate-vectors` target after changing the encoder or the vector set.

add_executable(sofab_gen_vectors gen_vectors.c)

target_compile_options(sofab_gen_vectors
    PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-error=cpp
)

target_link_libraries(sofab_gen_vectors sofabuffers)

set(SOFAB_VECTORS_JSON ${CMAKE_CURRENT_SOURCE_DIR}/test_vectors.json)

# Regenerate the committed JSON in the source tree.
add_custom_target(generate-vectors
    COMMAND $<TARGET_FILE:sofab_gen_vectors> > ${SOFAB_VECTORS_JSON}
    DEPENDS sofab_gen_vectors
    COMMENT "Generating ${SOFAB_VECTORS_JSON}"
    VERBATIM
)

# Verify the committed JSON still matches the generator output.
add_test(
    NAME test_vectors_up_to_date
    COMMAND ${CMAKE_COMMAND}
        -DGEN=$<TARGET_FILE:sofab_gen_vectors>
        -DEXPECTED=${SOFAB_VECTORS_JSON}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_vectors.cmake
)

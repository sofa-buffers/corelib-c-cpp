# --- test-vector generator (developer helper tool) ---
#
# This is NOT a unit test. It builds a small host program that replays a set of
# messages through the SofaBuffers C encoder and prints the JSON test-vector
# file. The committed test_vectors.json is the artifact that matters; this tool
# only exists to (re)produce it from the real encoder.
#
# Regenerate the committed JSON after changing the encoder or the vector set:
#   cmake -S . -B build -DSOFAB_ENABLE_VECTORGEN=ON
#   cmake --build build --target generate-vectors

add_executable(sofab_gen_vectors gen_vectors.c)

target_compile_options(sofab_gen_vectors
    PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-error=cpp
)

target_link_libraries(sofab_gen_vectors sofabuffers)

# Regenerate the committed JSON in the source tree.
add_custom_target(generate-vectors
    COMMAND $<TARGET_FILE:sofab_gen_vectors> > ${CMAKE_CURRENT_SOURCE_DIR}/test_vectors.json
    DEPENDS sofab_gen_vectors
    COMMENT "Generating test/vectorgen/test_vectors.json"
    VERBATIM
)

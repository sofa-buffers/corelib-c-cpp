# ----------------------------------------
# SofaBuffers throughput benchmarks (MB/s)
# ----------------------------------------
# The corelib sources are compiled straight into each benchmark at -O3 so the
# measurement reflects an optimised build, independent of the size-optimised
# (-Os) library that the rest of the project links against.

set(SOFAB_BENCH_CORELIB
    ${CMAKE_SOURCE_DIR}/src/ostream.c
    ${CMAKE_SOURCE_DIR}/src/istream.c
)

# --- C benchmark ---
add_executable(bench_c c/bench.c ${SOFAB_BENCH_CORELIB})
target_include_directories(bench_c PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
target_compile_options(bench_c PRIVATE -O3 -g -DNDEBUG -Wall -Wextra)

# --- C++ benchmark ---
add_executable(bench_cpp cpp/bench.cpp ${SOFAB_BENCH_CORELIB})
target_include_directories(bench_cpp PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
target_compile_options(bench_cpp PRIVATE -O3 -g -DNDEBUG -Wall -Wextra)

# ----------------------------------------------------------------------------
# Combined per-operation cost benchmarks (CPU cycles/op + throughput MB/s).
# A single executable per language that reports both the hardware cycle count
# per operation (cost of the code, machine-clock independent) and the sustained
# throughput in MB/s (a speedtest value for the host it runs on).
# ----------------------------------------------------------------------------

# --- C per-op benchmark ---
add_executable(perf_c c/perf.c ${SOFAB_BENCH_CORELIB})
target_include_directories(perf_c PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
target_compile_options(perf_c PRIVATE -O3 -g -DNDEBUG -Wall -Wextra)

# --- C++ per-op benchmark ---
add_executable(perf_cpp cpp/perf.cpp ${SOFAB_BENCH_CORELIB})
target_include_directories(perf_cpp PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
target_compile_options(perf_cpp PRIVATE -O3 -g -DNDEBUG -Wall -Wextra)

# --- convenience target: build + run the timed (CPU-time MB/s) benchmarks ---
add_custom_target(run_bench
    COMMAND $<TARGET_FILE:bench_c>
    COMMAND $<TARGET_FILE:bench_cpp>
    DEPENDS bench_c bench_cpp
    COMMENT "Running SofaBuffers throughput benchmarks (CPU-time MB/s)"
    VERBATIM
)

# --- convenience target: build + run the per-op (cycles/op + MB/s) benchmarks -
add_custom_target(run_perf
    COMMAND $<TARGET_FILE:perf_c>
    COMMAND $<TARGET_FILE:perf_cpp>
    DEPENDS perf_c perf_cpp
    COMMENT "Running SofaBuffers per-op benchmarks (CPU cycles/op + MB/s)"
    VERBATIM
)

# --- convenience target: machine-independent instructions/op via Callgrind ---
# Requires valgrind. Deterministic instruction counts (independent of the host
# clock speed / scheduler), so the numbers compare across machines.
add_custom_target(run_bench_callgrind
    COMMAND ${CMAKE_COMMAND} -E env BUILD=${CMAKE_BINARY_DIR}
            bash ${CMAKE_CURRENT_SOURCE_DIR}/run_callgrind.sh
    DEPENDS bench_c bench_cpp
    USES_TERMINAL
    COMMENT "Measuring instructions/op under Callgrind (machine-independent)"
    VERBATIM
)

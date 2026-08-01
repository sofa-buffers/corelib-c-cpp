FROM ubuntu:26.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    gdb \
    git \
    pkg-config \
    ca-certificates \
    curl \
    valgrind \
    linux-tools-generic \
    gcovr \
    jq \
    doxygen \
    graphviz \
    && rm -rf /var/lib/apt/lists/*

# Bare-metal cross toolchains.
#
# These are what tools/footprint.sh checks for in its preflight and what the
# cross-compiling CI jobs use (.github/workflows/build-gcc-{cortex-m,avr}.yaml),
# so without them the README footprint tables cannot be regenerated or verified
# from inside the container — the one measurement this corelib exists to track.
#
# The two ARM runtime packages are not optional even though nothing here is
# linked for the target: CMake's project() probes the C and C++ compilers by
# building and *linking* a test program, so configuring any Cortex-M build fails
# without a libc and a libstdc++ for the target. RISC-V takes its headers from
# picolibc instead (--specs=picolibc.specs, see the RV32IMC toolchain file
# footprint.sh writes).
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc-arm-none-eabi \
    libnewlib-arm-none-eabi \
    libstdc++-arm-none-eabi-newlib \
    gcc-avr \
    binutils-avr \
    avr-libc \
    gcc-riscv64-unknown-elf \
    picolibc-riscv64-unknown-elf \
    && rm -rf /var/lib/apt/lists/*

# GitHub CLI (gh)
RUN mkdir -p -m 755 /etc/apt/keyrings \
    && curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg \
        | tee /etc/apt/keyrings/githubcli-archive-keyring.gpg > /dev/null \
    && chmod go+r /etc/apt/keyrings/githubcli-archive-keyring.gpg \
    && echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" \
        > /etc/apt/sources.list.d/github-cli.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends gh \
    && rm -rf /var/lib/apt/lists/*

# Node.js (LTS) für Claude Code
RUN curl -fsSL https://deb.nodesource.com/setup_lts.x | bash - \
    && apt-get install -y --no-install-recommends nodejs \
    && rm -rf /var/lib/apt/lists/*

# Claude Code
RUN npm install -g @anthropic-ai/claude-code

WORKDIR /workspace

CMD ["/bin/bash"]

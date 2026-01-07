<p align="center"><img src="assets/sofabuffers_logo.png" alt="SofaBuffers Logo" height="140"></p>

# SofaBuffers

<b>Structured Objects For Anyone</b><br>
<i>... so optimized, feels amazing.</i>

[Would you like to know more?](https://github.com/sofa-buffers)

## SofaBuffers C/C++ library

[GitHub repository](https://github.com/sofa-buffers/corelib-c-cpp)

### Supported languages

- <b>C99</b> or later
- <b>C++20</b> or later

### Built with following compilers

| Target | Status
| - | -
| GCC x86_64 (little endian) | ![badge](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-x86_64.yaml/badge.svg)
| Clang x86_64 (little endian) | ![badge](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-clang-x86_64.yaml/badge.svg)
| GCC MIPS (big endian) | ![badge](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-mips.yaml/badge.svg)
| GCC PowerPC (big endian) | ![badge](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-powerpc.yaml/badge.svg)
| GCC RISCV-V 64 (little endian) | ![badge](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-riscv64.yaml/badge.svg)

### Tested on the following architectures

* x86_64 (little endian)
* ARMv5T (little endian)
* MIPS (big endian)
* PowerPC (big endian)

### Who is this suitable for?

The C core library is very much aimed at small embedded devices, where C is simply essential. The focus here was therefore on minimal resource consumption.

The C++ core library is aimed more at IoT devices. Such devices do not necessarily run Linux directly, but they are powerful enough to deal with C++/Heap etc. However, the focus here was also on supporting IoT devices that do not support everything specified by C++ - for example, no exceptions are used, as these are often prohibited or not supported. Similarly, std::iostream is not used, as this is often not supported or simply too heavy.

### Features

Since the focus was on embedded devices, special attention was paid to the following features during implementation:

- **Keep it simple** and don't use anything too fancy.
* Fully streaming-capable to be able to **serialize larger messages than the available memory**.
* The start of the serialized message in the target buffer can be defined by an offset value, so the message buffer can be used directly to write underlying protocol headers to the same buffer.
* It should be possible to **serialize data without using the heap** to avoid heap fragmentation.
* The API of the core lib should still be clearly structured so that it **can be used without a code generator**.

### Is this C/C++ implementation zero-copy?

No. It is designed to run on small MCUs, and many of these architectures do not allow unaligned access to words.
SofaBuffers focuses on generating as little overhead as possible, which is why words in the payload of a message are not always aligned.
To be compatible with all architectures, the data from the message is copied into memory provided by the user.

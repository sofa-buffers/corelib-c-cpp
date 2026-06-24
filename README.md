<p align="center"><img src="assets/sofabuffers_logo.png" alt="SofaBuffers Logo" height="140"></p>

# SofaBuffers

<b>Structured Objects For Anyone</b><br>
<i>... so optimized, feels amazing.</i>

[Would you like to know more?](https://github.com/sofa-buffers)

## SofaBuffers C/C++ library

[GitHub repository](https://github.com/sofa-buffers/corelib-c-cpp)

[![C coverage](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/sofa-buffers/corelib-c-cpp/badges/coverage-c.json)](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-x86_64.yaml)

### Source documentation

[Documentation](https://sofa-buffers.github.io/corelib-c-cpp/)

### Supported languages

- <b>C99</b> or later
- <b>C++20</b> or later

### Built with following compilers

| Target | Status |
| - | - |
| GCC x86_64 (little endian) | [![badge](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-x86_64.yaml/badge.svg)](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-x86_64.yaml) |
| Clang x86_64 (little endian) | [![badge](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-clang-x86_64.yaml/badge.svg)](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-clang-x86_64.yaml) |
| GCC MIPS (big endian) | [![badge](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-mips.yaml/badge.svg)](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-mips.yaml) |
| GCC PowerPC (big endian) | [![badge](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-powerpc.yaml/badge.svg)](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-powerpc.yaml) |
| GCC RISCV-V 64 (little endian) | [![badge](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-riscv64.yaml/badge.svg)](https://github.com/sofa-buffers/corelib-c-cpp/actions/workflows/build-gcc-riscv64.yaml) |


### Build and test

The library is built with [CMake](https://cmake.org/) (version 3.10 or later) and a C99 / C++20 capable toolchain such as GCC or Clang.

#### Build

```sh
cmake -S . -B build
cmake --build build --parallel
```

#### Test

Tests are registered with CTest and cover both the C and C++ core libraries:

```sh
ctest --test-dir build --output-on-failure
```

#### Useful CMake options

| Option | Default | Description |
| - | - | - |
| `SOFAB_ENABLE_CPP` | `ON` | Build the C++ tests |
| `SOFAB_ENABLE_BENCH` | `ON` | Build the throughput benchmarks (`bench_c` / `bench_cpp`) |
| `SOFAB_ENABLE_COVERAGE` | `OFF` | Enable code coverage instrumentation (`-O0 -g --coverage`) |
| `SOFAB_ENABLE_FUZZ` | `OFF` | Enable fuzzing instrumentation (sanitizers) |

For example, to configure a debug build with coverage enabled:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSOFAB_ENABLE_COVERAGE=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

### Usage

The corelib serializes fields into a caller-owned buffer and deserializes them
through a small callback-driven decoder. No heap is required, and the same wire
format is shared by the C and C++ APIs.

#### C: encoding with `ostream`

Initialize an `sofab_ostream_t` over a buffer, write a few fields, then read the
number of bytes produced:

```c
#include "sofab/ostream.h"

uint8_t buf[64];
sofab_ostream_t os;
sofab_ostream_init(&os, buf, sizeof(buf), 0, NULL, NULL);

sofab_ostream_write_unsigned(&os, 1, 42);
sofab_ostream_write_signed(&os, 2, -7);
sofab_ostream_write_string(&os, 3, "hi");

size_t used = sofab_ostream_bytes_used(&os); /* buf[0..used] holds the message */
```

To serialize messages larger than the buffer, pass a flush callback. It is
invoked whenever the buffer fills up (and on the final `sofab_ostream_flush()`),
letting you hand the bytes off to a socket, file, or any other sink:

```c
static void on_flush(sofab_ostream_t *os, const uint8_t *data, size_t len, void *usrptr)
{
    int fd = *(int *)usrptr;
    write(fd, data, len); /* stream the chunk out; the buffer is then reused */
}

uint8_t scratch[16];
sofab_ostream_t os;
sofab_ostream_init(&os, scratch, sizeof(scratch), 0, on_flush, &fd);

for (uint32_t i = 0; i < 1000; i++)
    sofab_ostream_write_unsigned(&os, i, i);

sofab_ostream_flush(&os); /* flush the final partial buffer */
```

#### C: decoding with `istream`

Decoding is callback-driven: feed raw bytes and dispatch on the field id inside
the callback, binding each field to a destination with a `sofab_istream_read_*()`
function. Fields you do not read are skipped automatically.

```c
#include "sofab/istream.h"

struct my_msg { uint64_t a; int64_t b; char text[16]; };

static void on_field(sofab_istream_t *is, sofab_id_t id,
                     size_t size, size_t count, void *usrptr)
{
    struct my_msg *m = usrptr;
    (void)size; (void)count;

    switch (id)
    {
        case 1: sofab_istream_read_u64(is, &m->a); break;
        case 2: sofab_istream_read_i64(is, &m->b); break;
        case 3: sofab_istream_read_string(is, m->text, sizeof(m->text)); break;
    }
}

struct my_msg msg = {0};
sofab_istream_t is;
sofab_istream_init(&is, on_field, &msg);
sofab_istream_feed(&is, buf, used);
```

#### C: descriptor-driven `object` API

When you have many messages, the generic transcoder keeps the footprint small:
instead of per-field API calls, a constant descriptor drives both encoding and
decoding of a plain C struct.

```c
#include "sofab/object.h"

typedef struct { uint32_t id; float value; char name[16]; } sensor_t;

static const sofab_object_descr_field_t sensor_fields[] = {
    SOFAB_OBJECT_FIELD(1, sensor_t, id,    SOFAB_OBJECT_FIELDTYPE_UNSIGNED),
    SOFAB_OBJECT_FIELD(2, sensor_t, value, SOFAB_OBJECT_FIELDTYPE_FP32),
    SOFAB_OBJECT_FIELD(3, sensor_t, name,  SOFAB_OBJECT_FIELDTYPE_STRING),
};

static const sofab_object_descr_t sensor_descr =
    SOFAB_OBJECT_DESCR(sensor_fields, 3, NULL, 0);

/* encode */
sensor_t out = { .id = 7, .value = 3.14f, .name = "temp" };
uint8_t buf[64];
sofab_ostream_t os;
sofab_ostream_init(&os, buf, sizeof(buf), 0, NULL, NULL);
sofab_object_encode(&os, &sensor_descr, &out);
size_t used = sofab_ostream_bytes_used(&os);

/* decode */
sensor_t in;
sofab_object_init(&sensor_descr, &in); /* apply defaults / zero-init */

sofab_istream_t is;
sofab_object_decoder_t dec = { .info = &sensor_descr, .dst = (uint8_t *)&in, .depth = 0 };
sofab_istream_init(&is, sofab_object_field_cb, &dec);
sofab_istream_feed(&is, buf, used);
```

#### C++: encoding with `OStream`

The C++ API is a thin, type-deducing wrapper. `write()` picks the right encoding
from the argument type and the calls chain through a `Result` that carries the
first error:

```cpp
#include "sofab/sofab.hpp"

sofab::OStreamInline<64> os; // 64-byte buffer held inline

os.write(1, 42u)                      // unsigned
  .write(2, -7)                       // signed
  .write(3, "hi");                    // string

std::span<const uint8_t> msg{os.data(), os.bytesUsed()};
```

#### C++: decoding with `IStream`

Derive a message from `sofab::IStreamMessage` and dispatch fields in
`deserialize()`; `IStreamObject` wires the decoder to an embedded instance:

```cpp
#include "sofab/sofab.hpp"

class Sensor : public sofab::IStreamMessage
{
    struct Data { uint32_t id = 0; float value = 0; } data_;

    void deserialize(sofab::IStreamImpl &is, sofab::id id,
                     size_t size, size_t count) noexcept override
    {
        (void)size; (void)count;

        switch (id)
        {
            case 1: is.read(data_.id);    break;
            case 2: is.read(data_.value); break;
        }
    }

public:
    Data* operator->() noexcept { return &data_; }
};

sofab::IStreamObject<Sensor> sensor;
sensor.feed(msg.data(), msg.size()); // msg from the encoding example above
// sensor->id and sensor->value now hold the decoded values
```

### Who is this suitable for?

The C core library is very much aimed at small embedded devices, where C is simply essential. The focus here was therefore on minimal resource consumption.

The C++ core library is aimed more at IoT devices. Such devices do not necessarily run Linux directly, but they are powerful enough to deal with C++/Heap etc. However, the focus here was also on supporting IoT devices that do not support everything specified by C++ - for example, no exceptions are used, as these are often prohibited or not supported. Similarly, std::iostream is not used, as this is often not supported or simply too heavy.

### Features

Since the focus was on embedded devices, special attention was paid to the following features during implementation:

* **Keep it simple** and don't use anything too fancy.
* Fully streaming-capable to serialize **messages larger than the available memory**.
* The start of the message in the destination buffer can be defined by an offset value to reserve space for protocol headers of underlying protocols, thus **reducing the amount of copy operations**.
* Data can be serialized and deserialized **without heap** to avoid heap fragmentation.
* The corelib API should remain clearly structured so that it **can be used without a code generator**.
* **No dependencies** on other libraries to be embeddable.

### Is this C/C++ implementation zero-copy?

No. SofaBuffers focuses on generating as little protocol overhead as possible, which is why the words in the payload of a message are not always aligned.
To be compatible with all architectures, the data from the message is copied to user-provided memory.


### Footprint

This table shows the memory requirements of the C corelib for different bare metal architectures.

The lib was always built with `-Os` (optimized for minimal size).

#### Full configuration

| Architecture | .text | .data | .bss
| - | - | - | -
| ARMv6-m |            ~2.6KB | 0.0KB | 0.0KB
| ARMv7-m+fp.dp |      ~2.7KB | 0.0KB | 0.0KB
| ARMv8-m.main+fp |    ~2.7KB | 0.0KB | 0.0KB
| ARMv8.1-m.main+mve | ~2.7KB | 0.0KB | 0.0KB
| atmega8 |            ~6.3KB | 0.0KB | 0.0KB

#### Minimal configuration

| Architecture | .text | .data | .bss
| - | - | - | -
| ARMv6-m |            ~1.0KB | 0.0KB | 0.0KB
| ARMv7-m+fp.dp |      ~1.1KB | 0.0KB | 0.0KB
| ARMv8-m.main+fp |    ~1.1KB | 0.0KB | 0.0KB
| ARMv8.1-m.main+mve | ~1.1KB | 0.0KB | 0.0KB
| atmega8 |            ~2.6KB | 0.0KB | 0.0KB

corelib API without `object.c`:

| Architecture | .text | .data | .bss
| - | - | - | -
| ARMv6-m |            ~0.8KB | 0.0KB | 0.0KB
| ARMv7-m+fp.dp |      ~0.9KB | 0.0KB | 0.0KB
| ARMv8-m.main+fp |    ~0.9KB | 0.0KB | 0.0KB
| ARMv8.1-m.main+mve | ~0.9KB | 0.0KB | 0.0KB
| atmega8 |            ~1.9KB | 0.0KB | 0.0KB
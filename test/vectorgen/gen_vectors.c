/*!
 * @file gen_vectors.c
 * @brief Generator for the SofaBuffers cross-implementation test vectors.
 *
 * This program describes a set of messages as a list of high-level encode
 * operations (the "structure" + the "values"), replays each one through the
 * real SofaBuffers C encoder, and writes a JSON document that pairs every
 * message with the exact bytes the encoder produced (the "serialized binary").
 *
 * Because the declared structure and the serialized bytes both come from the
 * same op-list, they can never drift apart: the JSON is, by construction, a
 * faithful description of what the library encodes. Any other SofaBuffers
 * implementation (e.g. corelib-rs) can load the JSON, re-encode each message
 * from the structure/values, and assert it matches `serialized.hex`.
 *
 * The op-lists mirror the happy-path cases in test/c/test_ostream.c.
 *
 * Usage:  sofab_gen_vectors > assets/test_vectors.json
 *
 * SPDX-License-Identifier: MIT
 */

#include "sofab/ostream.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* op model *******************************************************************/

typedef enum
{
    K_UNSIGNED,
    K_SIGNED,
    K_BOOLEAN,
    K_FP32,
    K_FP64,
    K_STRING,
    K_BLOB,
    K_ARR_U8,  K_ARR_U16, K_ARR_U32, K_ARR_U64,
    K_ARR_I8,  K_ARR_I16, K_ARR_I32, K_ARR_I64,
    K_ARR_FP32, K_ARR_FP64,
    K_SEQ_BEGIN,
    K_SEQ_END,
} kind_t;

typedef struct
{
    kind_t      kind;
    uint32_t    id;
    uint64_t    u;     /* unsigned / boolean scalar          */
    int64_t     s;     /* signed scalar                      */
    double      f;     /* floating-point scalar              */
    const char *str;   /* string payload                     */
    const void *arr;   /* array / blob payload (typed below) */
    int32_t     count; /* array element count / blob length  */
} op_t;

typedef struct
{
    op_t   ops[512];
    size_t n;
} oplist_t;

static void push(oplist_t *l, op_t op) { l->ops[l->n++] = op; }

/* builder helpers ************************************************************/

static void op_u    (oplist_t *l, uint32_t id, uint64_t v) { push(l, (op_t){.kind = K_UNSIGNED, .id = id, .u = v}); }
static void op_i    (oplist_t *l, uint32_t id, int64_t v)  { push(l, (op_t){.kind = K_SIGNED,   .id = id, .s = v}); }
static void op_bool (oplist_t *l, uint32_t id, int v)      { push(l, (op_t){.kind = K_BOOLEAN,  .id = id, .u = v ? 1u : 0u}); }
static void op_f32  (oplist_t *l, uint32_t id, float v)    { push(l, (op_t){.kind = K_FP32,     .id = id, .f = (double)v}); }
static void op_f64  (oplist_t *l, uint32_t id, double v)   { push(l, (op_t){.kind = K_FP64,     .id = id, .f = v}); }
static void op_str  (oplist_t *l, uint32_t id, const char *v) { push(l, (op_t){.kind = K_STRING, .id = id, .str = v}); }
static void op_blob (oplist_t *l, uint32_t id, const void *v, int32_t n) { push(l, (op_t){.kind = K_BLOB, .id = id, .arr = v, .count = n}); }
static void op_arr  (oplist_t *l, kind_t k, uint32_t id, const void *v, int32_t n) { push(l, (op_t){.kind = k, .id = id, .arr = v, .count = n}); }
static void op_seqb (oplist_t *l, uint32_t id) { push(l, (op_t){.kind = K_SEQ_BEGIN, .id = id}); }
static void op_seqe (oplist_t *l)              { push(l, (op_t){.kind = K_SEQ_END}); }

/* replay through the real encoder *******************************************/

static sofab_ret_t replay(sofab_ostream_t *os, const op_t *op)
{
    switch (op->kind)
    {
        case K_UNSIGNED:  return sofab_ostream_write_unsigned(os, op->id, op->u);
        case K_SIGNED:    return sofab_ostream_write_signed(os, op->id, op->s);
        case K_BOOLEAN:   return sofab_ostream_write_boolean(os, op->id, op->u != 0);
        case K_FP32:      return sofab_ostream_write_fp32(os, op->id, (float)op->f);
        case K_FP64:      return sofab_ostream_write_fp64(os, op->id, op->f);
        case K_STRING:    return sofab_ostream_write_string(os, op->id, op->str);
        case K_BLOB:      return sofab_ostream_write_blob(os, op->id, op->arr, (size_t)op->count);
        case K_ARR_U8:    return sofab_ostream_write_array_of_u8(os, op->id, op->arr, op->count);
        case K_ARR_U16:   return sofab_ostream_write_array_of_u16(os, op->id, op->arr, op->count);
        case K_ARR_U32:   return sofab_ostream_write_array_of_u32(os, op->id, op->arr, op->count);
        case K_ARR_U64:   return sofab_ostream_write_array_of_u64(os, op->id, op->arr, op->count);
        case K_ARR_I8:    return sofab_ostream_write_array_of_i8(os, op->id, op->arr, op->count);
        case K_ARR_I16:   return sofab_ostream_write_array_of_i16(os, op->id, op->arr, op->count);
        case K_ARR_I32:   return sofab_ostream_write_array_of_i32(os, op->id, op->arr, op->count);
        case K_ARR_I64:   return sofab_ostream_write_array_of_i64(os, op->id, op->arr, op->count);
        case K_ARR_FP32:  return sofab_ostream_write_array_of_fp32(os, op->id, op->arr, op->count);
        case K_ARR_FP64:  return sofab_ostream_write_array_of_fp64(os, op->id, op->arr, op->count);
        case K_SEQ_BEGIN: return sofab_ostream_write_sequence_begin(os, op->id);
        case K_SEQ_END:   return sofab_ostream_write_sequence_end(os);
    }
    return SOFAB_RET_E_USAGE;
}

/* JSON emission *************************************************************/

static void json_string(FILE *o, const char *s)
{
    fputc('"', o);
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p)
    {
        switch (*p)
        {
            case '"':  fputs("\\\"", o); break;
            case '\\': fputs("\\\\", o); break;
            case '\b': fputs("\\b", o);  break;
            case '\f': fputs("\\f", o);  break;
            case '\n': fputs("\\n", o);  break;
            case '\r': fputs("\\r", o);  break;
            case '\t': fputs("\\t", o);  break;
            default:
                if (*p < 0x20)
                    fprintf(o, "\\u%04x", *p);
                else
                    fputc(*p, o); /* raw UTF-8 byte */
        }
    }
    fputc('"', o);
}

/* floats: finite values as JSON numbers (round-trippable), the only
 * non-finite values present in the vectors (+/-inf) as string tokens. */
static void json_float(FILE *o, double v, int is32)
{
    if (isinf(v)) { fputs(v < 0 ? "\"-inf\"" : "\"inf\"", o); return; }
    if (is32) fprintf(o, "%.9g", (double)(float)v);
    else      fprintf(o, "%.17g", v);
}

static void json_hex(FILE *o, const uint8_t *data, size_t len)
{
    fputc('"', o);
    for (size_t i = 0; i < len; ++i)
        fprintf(o, "%02x", data[i]);
    fputc('"', o);
}

static const char *array_element_type(kind_t k)
{
    switch (k)
    {
        case K_ARR_U8:   return "u8";
        case K_ARR_U16:  return "u16";
        case K_ARR_U32:  return "u32";
        case K_ARR_U64:  return "u64";
        case K_ARR_I8:   return "i8";
        case K_ARR_I16:  return "i16";
        case K_ARR_I32:  return "i32";
        case K_ARR_I64:  return "i64";
        case K_ARR_FP32: return "fp32";
        case K_ARR_FP64: return "fp64";
        default:         return "";
    }
}

static void json_array_values(FILE *o, const op_t *op)
{
    fputc('[', o);
    for (int32_t i = 0; i < op->count; ++i)
    {
        if (i) fputs(", ", o);
        switch (op->kind)
        {
            case K_ARR_U8:   fprintf(o, "%" PRIu64, (uint64_t)((const uint8_t  *)op->arr)[i]); break;
            case K_ARR_U16:  fprintf(o, "%" PRIu64, (uint64_t)((const uint16_t *)op->arr)[i]); break;
            case K_ARR_U32:  fprintf(o, "%" PRIu64, (uint64_t)((const uint32_t *)op->arr)[i]); break;
            case K_ARR_U64:  fprintf(o, "%" PRIu64, ((const uint64_t *)op->arr)[i]); break;
            case K_ARR_I8:   fprintf(o, "%" PRId64, (int64_t)((const int8_t  *)op->arr)[i]); break;
            case K_ARR_I16:  fprintf(o, "%" PRId64, (int64_t)((const int16_t *)op->arr)[i]); break;
            case K_ARR_I32:  fprintf(o, "%" PRId64, (int64_t)((const int32_t *)op->arr)[i]); break;
            case K_ARR_I64:  fprintf(o, "%" PRId64, ((const int64_t *)op->arr)[i]); break;
            case K_ARR_FP32: json_float(o, (double)((const float  *)op->arr)[i], 1); break;
            case K_ARR_FP64: json_float(o, ((const double *)op->arr)[i], 0); break;
            default: break;
        }
    }
    fputc(']', o);
}

static void json_field(FILE *o, const char *indent, const op_t *op)
{
    fprintf(o, "%s{ ", indent);
    switch (op->kind)
    {
        case K_UNSIGNED:  fprintf(o, "\"op\": \"unsigned\", \"id\": %" PRIu32 ", \"value\": %" PRIu64, op->id, op->u); break;
        case K_SIGNED:    fprintf(o, "\"op\": \"signed\", \"id\": %" PRIu32 ", \"value\": %" PRId64, op->id, op->s); break;
        case K_BOOLEAN:   fprintf(o, "\"op\": \"boolean\", \"id\": %" PRIu32 ", \"value\": %s", op->id, op->u ? "true" : "false"); break;
        case K_FP32:      fprintf(o, "\"op\": \"fp32\", \"id\": %" PRIu32 ", \"value\": ", op->id); json_float(o, op->f, 1); break;
        case K_FP64:      fprintf(o, "\"op\": \"fp64\", \"id\": %" PRIu32 ", \"value\": ", op->id); json_float(o, op->f, 0); break;
        case K_STRING:    fprintf(o, "\"op\": \"string\", \"id\": %" PRIu32 ", \"value\": ", op->id); json_string(o, op->str); break;
        case K_BLOB:      fprintf(o, "\"op\": \"blob\", \"id\": %" PRIu32 ", \"value_hex\": ", op->id); json_hex(o, op->arr, (size_t)op->count); break;
        case K_SEQ_BEGIN: fprintf(o, "\"op\": \"sequence_begin\", \"id\": %" PRIu32, op->id); break;
        case K_SEQ_END:   fprintf(o, "\"op\": \"sequence_end\""); break;
        default:
            fprintf(o, "\"op\": \"array\", \"id\": %" PRIu32 ", \"element_type\": \"%s\", \"values\": ",
                    op->id, array_element_type(op->kind));
            json_array_values(o, op);
            break;
    }
    fputs(" }", o);
}

/* emit one full vector (replay + structure + bytes) *************************/

static int g_first_vector = 1;

/* Capability tags: the optional library features a vector needs to encode/decode.
 * A build compiled without a feature skips vectors that require it. Mirrors the
 * SOFAB_DISABLE_* flags. */
#define REQ_FIXLEN   (1u << 0) /* fp32/fp64/string/blob (and fixlen arrays) */
#define REQ_ARRAY    (1u << 1) /* any array field */
#define REQ_SEQUENCE (1u << 2) /* nested sequences */
#define REQ_FP64     (1u << 3) /* 64-bit float (implies REQ_FIXLEN) */
#define REQ_INT64    (1u << 4) /* a value/id outside the 32-bit value range */

/* Largest field id a 32-bit (SOFAB_DISABLE_INT64_SUPPORT) build can encode:
 * the (id<<3)|type header is a varint accumulated in a 32-bit value. */
#define REQ_ID32_MAX (UINT32_MAX >> 3)

static int value_needs_int64_u(uint64_t v) { return v > UINT32_MAX; }
static int value_needs_int64_i(int64_t v)  { return v > INT32_MAX || v < INT32_MIN; }

/* Derive the capability mask a vector requires from its ops + values + ids. */
static uint32_t compute_requires(const oplist_t *l)
{
    uint32_t req = 0;
    for (size_t i = 0; i < l->n; ++i)
    {
        const op_t *op = &l->ops[i];

        if (op->id > REQ_ID32_MAX) req |= REQ_INT64;

        switch (op->kind)
        {
            case K_UNSIGNED:  if (value_needs_int64_u(op->u)) req |= REQ_INT64; break;
            case K_SIGNED:    if (value_needs_int64_i(op->s)) req |= REQ_INT64; break;
            case K_BOOLEAN:   break;
            case K_FP32:      req |= REQ_FIXLEN; break;
            case K_FP64:      req |= REQ_FIXLEN | REQ_FP64; break;
            case K_STRING:    req |= REQ_FIXLEN; break;
            case K_BLOB:      req |= REQ_FIXLEN; break;
            case K_SEQ_BEGIN: req |= REQ_SEQUENCE; break;
            case K_SEQ_END:   break;
            case K_ARR_FP32:  req |= REQ_ARRAY | REQ_FIXLEN; break;
            case K_ARR_FP64:  req |= REQ_ARRAY | REQ_FIXLEN | REQ_FP64; break;
            default: /* integer arrays K_ARR_U8..K_ARR_I64 */
                req |= REQ_ARRAY;
                for (int32_t k = 0; k < op->count; ++k)
                {
                    if (op->kind == K_ARR_U64 && value_needs_int64_u(((const uint64_t *)op->arr)[k])) req |= REQ_INT64;
                    else if (op->kind == K_ARR_I64 && value_needs_int64_i(((const int64_t *)op->arr)[k])) req |= REQ_INT64;
                }
                break;
        }
    }
    return req;
}

static void emit_requires(FILE *o, uint32_t req)
{
    if (!req) return;
    static const struct { uint32_t bit; const char *name; } tags[] = {
        { REQ_FIXLEN, "fixlen" }, { REQ_ARRAY, "array" }, { REQ_SEQUENCE, "sequence" },
        { REQ_FP64, "fp64" }, { REQ_INT64, "int64" },
    };
    fprintf(o, "      \"requires\": [");
    int first = 1;
    for (size_t i = 0; i < sizeof(tags) / sizeof(tags[0]); ++i)
        if (req & tags[i].bit)
        {
            fprintf(o, "%s\"%s\"", first ? "" : ", ", tags[i].name);
            first = 0;
        }
    fprintf(o, "],\n");
}

/*!
 * Emit one vector. @p skip_ids (optional, may be NULL) lists field ids a
 * receiver is expected to skip during decoding — it drives the harness's
 * "skip-ids" scenario (simulating optional fields the receiver ignores). It is
 * purely decode-side metadata and does not affect the encoded bytes.
 *
 * A "requires" array (derived from the ops/values/ids) is emitted when the
 * vector needs optional features, so a build compiled without a feature can
 * skip the vectors it cannot handle.
 */
/*
 * A leaf field equal to its type default (zero / empty) is omitted by a
 * sparse-canonical encoder (MESSAGE_SPEC S2). A SEQUENCE is always framed, so
 * seq_begin/seq_end and any non-default child survive; only default leaves drop.
 */
static int is_default_leaf(const op_t *op)
{
    switch (op->kind)
    {
        case K_UNSIGNED:
        case K_BOOLEAN:   return op->u == 0;
        case K_SIGNED:    return op->s == 0;
        case K_FP32:
        case K_FP64:      return op->f == 0.0;
        case K_STRING:    return op->str[0] == '\0';
        case K_BLOB:
        case K_ARR_U8:  case K_ARR_U16: case K_ARR_U32: case K_ARR_U64:
        case K_ARR_I8:  case K_ARR_I16: case K_ARR_I32: case K_ARR_I64:
        case K_ARR_FP32: case K_ARR_FP64:
                          return op->count == 0;
        default:          return 0; /* K_SEQ_BEGIN / K_SEQ_END: never omitted */
    }
}

static void emit_vector_skip(FILE *o, const char *name, const char *group,
                             const char *desc, const oplist_t *l,
                             const uint32_t *skip_ids, size_t nskip)
{
    uint8_t buffer[1024];
    sofab_ostream_t os;
    sofab_ostream_init(&os, buffer, sizeof(buffer), 0, NULL, NULL);

    for (size_t i = 0; i < l->n; ++i)
    {
        if (replay(&os, &l->ops[i]) != SOFAB_RET_OK)
        {
            fprintf(stderr, "encode failed in vector '%s' at op %zu\n", name, i);
            return;
        }
    }
    size_t used = sofab_ostream_flush(&os);

    /*
     * Sparse-canonical form: replay again, omitting every leaf op equal to its
     * type default; sequences stay framed. This is the byte-exact target for a
     * sparse encoder (the generated non-C backends), while "serialized" (dense)
     * remains the primitive-layer ground truth and the decoder's skip input.
     */
    uint8_t sbuffer[1024];
    sofab_ostream_t sos;
    sofab_ostream_init(&sos, sbuffer, sizeof(sbuffer), 0, NULL, NULL);
    for (size_t i = 0; i < l->n; ++i)
    {
        if (is_default_leaf(&l->ops[i])) continue;
        if (replay(&sos, &l->ops[i]) != SOFAB_RET_OK)
        {
            fprintf(stderr, "sparse encode failed in vector '%s' at op %zu\n", name, i);
            return;
        }
    }
    size_t sused = sofab_ostream_flush(&sos);

    if (!g_first_vector) fputs(",\n", o);
    g_first_vector = 0;

    fprintf(o, "    {\n");
    fprintf(o, "      \"name\": ");        json_string(o, name);  fputs(",\n", o);
    fprintf(o, "      \"group\": ");       json_string(o, group); fputs(",\n", o);
    fprintf(o, "      \"description\": "); json_string(o, desc);  fputs(",\n", o);
    fprintf(o, "      \"offset\": 0,\n");
    emit_requires(o, compute_requires(l));
    if (skip_ids && nskip)
    {
        fprintf(o, "      \"skip_ids\": [");
        for (size_t i = 0; i < nskip; ++i)
            fprintf(o, "%s%" PRIu32, i ? ", " : "", skip_ids[i]);
        fprintf(o, "],\n");
    }
    fprintf(o, "      \"fields\": [\n");
    for (size_t i = 0; i < l->n; ++i)
    {
        json_field(o, "        ", &l->ops[i]);
        fputs(i + 1 < l->n ? ",\n" : "\n", o);
    }
    fprintf(o, "      ],\n");
    fprintf(o, "      \"serialized\": { \"length\": %zu, \"hex\": ", used);
    json_hex(o, buffer, used);
    fprintf(o, " },\n");
    fprintf(o, "      \"serialized_sparse\": { \"length\": %zu, \"hex\": ", sused);
    json_hex(o, sbuffer, sused);
    fprintf(o, " }\n");
    fprintf(o, "    }");
}

static void emit_vector(FILE *o, const char *name, const char *group,
                        const char *desc, const oplist_t *l)
{
    emit_vector_skip(o, name, group, desc, l, NULL, 0);
}

/* helper to run a single-call builder **************************************
 *
 * `call` must be a single function-call expression (its commas are protected
 * by parentheses); multi-statement messages are built with explicit blocks. */

#define EMIT(o, name, group, desc, call)        \
    do {                                        \
        oplist_t l = {0};                       \
        call;                                   \
        emit_vector(o, name, group, desc, &l);  \
    } while (0)

/* the vectors ***************************************************************/

static void emit_all(FILE *o)
{
    /* --- unsigned varint ladder (test_write_unsigned_*) --- */
    static const struct { const char *name; uint64_t value; } ladder[] = {
        {"unsigned_0",                  UINT64_C(0x0)},
        {"unsigned_0x7F",               UINT64_C(0x7F)},
        {"unsigned_0x80",               UINT64_C(0x80)},
        {"unsigned_0x3FFF",             UINT64_C(0x3FFF)},
        {"unsigned_0x4000",             UINT64_C(0x4000)},
        {"unsigned_0x1FFFFF",           UINT64_C(0x1FFFFF)},
        {"unsigned_0x200000",           UINT64_C(0x200000)},
        {"unsigned_0xFFFFFFF",          UINT64_C(0xFFFFFFF)},
        {"unsigned_0x10000000",         UINT64_C(0x10000000)},
        {"unsigned_0x7FFFFFFFF",        UINT64_C(0x7FFFFFFFF)},
        {"unsigned_0x800000000",        UINT64_C(0x800000000)},
        {"unsigned_0x3FFFFFFFFFF",      UINT64_C(0x3FFFFFFFFFF)},
        {"unsigned_0x40000000000",      UINT64_C(0x40000000000)},
        {"unsigned_0x1FFFFFFFFFFFF",    UINT64_C(0x1FFFFFFFFFFFF)},
        {"unsigned_0x2000000000000",    UINT64_C(0x2000000000000)},
        {"unsigned_0xFFFFFFFFFFFFFF",   UINT64_C(0xFFFFFFFFFFFFFF)},
        {"unsigned_0x100000000000000",  UINT64_C(0x100000000000000)},
        {"unsigned_0x7FFFFFFFFFFFFFFF", UINT64_C(0x7FFFFFFFFFFFFFFF)},
        {"unsigned_0x8000000000000000", UINT64_C(0x8000000000000000)},
        {"unsigned_0xFFFFFFFFFFFFFFFF", UINT64_C(0xFFFFFFFFFFFFFFFF)},
    };
    for (size_t i = 0; i < sizeof(ladder) / sizeof(ladder[0]); ++i)
        EMIT(o, ladder[i].name, "scalar/unsigned",
             "Unsigned varint at field id 0 covering a varint length boundary.",
             op_u(&l, 0, ladder[i].value));

    /* --- field id encoding --- */
    /* (id 0 / value 0 is already covered by unsigned_0, so it is not repeated.) */
    EMIT(o, "id_max", "scalar/id", "Largest field id (SOFAB_ID_MAX = INT32_MAX) with value 0.",
         op_u(&l, 2147483647u, 0));
    EMIT(o, "id_two_byte_header", "scalar/id",
         "Field id 16 — the first id whose (id<<3)|type header needs two varint bytes.",
         op_u(&l, 16, 1));

    /* --- signed scalars --- */
    EMIT(o, "signed_min", "scalar/signed", "INT64_MIN as a zigzag signed varint.",
         op_i(&l, 0, INT64_MIN));
    EMIT(o, "signed_max", "scalar/signed", "INT64_MAX as a zigzag signed varint.",
         op_i(&l, 0, INT64_MAX));
    /* zig-zag + varint boundaries: 0->0, -1->1, 1->2, 63->126 (1 byte),
     * 64->128 (2 bytes), -64->127 (1 byte), -65->129 (2 bytes). */
    static const struct { const char *name; int64_t value; } signed_ladder[] = {
        {"signed_0",       0},
        {"signed_minus1",  -1},
        {"signed_1",       1},
        {"signed_63",      63},
        {"signed_64",      64},
        {"signed_minus64", -64},
        {"signed_minus65", -65},
    };
    for (size_t i = 0; i < sizeof(signed_ladder) / sizeof(signed_ladder[0]); ++i)
        EMIT(o, signed_ladder[i].name, "scalar/signed",
             "Signed varint covering a zig-zag / varint length boundary.",
             op_i(&l, 0, signed_ladder[i].value));

    /* --- 32-bit value-type boundaries (SOFAB_DISABLE_INT64_SUPPORT) ---
     * These all fit in 32 bits, so they carry no "int64" requirement and run in
     * EVERY build. In the default 64-bit build they add boundary coverage; in a
     * 32-bit (no-int64) build they are the extreme min/max values, since the
     * 64-bit signed_min/max, id_max and large unsigned ladder steps are skipped
     * there. */
    EMIT(o, "unsigned_u32_max", "scalar/unsigned",
         "UINT32_MAX — the largest unsigned a 32-bit (no-int64) build can encode.",
         op_u(&l, 0, UINT32_MAX));
    EMIT(o, "signed_i32_min", "scalar/signed",
         "INT32_MIN — the most-negative signed a 32-bit (no-int64) build can encode.",
         op_i(&l, 0, INT32_MIN));
    EMIT(o, "signed_i32_max", "scalar/signed",
         "INT32_MAX — the largest signed a 32-bit (no-int64) build can encode.",
         op_i(&l, 0, INT32_MAX));
    EMIT(o, "id_max_32bit", "scalar/id",
         "Largest field id a 32-bit (no-int64) build can encode (UINT32_MAX >> 3).",
         op_u(&l, UINT32_MAX >> 3, 0));

    /* --- boolean (test_write_boolean) --- */
    EMIT(o, "boolean_true", "scalar/boolean", "Boolean true encoded as unsigned 1.",
         op_bool(&l, 0, 1));
    EMIT(o, "boolean_false", "scalar/boolean", "Boolean false encoded as unsigned 0.",
         op_bool(&l, 0, 0));

    /* --- floating point scalars (test_write_fp32 / _fp64) --- */
    EMIT(o, "fp32", "scalar/float", "32-bit float 3.1415f as a little-endian fixed-length field.",
         op_f32(&l, 0, 3.1415f));
    EMIT(o, "fp64", "scalar/float", "64-bit float (double)3.14159265f as a little-endian fixed-length field.",
         op_f64(&l, 0, (double)3.14159265f));

    /* --- strings (test_write_string / _empty) --- */
    EMIT(o, "string", "scalar/string", "UTF-8 string field.",
         op_str(&l, 0, "Hello Couch!"));
    EMIT(o, "string_empty", "scalar/string", "Empty string field.",
         op_str(&l, 0, ""));
    EMIT(o, "string_16", "scalar/string",
         "16-byte string — the fixlen length header crosses into two varint bytes.",
         op_str(&l, 0, "0123456789abcdef"));

    /* --- blobs (test_write_blob / _empty) --- */
    {
        static const uint8_t blob[] = {0x01, 0x02, 0x03, 0x04, 0x05};
        EMIT(o, "blob", "scalar/blob", "Binary blob field.",
             op_blob(&l, 0, blob, (int32_t)sizeof(blob)));
    }
    EMIT(o, "blob_empty", "scalar/blob", "Empty blob field.",
         op_blob(&l, 0, NULL, 0));

    /* --- optional fields a receiver may skip (drives the skip-ids scenario) --- */
    {
        static const uint32_t skip[] = {2, 4};
        oplist_t l = {0};
        op_u(&l, 1, 100);
        op_i(&l, 2, -200);
        op_str(&l, 3, "keep");
        op_f32(&l, 4, 1.5f);
        op_u(&l, 5, 300);
        emit_vector_skip(o, "optional_scalars", "skip",
            "Five scalars; a receiver ignoring optional ids 2 and 4 must still read 1, 3, 5.",
            &l, skip, sizeof(skip) / sizeof(skip[0]));
    }
    {
        /* One field of every wire type, each (except the two kept anchors)
         * marked optional. Exercises the decoder skipping every wire type —
         * scalar, string, blob, array and a whole sub-sequence — and resuming
         * correctly between them. */
        static const uint8_t  blob[] = {0x01, 0x02, 0x03};
        static const uint32_t arr[]  = {10, 20, 30};
        static const uint32_t skip[] = {2, 3, 4, 5, 6, 7, 8, 9};
        oplist_t l = {0};
        op_u  (&l, 1, 100);                 /* keep: anchor before */
        op_i  (&l, 2, -200);                /* skip: signed        */
        op_bool(&l, 3, 1);                  /* skip: boolean       */
        op_f32(&l, 4, 1.5f);               /* skip: fp32          */
        op_f64(&l, 5, 2.5);                /* skip: fp64          */
        op_str(&l, 6, "skip me");          /* skip: string        */
        op_blob(&l, 7, blob, (int32_t)sizeof(blob)); /* skip: blob */
        op_arr(&l, K_ARR_U32, 8, arr, 3);  /* skip: array         */
        op_seqb(&l, 9);                     /* skip: whole sub-sequence */
            op_u(&l, 0, 1);
            op_str(&l, 1, "ignored");
        op_seqe(&l);
        op_u  (&l, 10, 300);               /* keep: anchor after  */
        emit_vector_skip(o, "skip_all_wire_types", "skip",
            "Every wire type as an optional field; a receiver skipping ids 2-9 "
            "must still read the id 1 and id 10 anchors.",
            &l, skip, sizeof(skip) / sizeof(skip[0]));
    }

    /* --- integer arrays (test_write_array_of_*) --- */
    {
        static const uint32_t a[] = {1, 2, 3, 0x80000000u, UINT32_MAX};
        EMIT(o, "array_unsigned_u32", "array/integer", "Array of unsigned values (u32 input).",
             op_arr(&l, K_ARR_U32, 0, a, 5));
    }
    {
        static const int32_t a[] = {-1, -2, -3, INT32_MIN, INT32_MAX};
        EMIT(o, "array_signed_i32", "array/integer", "Array of signed values (i32 input).",
             op_arr(&l, K_ARR_I32, 0, a, 5));
    }
    {
        static const int8_t a[] = {-1, -2, -3, INT8_MIN, INT8_MAX};
        EMIT(o, "array_i8", "array/integer", "Array of i8.", op_arr(&l, K_ARR_I8, 0, a, 5));
    }
    {
        static const uint8_t a[] = {1, 2, 3, 0, UINT8_MAX};
        EMIT(o, "array_u8", "array/integer", "Array of u8.", op_arr(&l, K_ARR_U8, 0, a, 5));
    }
    {
        static const int16_t a[] = {-1, -2, -3, INT16_MIN, INT16_MAX};
        EMIT(o, "array_i16", "array/integer", "Array of i16.", op_arr(&l, K_ARR_I16, 0, a, 5));
    }
    {
        static const uint16_t a[] = {1, 2, 3, 0, UINT16_MAX};
        EMIT(o, "array_u16", "array/integer", "Array of u16.", op_arr(&l, K_ARR_U16, 0, a, 5));
    }
    {
        static const uint64_t a[] = {1, 2, 3, 0, UINT64_MAX};
        EMIT(o, "array_u64", "array/integer", "Array of u64.", op_arr(&l, K_ARR_U64, 0, a, 5));
    }
    {
        static const int64_t a[] = {-1, -2, -3, INT64_MIN, INT64_MAX};
        EMIT(o, "array_i64", "array/integer", "Array of i64.", op_arr(&l, K_ARR_I64, 0, a, 5));
    }
    {
        /* 200 elements: the element-count varint crosses into two bytes (>127). */
        static uint8_t a[200];
        for (int i = 0; i < 200; ++i) a[i] = (uint8_t)(i * 7 + 1);
        EMIT(o, "array_u8_large", "array/integer",
             "200-element u8 array — the element count crosses into two varint bytes.",
             op_arr(&l, K_ARR_U8, 0, a, 200));
    }

    /* --- float arrays (test_write_array_of_fp32 / _fp64) --- */
    {
        static const float a[] = {1.0f, 2.0f, 3.0f, -FLT_MAX, FLT_MAX};
        EMIT(o, "array_fp32", "array/float", "Array of fp32.", op_arr(&l, K_ARR_FP32, 0, a, 5));
    }
    {
        static const double a[] = {1.0, 2.0, 3.0, -DBL_MAX, DBL_MAX};
        EMIT(o, "array_fp64", "array/float", "Array of fp64.", op_arr(&l, K_ARR_FP64, 0, a, 5));
    }
    /* NaN is intentionally excluded: its bit pattern is not portable across
     * architectures (the C test checks NaN separately for the same reason). */
    {
        static const float a[] = {0.0f, -0.0f, INFINITY, -INFINITY};
        EMIT(o, "array_fp32_specials", "array/float",
             "Array of fp32 special values: +0, -0, +inf, -inf (NaN excluded).",
             op_arr(&l, K_ARR_FP32, 0, a, 4));
    }
    {
        static const double a[] = {0.0, -0.0, INFINITY, -INFINITY};
        EMIT(o, "array_fp64_specials", "array/float",
             "Array of fp64 special values: +0, -0, +inf, -inf (NaN excluded).",
             op_arr(&l, K_ARR_FP64, 0, a, 4));
    }

    /* --- zero-count arrays (§4.7/§4.8): [hdr][count=0], no payload --- */
    {
        static const uint32_t a[1] = {0};
        EMIT(o, "array_unsigned_u32_empty", "array/integer",
             "Zero-count unsigned array — [hdr][count=0], no elements (§4.7).",
             op_arr(&l, K_ARR_U32, 0, a, 0));
    }
    {
        static const int32_t a[1] = {0};
        EMIT(o, "array_signed_i32_empty", "array/integer",
             "Zero-count signed array — [hdr][count=0], no elements (§4.7).",
             op_arr(&l, K_ARR_I32, 0, a, 0));
    }
    {
        static const float a[1] = {0};
        EMIT(o, "array_fp32_empty", "array/float",
             "Zero-count fixlen array — [hdr][count=0][fixlen_word], no payload; the "
             "fixlen_word is always present so fp32/fp64 stay distinguishable (§4.8).",
             op_arr(&l, K_ARR_FP32, 0, a, 0));
    }
    {
        static const double a[1] = {0};
        EMIT(o, "array_fp64_empty", "array/float",
             "Zero-count fixlen array (fp64) — [hdr][count=0][fixlen_word], no payload; "
             "the fixlen_word keeps it distinct from an empty fp32 array (§4.8).",
             op_arr(&l, K_ARR_FP64, 0, a, 0));
    }

    /* --- empty / edge sequences --- */
    {
        oplist_t l = {0};
        op_seqb(&l, 1);
        op_seqe(&l);
        emit_vector(o, "empty_sequence", "sequence", "A sequence with no fields.", &l);
    }
    {
        oplist_t l = {0};
        op_seqb(&l, 1);
            op_seqb(&l, 2);
            op_seqe(&l);
        op_seqe(&l);
        emit_vector(o, "nested_empty_sequences", "sequence",
                    "A sequence whose only content is an empty sub-sequence.", &l);
    }
    {
        static const uint32_t skip[] = {1};
        oplist_t l = {0};
        op_u(&l, 0, 7);
        op_seqb(&l, 1);
        op_seqe(&l);
        op_i(&l, 2, -7);
        emit_vector_skip(o, "empty_sequence_between_fields", "sequence",
                    "An empty sequence between two scalars; the decoder must resume after it.",
                    &l, skip, 1);
    }

    /* --- nested sequences (test_write_nested_sequence*) --- */
    {
        oplist_t l = {0};
        op_u(&l, 0, 42);
        op_seqb(&l, 1);
            op_u(&l, 0, 42);
            op_i(&l, 2, -42);
        op_seqe(&l);
        op_i(&l, 2, -42);
        static const uint32_t skip[] = {1};
        emit_vector_skip(o, "nested_sequence", "sequence",
                    "A scalar, a nested sequence, then a scalar.", &l, skip, 1);
    }

    {
        static const int32_t a[] = {-42, -43, -44};
        oplist_t l = {0};
        op_u(&l, 0, 42);
        op_seqb(&l, 3);
            op_u(&l, 0, 42);
            op_arr(&l, K_ARR_I32, 3, a, 3);
        op_seqe(&l);
        op_i(&l, 2, -42);
        static const uint32_t skip[] = {3};
        emit_vector_skip(o, "nested_sequence_with_array", "sequence",
                    "A nested sequence containing a signed array.", &l, skip, 1);
    }

    {
        oplist_t l = {0};
        op_u(&l, 0, 42);
        for (int i = 0; i < 10; ++i)
        {
            op_seqb(&l, 1);
            op_u(&l, 0, 42);
            op_i(&l, 2, -42);
        }
        for (int i = 0; i < 10; ++i)
            op_seqe(&l);
        op_i(&l, 2, -42);
        static const uint32_t skip[] = {1};
        emit_vector_skip(o, "nested_sequence_multilevel", "sequence",
                    "Ten levels of nested sequences.", &l, skip, 1);
    }

    {
        /* Multi-depth nested sequences with skipping at several levels: a scalar
         * skipped at depth 2 (id 5) and a whole sub-tree skipped at depth 3
         * (id 7, which itself nests a depth-4 sequence). Verifies the decoder
         * resumes correctly after a skipped field and after a skipped sub-tree,
         * at every level on the way back out. */
        static const int32_t arr[] = {-1, -2, -3};
        static const uint32_t skip[] = {5, 7};
        oplist_t l = {0};
        op_u(&l, 1, 10);                       /* depth 0: keep */
        op_seqb(&l, 2);                         /* depth 1: descend */
            op_u(&l, 3, 20);                    /*   keep */
            op_seqb(&l, 4);                     /*   depth 2: descend */
                op_i(&l, 5, -30);              /*     skip (scalar at depth 2) */
                op_str(&l, 6, "deep");         /*     keep */
                op_seqb(&l, 7);                /*     depth 3: skip whole sub-tree */
                    op_arr(&l, K_ARR_I32, 8, arr, 3);
                    op_seqb(&l, 9);            /*       depth 4 (skipped via parent) */
                        op_f64(&l, 10, 1.5);
                    op_seqe(&l);
                op_seqe(&l);
                op_u(&l, 11, 40);             /*     keep: resume after skipped sub-tree */
            op_seqe(&l);
            op_i(&l, 12, -60);                /*   keep: resume at depth 1 */
        op_seqe(&l);
        op_u(&l, 13, 70);                     /* depth 0: keep, resume after deep sequence */
        emit_vector_skip(o, "nested_sequence_deep_skip", "sequence",
                    "Multi-depth nested sequences skipping a depth-2 scalar (id 5) "
                    "and a whole depth-3 sub-tree (id 7).", &l, skip,
                    sizeof(skip) / sizeof(skip[0]));
    }

    /* --- wrapper-array string elements: per-element sparse omission ---------
     * A wrapper-sequence array (array of string) is itself a sequence, so its
     * elements (id = index) follow the per-field rule (MESSAGE_SPEC S2): a
     * string element equal to its element default (empty) is omitted, leaving an
     * id gap the decoder restores; trailing default elements collapse. The
     * `serialized_sparse` here is auto-derived (is_default_leaf drops the empty
     * string ops), so these pin the element-level sparse wire the non-C backends
     * must produce. The sequence is always framed even when fully default. */
    {
        oplist_t l = {0};
        op_seqb(&l, 0);
            op_str(&l, 0, "a");
            op_str(&l, 1, "");   /* default -> dropped, leaves an id gap  */
            op_str(&l, 2, "c");
        op_seqe(&l);
        emit_vector(o, "array_string_gap", "array/string",
                    "Array of string with a default (empty) element in the middle: "
                    "sparse omits id 1, leaving a gap the decoder restores.", &l);
    }
    {
        oplist_t l = {0};
        op_seqb(&l, 0);
            op_str(&l, 0, "a");
            op_str(&l, 1, "");   /* trailing default -> collapses  */
        op_seqe(&l);
        emit_vector(o, "array_string_trailing_default", "array/string",
                    "Array of string with a trailing default element: sparse drops "
                    "it, so [\"a\",\"\"] encodes exactly like [\"a\"].", &l);
    }
    {
        oplist_t l = {0};
        op_seqb(&l, 0);
            op_str(&l, 0, "");
            op_str(&l, 1, "");
        op_seqe(&l);
        emit_vector(o, "array_string_all_default", "array/string",
                    "Array of only default (empty) string elements: every element "
                    "drops, so it encodes as the empty wrapper sequence.", &l);
    }
    {
        oplist_t l = {0};
        op_seqb(&l, 0);
            op_str(&l, 0, "");   /* leading default -> gap at id 0  */
            op_str(&l, 1, "x");
            op_str(&l, 2, "");   /* trailing default -> collapses    */
        op_seqe(&l);
        emit_vector(o, "array_string_leading_default", "array/string",
                    "Array of string with leading and trailing default elements: "
                    "the leading one leaves an id gap, the trailing one collapses.", &l);
    }

    /* --- full scale composite message (test_write_full_scale_example) --- */
    {
        static const uint8_t  blob[]  = {0xDE, 0xAD, 0xBE, 0xEF};
        static const uint8_t  au8[]   = {0, 64, 128, 191, 255};
        static const int8_t   ai8[]   = {-128, -64, 0, 63, 127};
        static const uint16_t au16[]  = {0, 16384, 32768, 49151, 65535};
        static const int16_t  ai16[]  = {-32768, -16384, 0, 16383, 32767};
        static const uint32_t au32[]  = {0u, 1073741824u, 2147483648u, 3221225471u, 4294967295u};
        static const int32_t  ai32[]  = {-2147483647 - 1, -1073741824, 0, 1073741823, 2147483647};
        static const uint64_t au64[]  = {0ull, 4611686018427387904ull, 9223372036854775808ull, 13835058055282163711ull, 18446744073709551615ull};
        static const int64_t  ai64[]  = {-9223372036854775807ll, -4611686018427387904ll, 0ll, 4611686018427387903ll, 9223372036854775807ll};
        static const float    af32[]  = {1.0f, 2.0f, 3.0f, -FLT_MAX, FLT_MAX};
        static const double   af64[]  = {1.0, 2.0, 3.0, -DBL_MAX, DBL_MAX};

        oplist_t l = {0};

        op_u(&l, 0, 200);
        op_i(&l, 1, -100);
        op_u(&l, 2, 50000);
        op_i(&l, 3, -20000);
        op_u(&l, 4, 3000000000ull);
        op_i(&l, 5, -1000000000ll);
        op_u(&l, 6, 10000000000000ull);
        op_i(&l, 7, -5000000000000ll);

        op_seqb(&l, 10);
            op_f32(&l, 0, 3.14f);
            op_f64(&l, 1, 3.14159265);
            op_str(&l, 2, "Hello, World!");
            op_blob(&l, 3, blob, (int32_t)sizeof(blob));
        op_seqe(&l);

        op_seqb(&l, 100);
            op_arr(&l, K_ARR_U8,  0, au8,  5);
            op_arr(&l, K_ARR_I8,  1, ai8,  5);
            op_arr(&l, K_ARR_U16, 2, au16, 5);
            op_arr(&l, K_ARR_I16, 3, ai16, 5);
            op_arr(&l, K_ARR_U32, 4, au32, 5);
            op_arr(&l, K_ARR_I32, 5, ai32, 5);
            op_arr(&l, K_ARR_U64, 6, au64, 5);
            op_arr(&l, K_ARR_I64, 7, ai64, 5);
            op_seqb(&l, 10);
                op_arr(&l, K_ARR_FP32, 0, af32, 5);
                op_arr(&l, K_ARR_FP64, 1, af64, 5);
            op_seqe(&l);
        op_seqe(&l);

        op_seqb(&l, 200);
            op_str(&l, 0, "Hello, Sofab!");
            op_str(&l, 1, "");
            op_str(&l, 2, "1234567890");
            op_str(&l, 3, "\xC3\xA4\xC3\xB6\xC3\xBC\xC3\x84\xC3\x96\xC3\x9C\xC3\x9F"); /* äöüÄÖÜß */
            op_str(&l, 4, "This_is_a_very_long_test_string_with_!@#$%^&*()_+-=[]{}");
        op_seqe(&l);

        /* Skip a top-level scalar (id 1) and a whole top-level sub-sequence
         * (id 100); id 1 also appears inside sequences, exercising id-based
         * skipping at multiple nesting levels. */
        static const uint32_t skip[] = {1, 100};
        emit_vector_skip(o, "full_scale_example", "composite",
                    "Large message mixing scalars, nested sequences, "
                    "integer/float arrays and strings.", &l, skip, 2);
    }
}

int main(void)
{
    FILE *o = stdout;

    fprintf(o, "{\n");
    fprintf(o, "  \"format\": \"sofabuffers-test-vectors\",\n");
    fprintf(o, "  \"version\": 1,\n");
    fprintf(o, "  \"description\": \"SofaBuffers wire-format test vectors generated from the C encoder. "
               "Each vector lists the message structure (ordered encode operations and their values) "
               "and the exact bytes the encoder produced.\",\n");
    fprintf(o, "  \"notes\": {\n");
    fprintf(o, "    \"byte_order\": \"little-endian\",\n");
    fprintf(o, "    \"serialized.hex\": \"lowercase hex of the full (dense) message; primitive-layer ground truth and the decoder's skip input\",\n");
    fprintf(o, "    \"serialized_sparse.hex\": \"lowercase hex of the sparse-canonical message (MESSAGE_SPEC S2): every leaf field equal to its type default is omitted, sequences stay framed; byte-exact target for a sparse encoder\",\n");
    fprintf(o, "    \"integers\": \"decimal JSON number literals (full u64/i64 range)\",\n");
    fprintf(o, "    \"floats\": \"finite values as JSON numbers; +/-infinity as the strings 'inf'/'-inf'\",\n");
    fprintf(o, "    \"blob.value_hex\": \"lowercase hex of the blob payload\",\n");
    fprintf(o, "    \"array.element_type\": \"input element width/type fed to the encoder (u8..u64, i8..i64, fp32, fp64)\"\n");
    fprintf(o, "  },\n");
    fprintf(o, "  \"vectors\": [\n");

    emit_all(o);

    fprintf(o, "\n  ]\n");
    fprintf(o, "}\n");
    return 0;
}

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
    int         last;  /* op sits at a wrapper array's LAST element index */
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
/*!
 * @brief Mark the op just pushed as a wrapper array's LAST element.
 *
 * MESSAGE_SPEC §2/§5.1: a wrapper array carries no length, so the decoded length
 * is *highest present id + 1*. Nothing that carries it may be elided: the element
 * at the last index is ALWAYS written -- a `string`/`blob` leaf as its (possibly
 * default) value, a sequence element as an empty frame -- while every INTERIOR
 * element equal to its default is omitted, leaf and sequence element alike.
 *
 * The op list has no schema, so the position cannot be inferred; it is recorded
 * here, on the last element's op (a leaf, or the closer of a sequence element).
 * The sparse pass honours it -- it neither drops a marked leaf nor lets a marked
 * frame collapse -- and the dense pass writes everything anyway.
 */
static void op_last (oplist_t *l)              { l->ops[l->n - 1].last = 1; }
/*! @brief Close a sequence element that sits at the array's last index. */
static void op_seqe_last (oplist_t *l)         { push(l, (op_t){.kind = K_SEQ_END, .last = 1}); }

/* replay through the real encoder *******************************************/

/*!
 * @brief Replay one op into an output stream.
 *
 * @param os        Output stream.
 * @param op        Op to replay.
 * @param lazy_seq  Open a sequence with the lazy primitive, so an all-default one
 *                  is omitted rather than framed empty (MESSAGE_SPEC §2). Set for
 *                  the sparse-canonical pass only; the dense pass is the
 *                  primitive-layer ground truth and always frames.
 */
static sofab_ret_t replay(sofab_ostream_t *os, const op_t *op, int lazy_seq)
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
        case K_SEQ_BEGIN: return lazy_seq ? sofab_ostream_write_sequence_begin_lazy(os, op->id)
                                          : sofab_ostream_write_sequence_begin(os, op->id);
        case K_SEQ_END:   return (lazy_seq && op->last)
                                 ? sofab_ostream_write_sequence_end_keep(os)
                                 : sofab_ostream_write_sequence_end(os);
    }
    return SOFAB_RET_E_ARGUMENT;
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
        case K_SEQ_END:   fprintf(o, "\"op\": \"sequence_end\"");
                          break;
        default:
            fprintf(o, "\"op\": \"array\", \"id\": %" PRIu32 ", \"element_type\": \"%s\", \"values\": ",
                    op->id, array_element_type(op->kind));
            json_array_values(o, op);
            break;
    }
    /* MESSAGE_SPEC §2/§5.1: this op is a wrapper array's LAST element, the one a
     * sparse encoder must write whatever its value (see op_last). On a
     * sequence_end it means the element's frame survives even when empty. */
    if (op->last) fprintf(o, ", \"element\": true");
    fputs(" }", o);
}

/* raw wire assembly (for the negative UTF-8 vectors) ************************
 *
 * The invalid-UTF-8 vectors carry bytes the strict encoder would REJECT, so
 * they cannot be produced by replaying ops through it. Instead the message is
 * hand-assembled from the same wire rules (CORELIB_PLAN §4.3/§4.6): a field
 * header varint, a fixlen_word varint, then the raw (invalid) payload. */

static size_t raw_varint(uint8_t *out, uint64_t v)
{
    size_t n = 0;
    do {
        uint8_t b = v & 0x7F;
        v >>= 7;
        if (v) b |= 0x80;
        out[n++] = b;
    } while (v);
    return n;
}

/* Build the wire bytes for a single `string` field (id, STRING subtype) whose
 * payload is the raw @p len bytes at @p payload (which need not be valid UTF-8). */
static size_t build_string_msg(uint8_t *out, uint32_t id,
                               const uint8_t *payload, size_t len)
{
    size_t n = 0;
    n += raw_varint(out + n, ((uint64_t)id << 3) | (uint64_t)SOFAB_TYPE_FIXLEN);
    n += raw_varint(out + n, ((uint64_t)len << 3) | (uint64_t)SOFAB_FIXLENTYPE_STRING);
    memcpy(out + n, payload, len);
    return n + len;
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
 * sparse-canonical encoder (MESSAGE_SPEC S2). This predicate covers LEAVES only:
 * a sequence op is never dropped here, because whether its frame survives is not
 * a property of the op but of what the ops inside it turn out to be. The sparse
 * pass therefore replays every seq_begin/seq_end and lets the encoder decide --
 * it opens them LAZILY, so a sequence left without content drops itself, header
 * and end marker both (S2), while one with a surviving child stays framed.
 *
 * One position is exempt in both directions: an op marked as a wrapper array's
 * LAST element (op_last) is always written, because its presence is what carries
 * the array's length (S2/S5.1) -- a default leaf there stays, and a marked closer
 * keeps its frame.
 */
static int is_default_leaf(const op_t *op)
{
    if (op->last) return 0;

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
        if (replay(&os, &l->ops[i], 0) != SOFAB_RET_OK)
        {
            fprintf(stderr, "encode failed in vector '%s' at op %zu\n", name, i);
            return;
        }
    }
    size_t used = sofab_ostream_flush(&os);

    /*
     * Sparse-canonical form: replay again, omitting every leaf op equal to its
     * type default and opening every sequence LAZILY, so a sequence left without
     * content is omitted rather than framed empty (MESSAGE_SPEC §2 -- the
     * ≠-default test is per field and a sequence is no exception). This is the
     * byte-exact target for a sparse encoder (the generated non-C backends), while
     * "serialized" (dense) remains the primitive-layer ground truth and the
     * decoder's skip input.
     */
    uint8_t sbuffer[1024];
    sofab_ostream_t sos;
    sofab_ostream_init(&sos, sbuffer, sizeof(sbuffer), 0, NULL, NULL);
    for (size_t i = 0; i < l->n; ++i)
    {
        if (is_default_leaf(&l->ops[i])) continue;
        if (replay(&sos, &l->ops[i], 1) != SOFAB_RET_OK)
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

/* the skip matrix ***********************************************************
 *
 * A receiver skips every field it does not bind — an unknown id (MESSAGE_SPEC
 * §5.1.5) or one whose wire type contradicts the schema (§7.3) — and must resume
 * at the very next header, whatever the skipped construct was. The skip is
 * driven by the WIRE TYPE alone (CORELIB_PLAN §4.3): a varint is consumed by its
 * continuation bits, a fixlen by the length in its `fixlen_word` (§4.6), an
 * integer array element by element (§4.7), a fixlen array by
 * `count × element_length` (§4.8) and a sequence by its end marker (§4.9).
 * Those are five different length computations, and each can be off by a byte on
 * its own.
 *
 * This block is the full matrix over them: every skippable construct skipped
 * directly behind every skippable construct that IS read, each followed by an
 * unsigned ANCHOR that the receiver reads and compares. The anchor is the
 * detector — a skip that consumes one byte too few or too many lands the decoder
 * in the middle of it, and the comparison fails. One anchor wire type is enough:
 * what a resync must land on is a header, and every header is the same varint.
 *
 * The ten constructs are the eight wire types with `fixlen` split by subtype
 * (the decoder branches on it) and `sequence end` left out (it is a marker, not
 * a field a receiver can decline). `boolean` is not a construct of its own —
 * §4.4 makes it an unsigned integer on the wire, which the matrix already covers.
 *
 * Pairs are grouped into vectors by the capability set they need, so a reduced
 * build still runs the part of the matrix it can represent: a single vector
 * holding all ten constructs would carry every "requires" tag and be dropped
 * whole by every SOFAB_DISABLE_* build.
 */

typedef enum
{
    MW_UNSIGNED, MW_SIGNED,           /* tier "varint"       */
    MW_FP32, MW_STRING, MW_BLOB,      /* tier "fixlen"       */
    MW_FP64,                          /* tier "fp64"         */
    MW_ARR_U, MW_ARR_I,               /* tier "int_array"    */
    MW_ARR_F,                         /* tier "fixlen_array" */
    MW_SEQ,                           /* tier "sequence"     */
    MW_COUNT
} mwire_t;

static const char *const MW_NAME[MW_COUNT] = {
    "unsigned", "signed", "fp32", "string", "blob", "fp64",
    "array<unsigned>", "array<signed>", "array<fixlen>", "sequence",
};

/* Tiers are contiguous runs of MW_*, each with one capability set. */
static const struct { const char *name; int first, last; } MTIER[] = {
    { "varint",       MW_UNSIGNED, MW_SIGNED },
    { "fixlen",       MW_FP32,     MW_BLOB   },
    { "fp64",         MW_FP64,     MW_FP64   },
    { "int_array",    MW_ARR_U,    MW_ARR_I  },
    { "fixlen_array", MW_ARR_F,    MW_ARR_F  },
    { "sequence",     MW_SEQ,      MW_SEQ    },
};
#define MTIER_COUNT (sizeof(MTIER) / sizeof(MTIER[0]))

/* Widest vector the grouping produces: 3 (fixlen tier) x 3 (fixlen tier) pairs. */
#define MMAX_PAIRS 9
#define MMAX_SLOTS (MMAX_PAIRS * 3)

/*!
 * @brief Append one field of wire type @p w at id @p id.
 *
 * @p slot is the field's index inside the vector being built; it varies the
 * payloads so no two fields of a vector carry the same value, and indexes the
 * string pool (op_str stores the pointer, so the buffer must outlive the build).
 * Every value is non-default, which keeps the sparse column identical to the
 * dense one: the matrix is about skipping, not about omission.
 */
static void push_wire(oplist_t *l, mwire_t w, uint32_t id, size_t slot)
{
    static const uint32_t au[3] = {1000u, 2000u, 3000u};
    static const int32_t  ai[3] = {-1000, -2000, -3000};
    static const float    af[3] = {1.5f, -2.5f, 3.5f};
    static const uint8_t  bl[3] = {0xDE, 0xAD, 0xBE};
    static char strpool[MMAX_SLOTS][8];

    switch (w)
    {
        /* Multi-byte payloads throughout: a one-byte varint would hide a skip
         * that stops after the first byte. */
        case MW_UNSIGNED: op_u  (l, id, UINT64_C(1000) + slot); break;
        case MW_SIGNED:   op_i  (l, id, -(int64_t)(1000 + slot)); break;
        case MW_FP32:     op_f32(l, id, 1.5f + (float)slot); break;
        case MW_STRING:   snprintf(strpool[slot], sizeof(strpool[0]), "s%02zu", slot);
                          op_str(l, id, strpool[slot]); break;
        case MW_BLOB:     op_blob(l, id, bl, (int32_t)sizeof(bl)); break;
        case MW_FP64:     op_f64(l, id, 2.5 + (double)slot); break;
        case MW_ARR_U:    op_arr(l, K_ARR_U32,  id, au, 3); break;
        case MW_ARR_I:    op_arr(l, K_ARR_I32,  id, ai, 3); break;
        case MW_ARR_F:    op_arr(l, K_ARR_FP32, id, af, 3); break;
        /* A sequence carries content, so skipping it means skipping a frame with
         * something in it -- the empty frame is covered by
         * empty_sequence_between_fields. The child id is 0, which no skip_ids
         * list here names (skipped ids are always 3k+1), so a READ sequence keeps
         * its child. */
        case MW_SEQ:      op_seqb(l, id); op_u(l, 0, UINT64_C(7) + slot); op_seqe(l); break;
        default:          break;
    }
}

/* "unsigned, signed" — the tier's members, for the vector description. */
static void tier_members(char *buf, size_t n, size_t tier)
{
    size_t used = 0;
    buf[0] = '\0';
    for (int w = MTIER[tier].first; w <= MTIER[tier].last; ++w)
    {
        int k = snprintf(buf + used, n - used, "%s%s",
                         w == MTIER[tier].first ? "" : ", ", MW_NAME[w]);
        if (k < 0 || (size_t)k >= n - used) return;
        used += (size_t)k;
    }
}

static void emit_skip_matrix(FILE *o)
{
    for (size_t pt = 0; pt < MTIER_COUNT; ++pt)
    for (size_t st = 0; st < MTIER_COUNT; ++st)
    {
        oplist_t l = {0};
        uint32_t skip[MMAX_PAIRS];
        size_t   nskip = 0, slot = 0;
        uint32_t id = 0;

        for (int p = MTIER[pt].first; p <= MTIER[pt].last; ++p)
        for (int s = MTIER[st].first; s <= MTIER[st].last; ++s)
        {
            /* one row: read a field, skip the next, read the anchor after it.
             * Ids run 3k / 3k+1 / 3k+2, so the skipped id is never one a read
             * field (or a read sequence's child) carries. */
            push_wire(&l, (mwire_t)p, id++, slot++);
            skip[nskip++] = id;
            push_wire(&l, (mwire_t)s, id++, slot++);
            op_u(&l, id++, UINT64_C(500) + slot);
            slot++;
        }

        char pmem[96], smem[96], name[64], desc[640];
        tier_members(pmem, sizeof(pmem), pt);
        tier_members(smem, sizeof(smem), st);
        snprintf(name, sizeof(name), "skip_matrix_%s_after_%s", MTIER[st].name, MTIER[pt].name);
        snprintf(desc, sizeof(desc),
                 "Skip matrix, %zu of the 100 (read, skipped) wire-type pairs: each of {%s} is skipped "
                 "where it directly follows a {%s} field the receiver DOES read. Every skipped field is "
                 "followed by an unsigned anchor that must still decode with its exact value -- a skip "
                 "consuming one byte too few or too many is caught there (MESSAGE_SPEC S7.3, S4.3).",
                 nskip, smem, pmem);
        emit_vector_skip(o, name, "skip/matrix", desc, &l, skip, nskip);
    }
}

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
    {
        /* Zero-length payloads as the skipped field. Each is its own branch in
         * the length arithmetic: a fixlen with length 0 has a fixlen_word and no
         * payload, a zero-count integer array ends after the count (§4.7), and a
         * zero-count fixlen array still carries its fixlen_word (§4.8) -- the one
         * empty construct where the skip must consume a word it cannot infer
         * from the count. The skip matrix below uses non-empty payloads
         * throughout, so these three sit here. */
        static const uint32_t skip[] = {1, 3};
        oplist_t l = {0};
        op_u  (&l, 0, 1000);
        op_str(&l, 1, "");                  /* skip: zero-length string */
        op_u  (&l, 2, 2000);
        op_blob(&l, 3, NULL, 0);            /* skip: zero-length blob   */
        op_u  (&l, 4, 3000);
        emit_vector_skip(o, "skip_empty_fixlen_payloads", "skip",
            "Zero-length string and blob as skipped fields: the fixlen_word is there, the "
            "payload is not, and the anchors after them must still decode (S4.6).",
            &l, skip, sizeof(skip) / sizeof(skip[0]));
    }
    {
        static const uint32_t au[1] = {0};
        static const int32_t  ai[1] = {0};
        static const uint32_t skip[] = {1, 3};
        oplist_t l = {0};
        op_u  (&l, 0, 1000);
        op_arr(&l, K_ARR_U32, 1, au, 0);    /* skip: zero-count unsigned array */
        op_u  (&l, 2, 2000);
        op_arr(&l, K_ARR_I32, 3, ai, 0);    /* skip: zero-count signed array   */
        op_u  (&l, 4, 3000);
        emit_vector_skip(o, "skip_empty_int_arrays", "skip",
            "Zero-count integer arrays as skipped fields: the field ends after the count, "
            "with no elements and no fixlen_word (S4.7).",
            &l, skip, sizeof(skip) / sizeof(skip[0]));
    }
    {
        static const float  af[1] = {0};
        static const double ad[1] = {0};
        static const uint32_t skip[] = {1, 3};
        oplist_t l = {0};
        op_u  (&l, 0, 1000);
        op_arr(&l, K_ARR_FP32, 1, af, 0);   /* skip: zero-count fp32 array */
        op_u  (&l, 2, 2000);
        op_arr(&l, K_ARR_FP64, 3, ad, 0);   /* skip: zero-count fp64 array */
        op_u  (&l, 4, 3000);
        emit_vector_skip(o, "skip_empty_fixlen_arrays", "skip",
            "Zero-count fixlen arrays as skipped fields: count 0 KEEPS the fixlen_word "
            "(S4.8), so a skip that stops at the count desynchronises the anchor after it.",
            &l, skip, sizeof(skip) / sizeof(skip[0]));
    }
    {
        /* A non-empty fp64 array as the skipped field: the skip matrix uses fp32
         * arrays, whose fixlen_word says 4 -- here it says 8, so a skip that
         * assumes the element width instead of reading it desynchronises (§4.8). */
        static const double a[] = {1.5, -2.5, 3.5};
        static const uint32_t skip[] = {1};
        oplist_t l = {0};
        op_u  (&l, 0, 1000);
        op_arr(&l, K_ARR_FP64, 1, a, 3);
        op_u  (&l, 2, 2000);
        emit_vector_skip(o, "skip_fp64_array", "skip",
            "A non-empty fp64 array skipped: element length 8 comes from the fixlen_word, "
            "and count x length must be consumed exactly (S4.8).",
            &l, skip, sizeof(skip) / sizeof(skip[0]));
    }

    /* --- skipped fields whose LENGTH/COUNT needs more than one varint byte ---
     *
     * Everything above keeps the skipped payload under 128, so its fixlen_word /
     * element count fits in a single byte. A decoder that reads that varint as
     * one byte, or truncates the count at 127, passes all of them. These carry
     * 130-byte / 130-element payloads, where the length itself is a two-byte
     * varint (§4.1). */
    {
        static char  s[131];
        static uint8_t b[130];
        static const uint32_t skip[] = {1, 3};
        for (size_t i = 0; i < sizeof(s) - 1; ++i) s[i] = (char)('a' + (i % 26));
        s[sizeof(s) - 1] = '\0';
        for (size_t i = 0; i < sizeof(b); ++i) b[i] = (uint8_t)(i & 0xFF);

        oplist_t l = {0};
        op_u   (&l, 0, 1000);
        op_str (&l, 1, s);                              /* skip: 130-byte string */
        op_u   (&l, 2, 2000);
        op_blob(&l, 3, b, (int32_t)sizeof(b));          /* skip: 130-byte blob   */
        op_u   (&l, 4, 3000);
        emit_vector_skip(o, "skip_long_fixlen_payloads", "skip",
            "Skipped string and blob of 130 bytes: the fixlen_word is a TWO-byte varint, so a "
            "skip that reads the length as one byte lands mid-payload (S4.1, S4.6).",
            &l, skip, sizeof(skip) / sizeof(skip[0]));
    }
    {
        static uint32_t au[130];
        static int32_t  ai[130];
        static const uint32_t skip[] = {1, 3};
        for (size_t i = 0; i < 130; ++i) { au[i] = (uint32_t)(i + 1); ai[i] = -(int32_t)(i + 1); }

        oplist_t l = {0};
        op_u  (&l, 0, 1000);
        op_arr(&l, K_ARR_U32, 1, au, 130);   /* skip: 130-element unsigned array */
        op_u  (&l, 2, 2000);
        op_arr(&l, K_ARR_I32, 3, ai, 130);   /* skip: 130-element signed array   */
        op_u  (&l, 4, 3000);
        emit_vector_skip(o, "skip_long_int_arrays", "skip",
            "Skipped integer arrays of 130 elements: the element count is a TWO-byte varint and "
            "the elements are skipped one varint at a time (S4.7).",
            &l, skip, sizeof(skip) / sizeof(skip[0]));
    }
    {
        static float af[130];
        static const uint32_t skip[] = {1};
        for (size_t i = 0; i < 130; ++i) af[i] = 1.5f + (float)i;

        oplist_t l = {0};
        op_u  (&l, 0, 1000);
        op_arr(&l, K_ARR_FP32, 1, af, 130);  /* skip: 520 payload bytes */
        op_u  (&l, 2, 2000);
        emit_vector_skip(o, "skip_long_fixlen_array", "skip",
            "Skipped fixlen array of 130 fp32 elements: a two-byte count, and 520 payload bytes "
            "to consume as count x element_length (S4.8).",
            &l, skip, sizeof(skip) / sizeof(skip[0]));
    }
    {
        /* A skipped field whose HEADER is a three-byte varint: the id is part of
         * the same varint as the type (§4.3), so a decoder that stops early
         * misreads both. */
        static const uint32_t skip[] = {100000};
        oplist_t l = {0};
        op_u(&l, 0, 1000);
        op_i(&l, 100000, -4242);
        op_u(&l, 100001, 2000);
        emit_vector_skip(o, "skip_large_id", "skip",
            "A skipped field at id 100000: its (id << 3 | type) header is a three-byte varint "
            "(S4.3), and the anchor behind it carries an equally wide header.",
            &l, skip, sizeof(skip) / sizeof(skip[0]));
    }

    /* --- where the skipped field SITS: message start, message end, and the last
     * position inside a sequence. Everything above puts an anchor behind every
     * skip, so the decoder never had to end a message, or close a sequence,
     * directly on a skip -- the moment where a skip that left the state machine
     * mid-field shows up as INCOMPLETE instead of a clean boundary (§5.2). The
     * end-of-message case is split per wire type because each leaves the decoder
     * in a different state to return from. --- */
    {
        static const uint32_t skip[] = {0, 2};
        oplist_t l = {0};
        op_i(&l, 0, -1234);   /* skip: the message's FIRST field  */
        op_u(&l, 1, 1000);
        op_u(&l, 2, 2000);    /* skip: the message's LAST field   */
        emit_vector_skip(o, "skip_at_message_edges", "skip",
            "The first and the last field of the message are both skipped: nothing precedes the "
            "one, nothing follows the other, and the message must still end at a clean boundary.",
            &l, skip, sizeof(skip) / sizeof(skip[0]));
    }
    {
        static const uint32_t skip[] = {1};
        oplist_t l = {0};
        op_u  (&l, 0, 1000);
        op_str(&l, 1, "trailing");   /* skip: a fixlen at end of message */
        emit_vector_skip(o, "skip_fixlen_at_message_end", "skip",
            "A fixlen field is the last thing in the message and is skipped: the decoder must "
            "leave the payload state and reach the message boundary (S4.6, S5.2).",
            &l, skip, sizeof(skip) / sizeof(skip[0]));
    }
    {
        static const uint32_t a[] = {10, 20, 30};
        static const uint32_t skip[] = {1};
        oplist_t l = {0};
        op_u  (&l, 0, 1000);
        op_arr(&l, K_ARR_U32, 1, a, 3);   /* skip: an array at end of message */
        emit_vector_skip(o, "skip_int_array_at_message_end", "skip",
            "An integer array is the last thing in the message and is skipped: the element loop "
            "must run out exactly at the boundary (S4.7, S5.2).",
            &l, skip, sizeof(skip) / sizeof(skip[0]));
    }
    {
        static const float a[] = {1.5f, -2.5f, 3.5f};
        static const uint32_t skip[] = {1};
        oplist_t l = {0};
        op_u  (&l, 0, 1000);
        op_arr(&l, K_ARR_FP32, 1, a, 3);  /* skip: a fixlen array at end of message */
        emit_vector_skip(o, "skip_fixlen_array_at_message_end", "skip",
            "A fixlen array is the last thing in the message and is skipped: count x "
            "element_length must run out exactly at the boundary (S4.8, S5.2).",
            &l, skip, sizeof(skip) / sizeof(skip[0]));
    }
    {
        static const uint32_t skip[] = {1};
        oplist_t l = {0};
        op_u  (&l, 0, 1000);
        op_seqb(&l, 1);                   /* skip: a sequence at end of message */
            op_u(&l, 0, 7);
            op_seqb(&l, 1);
                op_u(&l, 0, 8);
            op_seqe(&l);
        op_seqe(&l);
        emit_vector_skip(o, "skip_sequence_at_message_end", "skip",
            "A nested sequence is the last thing in the message and is skipped: skip_depth must "
            "unwind to zero on the closing marker, or the message never reaches a boundary (S4.9).",
            &l, skip, sizeof(skip) / sizeof(skip[0]));
    }
    {
        static const uint32_t skip[] = {2};
        oplist_t l = {0};
        op_u(&l, 0, 1000);
        op_seqb(&l, 1);
            op_u(&l, 0, 7);
            op_i(&l, 2, -55);   /* skip: the LAST field inside a read sequence */
        op_seqe(&l);
        op_u(&l, 3, 2000);
        emit_vector_skip(o, "skip_before_sequence_end", "skip",
            "The last field inside a sequence the receiver DOES read is skipped, so the resync "
            "lands on the sequence-end marker rather than on a value-bearing header (S4.9).",
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
        /* MESSAGE_SPEC S3: `count` is a capacity and the wire count M IS the
         * length, so a compact array carries its trailing default elements --
         * [1,2,0,0] is four elements and must not encode like [1,2]. */
        static const uint32_t a[] = {1, 2, 0, 0};
        EMIT(o, "array_unsigned_trailing_defaults", "array/integer",
             "Unsigned array ending in default (zero) elements: M is the array's "
             "length, so all four elements stay on the wire.",
             op_arr(&l, K_ARR_U32, 0, a, 4));
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

    /* --- wrapper-array elements: the positional sparse rule ------------------
     * A wrapper-sequence array (array of string / struct / array) is itself a
     * sequence, so its elements (id = index) follow the per-field rule
     * (MESSAGE_SPEC S2). Because a wrapper carries no length, the decoded length
     * is *highest present id + 1*, and one rule covers both element kinds:
     *   - an INTERIOR element equal to its element default is omitted, leaving an
     *     id gap -- a string/blob is not written and a struct/array element is not
     *     framed either;
     *   - the LAST element (op_last) is always written -- a leaf as its (default)
     *     value, a sequence element as an empty frame -- because that is what
     *     carries the length.
     * `serialized_sparse` is derived from exactly that: is_default_leaf drops the
     * unmarked default leaves, the lazy opener drops the unmarked empty frames,
     * and the marked last element survives either way. */
    {
        oplist_t l = {0};
        op_seqb(&l, 0);
            op_str(&l, 0, "a");
            op_str(&l, 1, "");   /* interior default -> dropped, leaves an id gap */
            op_str(&l, 2, "c"); op_last(&l);
        op_seqe(&l);
        emit_vector(o, "array_string_gap", "array/string",
                    "Array of string with a default (empty) element in the middle: "
                    "sparse omits id 1, leaving a gap the decoder restores.", &l);
    }
    {
        oplist_t l = {0};
        op_seqb(&l, 0);
            op_str(&l, 0, "a");
            op_str(&l, 1, ""); op_last(&l);   /* last element -> always written */
        op_seqe(&l);
        emit_vector(o, "array_string_trailing_default", "array/string",
                    "Array of string whose LAST element is the default (empty): it "
                    "is written anyway, so [\"a\",\"\"] stays distinct from "
                    "[\"a\"] -- the sparse form equals the dense one.", &l);
    }
    {
        oplist_t l = {0};
        op_seqb(&l, 0);
            op_str(&l, 0, "");
            op_str(&l, 1, ""); op_last(&l);
        op_seqe(&l);
        emit_vector(o, "array_string_all_default", "array/string",
                    "Array of only default (empty) string elements: the interior one "
                    "drops, so it encodes as its final element alone, at id 1 -- not "
                    "as the empty array.", &l);
    }
    {
        /* MESSAGE_SPEC S2/S5.1: a wrapper array whose ELEMENTS are sequences. The
         * middle element carries only a default-valued leaf, so it is all-default
         * and -- being INTERIOR -- is not framed at all: it leaves an id gap, like
         * a default leaf element. The array still decodes at length 3 because the
         * element at id 2 is present. */
        oplist_t l = {0};
        op_seqb(&l, 0);
            op_seqb(&l, 0); op_u(&l, 0, 1); op_seqe(&l);
            op_seqb(&l, 1); op_u(&l, 0, 0); op_seqe(&l);
            op_seqb(&l, 2); op_u(&l, 0, 3); op_seqe_last(&l);
        op_seqe(&l);
        emit_vector(o, "array_struct_interior_default_element", "array/struct",
                    "Array of struct where the middle element holds only a "
                    "default-valued leaf: sparse drops the element entirely (id "
                    "gap), and the array still decodes at length 3 from id 2.", &l);
    }
    {
        /* Every element all-default. The interior one drops; the LAST keeps its
         * (empty) frame, so the array is [{},{}] at length 2 -- not the empty
         * array -- and the enclosing FIELD is emitted rather than omitted (S2). */
        oplist_t l = {0};
        op_seqb(&l, 0);
            op_seqb(&l, 0); op_seqe(&l);
            op_seqb(&l, 1); op_seqe_last(&l);
        op_seqe(&l);
        emit_vector(o, "array_struct_all_default_elements", "array/struct",
                    "Array of two all-default structs: the interior element drops "
                    "and the last one keeps its empty frame, so [{},{}] stays "
                    "distinguishable from [] at length 2.", &l);
    }
    {
        /* An array whose elements are themselves wrapper arrays (S5.3). No
         * vector or corpus definition covered this shape before. */
        oplist_t l = {0};
        op_seqb(&l, 0);
            op_seqb(&l, 0); op_str(&l, 0, "a"); op_last(&l); op_seqe(&l);
            op_seqb(&l, 1); op_seqe_last(&l);
        op_seqe(&l);
        emit_vector(o, "array_of_string_arrays", "array/nested",
                    "Array of string arrays: the second row is empty and, being the "
                    "last element, keeps its frame, so the outer array decodes at "
                    "length 2.", &l);
    }
    {
        oplist_t l = {0};
        op_seqb(&l, 0);
            op_str(&l, 0, "");   /* leading default -> gap at id 0  */
            op_str(&l, 1, "x");
            op_str(&l, 2, ""); op_last(&l);   /* last element -> written */
        op_seqe(&l);
        emit_vector(o, "array_string_leading_default", "array/string",
                    "Array of string with leading and trailing default elements: "
                    "the leading one leaves an id gap, the trailing one is the last "
                    "element and is written.", &l);
    }
    {
        /* MESSAGE_SPEC S7.3 at an ELEMENT position. An element whose header wire
         * type contradicts the declared element type "MUST be skipped, exactly as a
         * field with an unknown id is skipped" -- and an unknown id leaves nothing
         * behind. So it does NOT occupy its id and does NOT count toward S5.1's
         * *highest present id + 1*: the ids the length counts are the ones consumed
         * as elements, not the ones that merely appeared on the wire.
         *
         * A schema-less consumer cannot see "declared type" and simply round-trips
         * these bytes; the vector exists for the byte pattern and for the rule its
         * description states -- two readings of S7.3-at-an-element drifted apart in
         * exactly the gap where no vector was looking. Its control is the next
         * vector (an EMPTY element IS present and DOES count); the two must never
         * be collapsed into one rule.
         *
         * The scalar is non-default, so the dense and sparse columns are equal and
         * the shape cannot be blamed on either pass. It is deliberately NOT marked
         * op_last: an element that is skipped carries no length. */
        oplist_t l = {0};
        op_seqb(&l, 0);
            op_u(&l, 0, 7);
        op_seqe(&l);
        emit_vector(o, "array_element_wire_type_mismatch", "array/struct",
                    "Wrapper array whose element id 0 arrives as a SCALAR where the "
                    "schema declares a sequence (struct) element. MESSAGE_SPEC S7.3: "
                    "it MUST be skipped exactly as a field with an unknown id is "
                    "skipped, so it does NOT occupy its id and does NOT count toward "
                    "the array's length (S5.1) -- a schema-aware receiver "
                    "reconstructs the slot from the element default, decodes the "
                    "EMPTY array, and re-encodes these bytes as nothing at all (S2). "
                    "Control: array_element_empty_frame_present.", &l);
    }
    {
        /* The control the rule above must not swallow: a well-typed but EMPTY
         * element frame is a PRESENT element. It counts, so the array is length 1.
         * Only the wire-type-mismatched element stops counting. */
        oplist_t l = {0};
        op_seqb(&l, 0);
            op_seqb(&l, 0); op_seqe_last(&l);
        op_seqe(&l);
        emit_vector(o, "array_element_empty_frame_present", "array/struct",
                    "The control for array_element_wire_type_mismatch: element id 0 "
                    "arrives as a well-typed but EMPTY frame. An empty element is a "
                    "PRESENT element -- it counts toward the length (MESSAGE_SPEC "
                    "S5.1), so the array decodes at length 1 and keeps the frame on "
                    "re-encode. Only a wire-type-mismatched element (S7.3) stops "
                    "counting; the two cases are not the same.", &l);
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
            op_last(&l);   /* id 200 is a string array: id 4 is its last element */
        op_seqe(&l);

        /* Skip a top-level scalar (id 1) and a whole top-level sub-sequence
         * (id 100); id 1 also appears inside sequences, exercising id-based
         * skipping at multiple nesting levels. */
        static const uint32_t skip[] = {1, 100};
        emit_vector_skip(o, "full_scale_example", "composite",
                    "Large message mixing scalars, nested sequences, "
                    "integer/float arrays and strings.", &l, skip, 2);
    }

    /* --- the (read, skipped) wire-type matrix, grouped by capability --- */
    emit_skip_matrix(o);
}

/* the negative (invalid-UTF-8) vectors *************************************/

/*
 * A `string` payload is UTF-8 (MESSAGE_SPEC §8). These vectors carry payloads
 * that are NOT valid UTF-8, each placed as a single `string` field at id 0.
 * A strict (SOFAB_STRICT_UTF8) implementation MUST:
 *   - decode `serialized_hex` -> the INVALID outcome (when the string is read;
 *     skipped fields are never validated), and
 *   - encode `string_hex` (the raw payload) -> the invalid-argument error.
 * Seeds cover every overlong form (incl. C0 80, the Modified-UTF-8 NUL), a lone
 * surrogate, out-of-range code points, bare continuation / invalid lead bytes,
 * and multi-byte sequences truncated at end-of-payload.
 */
static void emit_invalid_utf8(FILE *o)
{
    static const struct {
        const char *name;
        const char *desc;
        uint8_t     payload[8];
        size_t      len;
    } seeds[] = {
        { "utf8_overlong_c0_80",
          "Overlong 2-byte NUL (C0 80, Java \"Modified UTF-8\"): U+0000 must be a single 00 byte.",
          {0xC0, 0x80}, 2 },
        { "utf8_overlong_c1",
          "Overlong 2-byte sequence C1 BF (would decode to U+007F, which is 1-byte ASCII).",
          {0xC1, 0xBF}, 2 },
        { "utf8_overlong_3byte",
          "Overlong 3-byte sequence E0 80 80 (would decode to U+0000).",
          {0xE0, 0x80, 0x80}, 3 },
        { "utf8_overlong_4byte",
          "Overlong 4-byte sequence F0 80 80 80 (would decode to U+0000).",
          {0xF0, 0x80, 0x80, 0x80}, 4 },
        { "utf8_surrogate_d800",
          "Lone UTF-16 surrogate code point U+D800 (ED A0 80): never valid in UTF-8.",
          {0xED, 0xA0, 0x80}, 3 },
        { "utf8_surrogate_dfff",
          "Lone UTF-16 surrogate code point U+DFFF (ED BF BF): never valid in UTF-8.",
          {0xED, 0xBF, 0xBF}, 3 },
        { "utf8_out_of_range",
          "Code point above U+10FFFF (F4 90 80 80 = U+110000): out of the Unicode range.",
          {0xF4, 0x90, 0x80, 0x80}, 4 },
        { "utf8_bare_continuation",
          "Bare continuation byte 0x80 with no lead byte.",
          {0x80}, 1 },
        { "utf8_lone_ff",
          "0xFF: never a valid UTF-8 byte in any position.",
          {0xFF}, 1 },
        { "utf8_truncated_2byte",
          "2-byte lead C2 with no continuation byte (truncated at end-of-payload -> INVALID).",
          {0xC2}, 1 },
        { "utf8_truncated_3byte",
          "3-byte lead E2 82 missing its final continuation byte (truncated at end-of-payload).",
          {0xE2, 0x82}, 2 },
    };

    int first = 1;
    for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); ++i)
    {
        uint8_t msg[32];
        size_t  msglen = build_string_msg(msg, 0, seeds[i].payload, seeds[i].len);

        if (!first) fputs(",\n", o);
        first = 0;

        fprintf(o, "    {\n");
        fprintf(o, "      \"name\": ");        json_string(o, seeds[i].name); fputs(",\n", o);
        fprintf(o, "      \"group\": \"invalid/utf8\",\n");
        fprintf(o, "      \"description\": "); json_string(o, seeds[i].desc); fputs(",\n", o);
        fprintf(o, "      \"requires\": [\"fixlen\"],\n");
        fprintf(o, "      \"id\": 0,\n");
        fprintf(o, "      \"string_hex\": ");  json_hex(o, seeds[i].payload, seeds[i].len); fputs(",\n", o);
        fprintf(o, "      \"serialized_hex\": "); json_hex(o, msg, msglen); fputs(",\n", o);
        fprintf(o, "      \"decode_outcome\": \"invalid\",\n");
        fprintf(o, "      \"encode_outcome\": \"invalid_argument\"\n");
        fprintf(o, "    }");
    }
}

/* the sequence-array growth cases ******************************************/

/*
 * CORELIB_PLAN §7.2 item 8 / ARCHITECTURE §9.5 shape B.
 *
 * A wrapper (sequence) array carries no element count on the wire: its length is
 * *highest present id + 1* (MESSAGE_SPEC §5.1), so the size is known only when
 * the array ends and the container GROWS as elements arrive. It is the one
 * allocation shape where growth is conformant — everything with a count or a
 * length ahead of its payload (§4.7 integer arrays, §4.8 fixlen arrays,
 * `string`/`blob`) checks that word and allocates exactly it, once.
 *
 * These cases cannot be vectors. A vector is keyed by a byte string, and two
 * ports that grow differently emit IDENTICAL bytes and reach IDENTICAL outcomes
 * — the positive suite is structurally blind to growth. A growth case is keyed
 * by a DELIVERY SEQUENCE OF ELEMENT IDS instead, and the port builds the message
 * from it. So, like `invalid_utf8`, this block is a hand-authored seed table and
 * not a replay of the encoder: there is no encoder run that produces a growth
 * expectation.
 *
 * Indices are CAP-RELATIVE on purpose. The receiver cap `max_dyn_array_count` is
 * per-target configuration — ARCHITECTURE §9.5 and CORELIB_PLAN §6.2.1
 * deliberately fix no family-wide number — so a case says `id_from_cap: -1` and
 * each port substitutes its own configured value. A hardcoded number would be
 * wrong for every target that chose another one. `id`/`length` are absolute;
 * `id_from_cap`/`length_from_cap` are added to the cap. Every case assumes a cap
 * of at least 4.
 *
 * THE EXPECTATIONS BELOW COME FROM ARCHITECTURE §9.5 AND CORELIB_PLAN §7.2 ITEM
 * 8 — NOT from what this library does. `corelib-c-cpp` is statically bounded and
 * never grows: it authors these cases and does not execute them (the block's
 * `requires: ["dynamic_arrays"]` gating excludes it, as it excludes C and Rust
 * `no_std`). Do not "fix" a case to match the one implementation that
 * structurally cannot run it. See assets/test_vectors_README.md.
 */
static void emit_sequence_growth(FILE *o)
{
    /* One delivered element. `rel` makes `id` an offset added to the cap. */
    struct el { int rel; int32_t id; const char *sval; long ival; };

    static const struct {
        const char *name;
        const char *group;
        const char *desc;
        const char *requires;   /* verbatim JSON array */
        const char *elem;       /* element_type: "string" | "struct" */
        struct el   deliver[4];
        size_t      ndeliver;
        const char *outcome;    /* "complete" | "limit_exceeded" */
        int         len_rel;    /* 1 -> length_from_cap, 0 -> length */
        int32_t     len;        /* the resulting container length (complete) */
        int32_t     max_length; /* not extended past this (limit_exceeded) */
        int32_t     default_ids[2];
        size_t      ndefault;
    } seeds[] = {
        { "growth_index_at_cap_minus_one", "growth/index",
          "Two string elements, at id 0 and at the last legal index (cap-1). The array is cap long "
          "although two elements arrived: a sparse array allocates by its highest id, not by how many "
          "elements were delivered. The at-cap-1 index is accepted (ARCHITECTURE S9.5).",
          "[\"sequence\", \"fixlen\", \"dynamic_arrays\"]", "string",
          { {0, 0, "a", 0}, {1, -1, "b", 0} }, 2,
          "complete", 1, 0, 0, {0, 0}, 0 },

        { "growth_index_at_cap", "growth/index",
          "A string element at id 0, then one at the cap itself. max_dyn_array_count binds the element "
          "INDEX for a wrapper array (there is no count header to bind), so id >= cap is LimitExceeded "
          "-- checked BEFORE the container grows, so nothing is allocated for it. A policy rejection, "
          "not INVALID: the bytes are well-formed and decode under a looser cap (CORELIB_PLAN S6.2.1).",
          "[\"sequence\", \"fixlen\", \"dynamic_arrays\"]", "string",
          { {0, 0, "a", 0}, {1, 0, "b", 0} }, 2,
          "limit_exceeded", 0, 0, 1, {0, 0}, 0 },

        { "growth_gap_filled", "growth/gap",
          "String elements at ids 0, 2 and 3 -- id 1 is an interior element equal to its default and so "
          "MUST NOT be written (MESSAGE_SPEC S5.1). The gap is well-formed: the array is 4 long, id 1 "
          "holds the element default, and the gap neither shortens nor shifts the array.",
          "[\"sequence\", \"fixlen\", \"dynamic_arrays\"]", "string",
          { {0, 0, "a", 0}, {0, 2, "c", 0}, {0, 3, "d", 0} }, 3,
          "complete", 0, 4, 0, {1, 0}, 1 },

        { "growth_no_partial_extension", "growth/reject",
          "A string element at id 0, then one at the cap (rejected), then one at id 1. The rejection is "
          "terminal (CORELIB_PLAN S6.3), and the container must not be left extended toward the rejected "
          "index: the check runs before the grow, so the length never passes what legitimately arrived.",
          "[\"sequence\", \"fixlen\", \"dynamic_arrays\"]", "string",
          { {0, 0, "a", 0}, {1, 0, "x", 0}, {0, 1, "b", 0} }, 3,
          "limit_exceeded", 0, 0, 1, {0, 0}, 0 },

        { "growth_empty_array", "growth/length",
          "A wrapper array framed empty -- the explicit-empty form, with no element at all. There is no "
          "highest present id, so the length is 0 and the container never grows (MESSAGE_SPEC S5.1). The "
          "zero end of the range the cap bounds at the other.",
          "[\"sequence\", \"dynamic_arrays\"]", "string",
          { {0, 0, NULL, 0} }, 0,
          "complete", 0, 0, 0, {0, 0}, 0 },

        { "growth_single_at_cap_minus_one", "growth/index",
          "One string element, at cap-1, with nothing before it. The array is still cap long: the length "
          "is highest present id + 1 whatever the delivery order or count, so a single element at the "
          "boundary is the whole allocation on its own.",
          "[\"sequence\", \"fixlen\", \"dynamic_arrays\"]", "string",
          { {1, -1, "a", 0} }, 1,
          "complete", 1, 0, 0, {0, 0}, 0 },

        { "growth_index_at_cap_minus_one_struct", "growth/index",
          "growth_index_at_cap_minus_one with STRUCT elements: each element is a framed sub-sequence "
          "carrying one unsigned field at id 0, not a leaf. A framed element reaches the container "
          "through the collector's sequence path rather than its leaf path, so the boundary is asserted "
          "on both. The verdict is the same -- the bound is the index, never the element kind.",
          "[\"sequence\", \"dynamic_arrays\"]", "struct",
          { {0, 0, NULL, 1}, {1, -1, NULL, 2} }, 2,
          "complete", 1, 0, 0, {0, 0}, 0 },

        { "growth_index_at_cap_struct", "growth/index",
          "growth_index_at_cap with STRUCT elements: an over-cap index is LimitExceeded on the framed "
          "element path too, before the frame is applied and before the container grows.",
          "[\"sequence\", \"dynamic_arrays\"]", "struct",
          { {0, 0, NULL, 1}, {1, 0, NULL, 2} }, 2,
          "limit_exceeded", 0, 0, 1, {0, 0}, 0 },
    };

    int first = 1;
    for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); ++i)
    {
        if (!first) fputs(",\n", o);
        first = 0;

        fprintf(o, "    {\n");
        fprintf(o, "      \"name\": ");        json_string(o, seeds[i].name);  fputs(",\n", o);
        fprintf(o, "      \"group\": ");       json_string(o, seeds[i].group); fputs(",\n", o);
        fprintf(o, "      \"description\": "); json_string(o, seeds[i].desc);  fputs(",\n", o);
        fprintf(o, "      \"requires\": %s,\n", seeds[i].requires);
        fprintf(o, "      \"field_id\": 0,\n");
        fprintf(o, "      \"element_type\": ");json_string(o, seeds[i].elem);  fputs(",\n", o);

        fprintf(o, "      \"deliver\": [");
        for (size_t k = 0; k < seeds[i].ndeliver; ++k)
        {
            const struct el *e = &seeds[i].deliver[k];
            fprintf(o, "%s\n        { ", k ? "," : "");
            if (e->rel) fprintf(o, "\"id_from_cap\": %d, ", (int)e->id);
            else        fprintf(o, "\"id\": %d, ", (int)e->id);
            fputs("\"value\": ", o);
            if (e->sval) json_string(o, e->sval);
            else         fprintf(o, "%ld", e->ival);
            fputs(" }", o);
        }
        fputs(seeds[i].ndeliver ? "\n      ],\n" : "],\n", o);

        fprintf(o, "      \"expect\": {\n");
        fprintf(o, "        \"outcome\": ");   json_string(o, seeds[i].outcome);
        if (strcmp(seeds[i].outcome, "complete") == 0)
        {
            fputs(",\n", o);
            if (seeds[i].len_rel) fprintf(o, "        \"length_from_cap\": %d", (int)seeds[i].len);
            else                  fprintf(o, "        \"length\": %d", (int)seeds[i].len);
            if (seeds[i].ndefault)
            {
                fputs(",\n        \"default_ids\": [", o);
                for (size_t k = 0; k < seeds[i].ndefault; ++k)
                    fprintf(o, "%s%d", k ? ", " : "", (int)seeds[i].default_ids[k]);
                fputs("]", o);
            }
            fputs("\n", o);
        }
        else
        {
            fprintf(o, ",\n        \"terminal\": true,\n");
            fprintf(o, "        \"max_length\": %d\n", (int)seeds[i].max_length);
        }
        fprintf(o, "      }\n");
        fprintf(o, "    }");
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
    fprintf(o, "    \"serialized_sparse.hex\": \"lowercase hex of the sparse-canonical message (MESSAGE_SPEC S2): every leaf field equal to its type default is omitted, and a sequence left without content is omitted too (not framed empty); byte-exact target for a sparse encoder\",\n");
    fprintf(o, "    \"field.element\": \"true marks the op at a wrapper array's LAST element index (MESSAGE_SPEC S2/S5.1). A wrapper carries no length, so the length is highest present id + 1: that element is ALWAYS written -- a leaf as its (default) value, a sequence element as an empty frame -- while every INTERIOR element equal to its default is omitted, leaf and sequence element alike. It is the only op the sparse column never drops\",\n");
    fprintf(o, "    \"integers\": \"decimal JSON number literals (full u64/i64 range)\",\n");
    fprintf(o, "    \"floats\": \"finite values as JSON numbers; +/-infinity as the strings 'inf'/'-inf'\",\n");
    fprintf(o, "    \"blob.value_hex\": \"lowercase hex of the blob payload\",\n");
    fprintf(o, "    \"array.element_type\": \"input element width/type fed to the encoder (u8..u64, i8..i64, fp32, fp64)\",\n");
    fprintf(o, "    \"invalid_utf8\": \"NEGATIVE vectors: a `string` field (id 0) whose bytes are not valid UTF-8. "
               "A strict (SOFAB_STRICT_UTF8) build MUST decode serialized_hex to the INVALID outcome and refuse to "
               "encode string_hex with the invalid-argument error. Backward-compatible: consumers that only read "
               "'vectors' ignore this key. See test_vectors_README.md.\",\n");
    fprintf(o, "    \"sequence_growth\": \"GROWTH cases (CORELIB_PLAN S7.2 item 8): a wrapper array's length is "
               "highest present id + 1 (MESSAGE_SPEC S5.1), so its container grows as elements arrive -- the one "
               "allocation shape where growth is conformant (ARCHITECTURE S9.5). A case is keyed by a DELIVERY "
               "SEQUENCE OF ELEMENT IDS, not by bytes, because two ports that grow differently emit identical bytes: "
               "the port builds the message from 'deliver' and asserts 'expect'. Indices are CAP-RELATIVE -- "
               "'id_from_cap'/'length_from_cap' are offsets added to the port's own configured max_dyn_array_count, "
               "which is per-target and never a family-wide number; every case assumes a cap of at least 4. Gated by "
               "requires 'dynamic_arrays': statically bounded profiles (C, C++ corelib: c-cpp, Rust no_std) never "
               "grow and do not run this block. Expectations come from ARCHITECTURE S9.5 and CORELIB_PLAN S7.2 item "
               "8, NOT from the generating implementation, which authors these cases without executing them. "
               "Backward-compatible: consumers that only read 'vectors' ignore this key. See "
               "test_vectors_README.md.\"\n");
    fprintf(o, "  },\n");
    fprintf(o, "  \"vectors\": [\n");

    emit_all(o);

    fprintf(o, "\n  ],\n");

    /* Negative conformance vectors: invalid-UTF-8 `string` payloads that a strict
     * build rejects on both decode and encode (CORELIB_PLAN §6.4). Kept in a
     * dedicated top-level array so existing consumers that only read "vectors"
     * stay unaffected. */
    fprintf(o, "  \"invalid_utf8\": [\n");

    emit_invalid_utf8(o);

    fprintf(o, "\n  ],\n");

    /* Sequence-array growth cases: keyed by a delivery sequence of element ids
     * rather than by bytes, so they cannot be vectors (CORELIB_PLAN §7.2 item
     * 8). Another dedicated top-level array, for the same backward-compatibility
     * reason as "invalid_utf8". */
    fprintf(o, "  \"sequence_growth\": [\n");

    emit_sequence_growth(o);

    fprintf(o, "\n  ]\n");
    fprintf(o, "}\n");
    return 0;
}

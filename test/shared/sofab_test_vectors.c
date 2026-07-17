/*!
 * @file sofab_test_vectors.c
 * @brief Shared conformance-vector engine (see sofab_test_vectors.h).
 *
 * SPDX-License-Identifier: MIT
 */

#include "sofab_test_vectors.h"
#include "sofab_test_json.h"

#include "sofab/ostream.h"
#include "sofab/istream.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* limits (the vector file is small and machine-generated) */
#define MAXELEM   256
#define STRMAX    256
#define BLOBMAX   256
#define MAXDEPTH  16
#define ENCBUF    4096

/* capability tags ***********************************************************
 *
 * Each vector may declare a "requires" list naming the optional library
 * features it needs. A build compiled without a feature (SOFAB_DISABLE_*) skips
 * the vectors that require it, so the same vector file drives every build
 * configuration. The generator derives these tags from each vector's content.
 */
#define CAP_FIXLEN   (1u << 0)
#define CAP_ARRAY    (1u << 1)
#define CAP_SEQUENCE (1u << 2)
#define CAP_FP64     (1u << 3)
#define CAP_INT64    (1u << 4)

/* Capabilities this build actually provides, from the compile-time flags. */
static uint32_t build_caps(void)
{
    uint32_t c = 0;
#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
    c |= CAP_FIXLEN;
#endif
#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
    c |= CAP_ARRAY;
#endif
#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
    c |= CAP_SEQUENCE;
#endif
    /* fp64 helpers live inside the fixlen block, so they need both. */
#if !defined(SOFAB_DISABLE_FP64_SUPPORT) && !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
    c |= CAP_FP64;
#endif
#if !defined(SOFAB_DISABLE_INT64_SUPPORT)
    c |= CAP_INT64;
#endif
    return c;
}

static uint32_t cap_from_name(const char *s, size_t len)
{
    if (len == 6 && !memcmp(s, "fixlen", 6))   return CAP_FIXLEN;
    if (len == 5 && !memcmp(s, "array", 5))    return CAP_ARRAY;
    if (len == 8 && !memcmp(s, "sequence", 8)) return CAP_SEQUENCE;
    if (len == 4 && !memcmp(s, "fp64", 4))     return CAP_FP64;
    if (len == 5 && !memcmp(s, "int64", 5))    return CAP_INT64;
    return 0; /* unknown tag: ignore (forward-compatible) */
}

/* op model ******************************************************************/

typedef enum
{
    OP_UNSIGNED, OP_SIGNED, OP_BOOLEAN, OP_FP32, OP_FP64, OP_STRING, OP_BLOB,
    OP_ARRAY, OP_SEQ_BEGIN, OP_SEQ_END
} opk_t;

typedef enum
{
    EL_U8, EL_U16, EL_U32, EL_U64, EL_I8, EL_I16, EL_I32, EL_I64, EL_FP32, EL_FP64
} elt_t;

typedef struct
{
    opk_t    kind;
    uint32_t id;
    uint64_t u;          /* unsigned / boolean scalar */
    int64_t  s;          /* signed scalar */
    double   f;          /* fp scalar */
    char    *str;        /* string payload (owned, NUL-terminated) */
    size_t   slen;
    uint8_t *blob;       /* blob payload (owned) */
    size_t   blen;
    elt_t    elem;       /* array element type */
    size_t   count;      /* array element count */
    uint64_t au[MAXELEM];
    int64_t  ai[MAXELEM];
    double   af[MAXELEM];
} op_t;

#define MAXSKIP 16

typedef struct
{
    char    *name;
    op_t    *ops;
    size_t   nops;
    uint8_t *bytes;
    size_t   nbytes;
    uint32_t skip_ids[MAXSKIP]; /* optional: field ids a receiver skips */
    size_t   nskip;
    uint32_t req;               /* capability mask from the "requires" tags */
} vector_t;

/* per-op decode destination (persists across the whole feed) */
typedef struct
{
    int     fired;
    int     skipped; /* the decoder was asked to skip this field (skip_ids) */
    sofab_unsigned_t u; /* value-type width: read directly into it (endian-safe) */
    sofab_signed_t   s;
    bool     b;
    float    f32;
    double   f64;
    char     str[STRMAX];
    uint8_t  blob[BLOBMAX];
    union {
        uint8_t  u8[MAXELEM];  uint16_t u16[MAXELEM];
        uint32_t u32[MAXELEM]; uint64_t u64[MAXELEM];
        int8_t   i8[MAXELEM];  int16_t  i16[MAXELEM];
        int32_t  i32[MAXELEM]; int64_t  i64[MAXELEM];
        float    f32a[MAXELEM]; double  f64a[MAXELEM];
    } arr;
} slot_t;

typedef struct
{
    const op_t *ops;
    size_t      n;
    size_t      i;
    size_t      depth;
    slot_t     *slots;
    sofab_istream_decoder_t decoders[MAXDEPTH];
    int         overflow;
    const uint32_t *skip_ids;    /* field ids the receiver skips (may be NULL) */
    size_t      nskip;           /* 0 => skip nothing (plain decode) */
} cursor_t;

/* helpers *******************************************************************/

static int eq32(float a, float b)
{ uint32_t x, y; memcpy(&x, &a, 4); memcpy(&y, &b, 4); return x == y; }

static int eq64(double a, double b)
{ uint64_t x, y; memcpy(&x, &a, 8); memcpy(&y, &b, 8); return x == y; }

static int hexnib(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int hex2bin(const char *hex, size_t hexlen, uint8_t **out, size_t *outlen)
{
    if (hexlen % 2) return -1;
    size_t n = hexlen / 2;
    uint8_t *b = (uint8_t *)malloc(n ? n : 1);
    if (!b) return -1;
    for (size_t i = 0; i < n; i++)
    {
        int hi = hexnib(hex[2 * i]), lo = hexnib(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) { free(b); return -1; }
        b[i] = (uint8_t)((hi << 4) | lo);
    }
    *out = b;
    *outlen = n;
    return 0;
}

/* parse a float field value: a JSON number, or the string "inf" / "-inf". */
static double parse_float(const sofab_json_t *v)
{
    if (sofab_json_type(v) == SOFAB_JSON_STRING)
    {
        size_t l; const char *s = sofab_json_string(v, &l);
        if (s && strcmp(s, "inf") == 0)  return (double)INFINITY;
        if (s && strcmp(s, "-inf") == 0) return -(double)INFINITY;
        return 0.0;
    }
    return sofab_json_double(v);
}

static int parse_elem(const char *s, elt_t *out)
{
    if (!strcmp(s, "u8"))   { *out = EL_U8;   return 0; }
    if (!strcmp(s, "u16"))  { *out = EL_U16;  return 0; }
    if (!strcmp(s, "u32"))  { *out = EL_U32;  return 0; }
    if (!strcmp(s, "u64"))  { *out = EL_U64;  return 0; }
    if (!strcmp(s, "i8"))   { *out = EL_I8;   return 0; }
    if (!strcmp(s, "i16"))  { *out = EL_I16;  return 0; }
    if (!strcmp(s, "i32"))  { *out = EL_I32;  return 0; }
    if (!strcmp(s, "i64"))  { *out = EL_I64;  return 0; }
    if (!strcmp(s, "fp32")) { *out = EL_FP32; return 0; }
    if (!strcmp(s, "fp64")) { *out = EL_FP64; return 0; }
    return -1;
}

static int elem_is_signed(elt_t e) { return e == EL_I8 || e == EL_I16 || e == EL_I32 || e == EL_I64; }
static int elem_is_float(elt_t e)  { return e == EL_FP32 || e == EL_FP64; }

/* vector loading ************************************************************/

static int load_field(const sofab_json_t *fj, op_t *op)
{
    const sofab_json_t *opn = sofab_json_get(fj, "op");
    size_t l; const char *ops = sofab_json_string(opn, &l);
    if (!ops) return -1;

    const sofab_json_t *idn = sofab_json_get(fj, "id");
    op->id = idn ? (uint32_t)sofab_json_u64(idn) : 0;

    if (!strcmp(ops, "unsigned"))      { op->kind = OP_UNSIGNED; op->u = sofab_json_u64(sofab_json_get(fj, "value")); }
    else if (!strcmp(ops, "signed"))   { op->kind = OP_SIGNED;   op->s = sofab_json_i64(sofab_json_get(fj, "value")); }
    else if (!strcmp(ops, "boolean"))  { op->kind = OP_BOOLEAN;  op->u = sofab_json_bool(sofab_json_get(fj, "value")) ? 1u : 0u; }
    else if (!strcmp(ops, "fp32"))     { op->kind = OP_FP32;     op->f = parse_float(sofab_json_get(fj, "value")); }
    else if (!strcmp(ops, "fp64"))     { op->kind = OP_FP64;     op->f = parse_float(sofab_json_get(fj, "value")); }
    else if (!strcmp(ops, "string"))
    {
        op->kind = OP_STRING;
        size_t sl; const char *sv = sofab_json_string(sofab_json_get(fj, "value"), &sl);
        if (!sv) return -1;
        op->str = (char *)malloc(sl + 1);
        if (!op->str) return -1;
        memcpy(op->str, sv, sl + 1);
        op->slen = sl;
    }
    else if (!strcmp(ops, "blob"))
    {
        op->kind = OP_BLOB;
        size_t hl; const char *hv = sofab_json_string(sofab_json_get(fj, "value_hex"), &hl);
        if (!hv) return -1;
        if (hex2bin(hv, hl, &op->blob, &op->blen)) return -1;
    }
    else if (!strcmp(ops, "array"))
    {
        op->kind = OP_ARRAY;
        const sofab_json_t *etn = sofab_json_get(fj, "element_type");
        size_t el; const char *et = sofab_json_string(etn, &el);
        if (!et || parse_elem(et, &op->elem)) return -1;
        const sofab_json_t *vals = sofab_json_get(fj, "values");
        size_t cnt = sofab_json_array_size(vals);
        if (cnt > MAXELEM) return -1;
        op->count = cnt;
        for (size_t k = 0; k < cnt; k++)
        {
            const sofab_json_t *e = sofab_json_array_at(vals, k);
            if (elem_is_float(op->elem))     op->af[k] = parse_float(e);
            else if (elem_is_signed(op->elem)) op->ai[k] = sofab_json_i64(e);
            else                              op->au[k] = sofab_json_u64(e);
        }
    }
    else if (!strcmp(ops, "sequence_begin")) { op->kind = OP_SEQ_BEGIN; }
    else if (!strcmp(ops, "sequence_end"))   { op->kind = OP_SEQ_END; }
    else return -1;

    return 0;
}

static void free_vector(vector_t *v)
{
    if (v->ops)
    {
        for (size_t i = 0; i < v->nops; i++)
        {
            free(v->ops[i].str);
            free(v->ops[i].blob);
        }
        free(v->ops);
    }
    free(v->bytes);
    free(v->name);
    memset(v, 0, sizeof(*v));
}

static int load_vector(const sofab_json_t *vj, vector_t *out)
{
    memset(out, 0, sizeof(*out));

    size_t nl; const char *nm = sofab_json_string(sofab_json_get(vj, "name"), &nl);
    out->name = (char *)malloc(nm ? nl + 1 : 1);
    if (!out->name) return -1;
    if (nm) memcpy(out->name, nm, nl + 1); else out->name[0] = '\0';

    const sofab_json_t *fields = sofab_json_get(vj, "fields");
    size_t nf = sofab_json_array_size(fields);
    out->ops = (op_t *)calloc(nf ? nf : 1, sizeof(op_t));
    if (!out->ops) { free_vector(out); return -1; }
    out->nops = nf;
    for (size_t i = 0; i < nf; i++)
    {
        if (load_field(sofab_json_array_at(fields, i), &out->ops[i]))
        { free_vector(out); return -1; }
    }

    const sofab_json_t *ser = sofab_json_get(vj, "serialized");
    size_t hl; const char *hex = sofab_json_string(sofab_json_get(ser, "hex"), &hl);
    if (!hex || hex2bin(hex, hl, &out->bytes, &out->nbytes))
    { free_vector(out); return -1; }

    /* optional: field ids a receiver is expected to skip */
    const sofab_json_t *skip = sofab_json_get(vj, "skip_ids");
    size_t ns = sofab_json_array_size(skip);
    if (ns > MAXSKIP) ns = MAXSKIP;
    out->nskip = ns;
    for (size_t i = 0; i < ns; i++)
        out->skip_ids[i] = (uint32_t)sofab_json_u64(sofab_json_array_at(skip, i));

    /* optional: capability tags this vector needs (drives feature skipping) */
    const sofab_json_t *req = sofab_json_get(vj, "requires");
    size_t nr = sofab_json_array_size(req);
    out->req = 0;
    for (size_t i = 0; i < nr; i++)
    {
        size_t tl; const char *tn = sofab_json_string(sofab_json_array_at(req, i), &tl);
        if (tn) out->req |= cap_from_name(tn, tl);
    }

    return 0;
}

/* encode side ***************************************************************/

typedef struct
{
    uint8_t *acc;
    size_t   len;
    size_t   cap;
    uint8_t  tiny[8];
    size_t   tiny_size;
} enc_acc_t;

static void enc_flush_cb(sofab_ostream_t *os, const uint8_t *data, size_t len, void *usr)
{
    enc_acc_t *a = (enc_acc_t *)usr;
    if (a->len + len <= a->cap)
    {
        memcpy(a->acc + a->len, data, len);
        a->len += len;
    }
    sofab_ostream_buffer_set(os, a->tiny, a->tiny_size, 0);
}

/* Encode one op. Feature-specific case BODIES are #if-guarded so the harness
 * compiles in reduced builds; vectors needing a disabled feature are skipped
 * before they ever reach here (see the capability filter in run_all), so a
 * guarded-out body is unreachable at run time and just falls through to the
 * E_USAGE return. All case labels are kept so -Wswitch stays satisfied. */
static sofab_ret_t replay_op(sofab_ostream_t *os, const op_t *op)
{
    switch (op->kind)
    {
        case OP_UNSIGNED: return sofab_ostream_write_unsigned(os, op->id, op->u);
        case OP_SIGNED:   return sofab_ostream_write_signed(os, op->id, op->s);
        case OP_BOOLEAN:  return sofab_ostream_write_boolean(os, op->id, op->u != 0);
        case OP_FP32:
#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
            return sofab_ostream_write_fp32(os, op->id, (float)op->f);
#else
            break;
#endif
        case OP_FP64:
#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) && !defined(SOFAB_DISABLE_FP64_SUPPORT)
            return sofab_ostream_write_fp64(os, op->id, op->f);
#else
            break;
#endif
        case OP_STRING:
#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
            return sofab_ostream_write_string(os, op->id, op->str);
#else
            break;
#endif
        case OP_BLOB:
#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
            return sofab_ostream_write_blob(os, op->id, op->blob, op->blen);
#else
            break;
#endif
        case OP_SEQ_BEGIN:
#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
            return sofab_ostream_write_sequence_begin(os, op->id);
#else
            break;
#endif
        case OP_SEQ_END:
#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
            return sofab_ostream_write_sequence_end(os);
#else
            break;
#endif
        case OP_ARRAY:
#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
        {
            size_t n = op->count;
            switch (op->elem)
            {
                case EL_U8:  { uint8_t  t[MAXELEM]; for (size_t k=0;k<n;k++) t[k]=(uint8_t) op->au[k];  return sofab_ostream_write_array_of_u8 (os, op->id, t, (int32_t)n); }
                case EL_U16: { uint16_t t[MAXELEM]; for (size_t k=0;k<n;k++) t[k]=(uint16_t)op->au[k];  return sofab_ostream_write_array_of_u16(os, op->id, t, (int32_t)n); }
                case EL_U32: { uint32_t t[MAXELEM]; for (size_t k=0;k<n;k++) t[k]=(uint32_t)op->au[k];  return sofab_ostream_write_array_of_u32(os, op->id, t, (int32_t)n); }
                case EL_I8:  { int8_t   t[MAXELEM]; for (size_t k=0;k<n;k++) t[k]=(int8_t)  op->ai[k];  return sofab_ostream_write_array_of_i8 (os, op->id, t, (int32_t)n); }
                case EL_I16: { int16_t  t[MAXELEM]; for (size_t k=0;k<n;k++) t[k]=(int16_t) op->ai[k];  return sofab_ostream_write_array_of_i16(os, op->id, t, (int32_t)n); }
                case EL_I32: { int32_t  t[MAXELEM]; for (size_t k=0;k<n;k++) t[k]=(int32_t) op->ai[k];  return sofab_ostream_write_array_of_i32(os, op->id, t, (int32_t)n); }
#if !defined(SOFAB_DISABLE_INT64_SUPPORT)
                case EL_U64: { uint64_t t[MAXELEM]; for (size_t k=0;k<n;k++) t[k]=          op->au[k];  return sofab_ostream_write_array_of_u64(os, op->id, t, (int32_t)n); }
                case EL_I64: { int64_t  t[MAXELEM]; for (size_t k=0;k<n;k++) t[k]=          op->ai[k];  return sofab_ostream_write_array_of_i64(os, op->id, t, (int32_t)n); }
#else
                case EL_U64: case EL_I64: break;
#endif
#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) && !defined(SOFAB_DISABLE_FP64_SUPPORT)
                case EL_FP64:{ double   t[MAXELEM]; for (size_t k=0;k<n;k++) t[k]=          op->af[k];  return sofab_ostream_write_array_of_fp64(os, op->id, t, (int32_t)n); }
#else
                case EL_FP64: break;
#endif
#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
                case EL_FP32:{ float    t[MAXELEM]; for (size_t k=0;k<n;k++) t[k]=(float)   op->af[k];  return sofab_ostream_write_array_of_fp32(os, op->id, t, (int32_t)n); }
#else
                case EL_FP32: break;
#endif
            }
            return SOFAB_RET_E_USAGE;
        }
#else
            break;
#endif /* !SOFAB_DISABLE_ARRAY_SUPPORT */
    }
    return SOFAB_RET_E_USAGE;
}

/* Encode all ops. tiny_size==0 -> single big buffer (no flush callback);
 * tiny_size>0 -> stream through a tiny re-armed buffer + flush accumulator. */
static int encode_bytes(const op_t *ops, size_t nops, size_t tiny_size,
                        uint8_t *out, size_t outcap, size_t *outlen, char *err, size_t errlen)
{
    sofab_ostream_t os;
    enc_acc_t acc;

    if (tiny_size == 0)
    {
        sofab_ostream_init(&os, out, outcap, 0, NULL, NULL);
    }
    else
    {
        memset(&acc, 0, sizeof(acc));
        acc.acc = out;
        acc.cap = outcap;
        acc.tiny_size = tiny_size;
        sofab_ostream_init(&os, acc.tiny, tiny_size, 0, enc_flush_cb, &acc);
    }

    for (size_t i = 0; i < nops; i++)
    {
        sofab_ret_t r = replay_op(&os, &ops[i]);
        if (r != SOFAB_RET_OK)
        {
            snprintf(err, errlen, "encode op %zu returned %d", i, (int)r);
            return -1;
        }
    }

    if (tiny_size == 0)
        *outlen = sofab_ostream_flush(&os);
    else
    {
        sofab_ostream_flush(&os);
        *outlen = acc.len;
    }
    return 0;
}

/* decode side ***************************************************************/

static void vec_field_cb(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usr)
{
    cursor_t *c = (cursor_t *)usr;
    (void)id; (void)size; (void)count;

    /* end markers are consumed internally by the decoder; advance past them */
    while (c->i < c->n && c->ops[c->i].kind == OP_SEQ_END)
    {
        c->i++;
        if (c->depth) c->depth--;
    }
    if (c->i >= c->n) { c->overflow = 1; return; }

    size_t idx = c->i++;
    const op_t *op = &c->ops[idx];
    slot_t *s = &c->slots[idx];

    /* skip-ids: when the field's id is one the receiver ignores, leave it
     * unread so the decoder auto-skips it — for any wire type, and the whole
     * sub-sequence (at any nesting depth) when the id names a sequence. */
    for (size_t k = 0; k < c->nskip; k++)
        if (c->skip_ids[k] == op->id) { s->skipped = 1; break; }
    if (s->skipped)
    {
        if (op->kind == OP_SEQ_BEGIN)
        {
            /* Do not descend: the decoder auto-skips the whole sub-sequence.
             * No child callbacks fire, so advance the cursor past the matching
             * SEQ_END (handles arbitrary nesting), marking every contained op
             * skipped so the comparison pass knows not to expect a value. */
            int d = 1;
            while (d > 0 && c->i < c->n)
            {
                opk_t k = c->ops[c->i].kind;
                c->slots[c->i].skipped = 1;
                c->i++;
                if (k == OP_SEQ_BEGIN) d++;
                else if (k == OP_SEQ_END) d--;
            }
        }
        return; /* non-sequence: decoder skips the payload automatically */
    }

    s->fired = 1;

    /* As in replay_op, feature-specific case bodies are #if-guarded; vectors
     * needing a disabled feature never reach here (they are skipped). */
    switch (op->kind)
    {
        case OP_UNSIGNED:
            /* s->u has the value-type width, so read straight into it. */
#if !defined(SOFAB_DISABLE_INT64_SUPPORT)
            sofab_istream_read_u64(ctx, &s->u);
#else
            sofab_istream_read_u32(ctx, &s->u);
#endif
            break;
        case OP_SIGNED:
#if !defined(SOFAB_DISABLE_INT64_SUPPORT)
            sofab_istream_read_i64(ctx, &s->s);
#else
            sofab_istream_read_i32(ctx, &s->s);
#endif
            break;
        case OP_BOOLEAN:  sofab_istream_read_bool(ctx, &s->b); break;
        case OP_FP32:
#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
            sofab_istream_read_fp32(ctx, &s->f32);
#endif
            break;
        case OP_FP64:
#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) && !defined(SOFAB_DISABLE_FP64_SUPPORT)
            sofab_istream_read_fp64(ctx, &s->f64);
#endif
            break;
        case OP_STRING:
#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
            sofab_istream_read_string(ctx, s->str, sizeof(s->str));
#endif
            break;
        case OP_BLOB:
#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
            if (op->blen) sofab_istream_read_blob(ctx, s->blob, sizeof(s->blob));
#endif
            break;
        case OP_ARRAY:
#if !defined(SOFAB_DISABLE_ARRAY_SUPPORT)
            switch (op->elem)
            {
                case EL_U8:  sofab_istream_read_array_of_u8 (ctx, s->arr.u8,  op->count); break;
                case EL_U16: sofab_istream_read_array_of_u16(ctx, s->arr.u16, op->count); break;
                case EL_U32: sofab_istream_read_array_of_u32(ctx, s->arr.u32, op->count); break;
                case EL_I8:  sofab_istream_read_array_of_i8 (ctx, s->arr.i8,  op->count); break;
                case EL_I16: sofab_istream_read_array_of_i16(ctx, s->arr.i16, op->count); break;
                case EL_I32: sofab_istream_read_array_of_i32(ctx, s->arr.i32, op->count); break;
#if !defined(SOFAB_DISABLE_INT64_SUPPORT)
                case EL_U64: sofab_istream_read_array_of_u64(ctx, s->arr.u64, op->count); break;
                case EL_I64: sofab_istream_read_array_of_i64(ctx, s->arr.i64, op->count); break;
#else
                case EL_U64: case EL_I64: break;
#endif
#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
                case EL_FP32:sofab_istream_read_array_of_fp32(ctx, s->arr.f32a, op->count); break;
#else
                case EL_FP32: break;
#endif
#if !defined(SOFAB_DISABLE_FIXLEN_SUPPORT) && !defined(SOFAB_DISABLE_FP64_SUPPORT)
                case EL_FP64:sofab_istream_read_array_of_fp64(ctx, s->arr.f64a, op->count); break;
#else
                case EL_FP64: break;
#endif
            }
#endif /* !SOFAB_DISABLE_ARRAY_SUPPORT */
            break;
        case OP_SEQ_BEGIN:
#if !defined(SOFAB_DISABLE_SEQUENCE_SUPPORT)
            if (c->depth < MAXDEPTH)
                sofab_istream_read_sequence(ctx, &c->decoders[c->depth++], vec_field_cb, c);
            else
                c->overflow = 1;
#endif
            break;
        case OP_SEQ_END: break; /* unreachable */
    }
}

static int compare_slot(const op_t *op, const slot_t *s, char *err, size_t errlen)
{
    if (!s->fired) { snprintf(err, errlen, "field never decoded"); return -1; }
    switch (op->kind)
    {
        case OP_UNSIGNED:
            if (s->u != op->u) { snprintf(err, errlen, "unsigned %llu != %llu",
                (unsigned long long)s->u, (unsigned long long)op->u); return -1; }
            break;
        case OP_SIGNED:
            if (s->s != op->s) { snprintf(err, errlen, "signed %lld != %lld",
                (long long)s->s, (long long)op->s); return -1; }
            break;
        case OP_BOOLEAN:
            if ((s->b ? 1 : 0) != (op->u ? 1 : 0)) { snprintf(err, errlen, "boolean mismatch"); return -1; }
            break;
        case OP_FP32:
            if (!eq32((float)op->f, s->f32)) { snprintf(err, errlen, "fp32 mismatch"); return -1; }
            break;
        case OP_FP64:
            if (!eq64(op->f, s->f64)) { snprintf(err, errlen, "fp64 mismatch"); return -1; }
            break;
        case OP_STRING:
            if (strcmp(s->str, op->str) != 0) { snprintf(err, errlen, "string '%.48s' != '%.48s'", s->str, op->str); return -1; }
            break;
        case OP_BLOB:
            if (op->blen && memcmp(s->blob, op->blob, op->blen) != 0) { snprintf(err, errlen, "blob mismatch"); return -1; }
            break;
        case OP_ARRAY:
            for (size_t k = 0; k < op->count; k++)
            {
                int bad = 0;
                switch (op->elem)
                {
                    case EL_U8:  bad = s->arr.u8[k]  != (uint8_t)op->au[k];  break;
                    case EL_U16: bad = s->arr.u16[k] != (uint16_t)op->au[k]; break;
                    case EL_U32: bad = s->arr.u32[k] != (uint32_t)op->au[k]; break;
                    case EL_U64: bad = s->arr.u64[k] != op->au[k];           break;
                    case EL_I8:  bad = s->arr.i8[k]  != (int8_t)op->ai[k];   break;
                    case EL_I16: bad = s->arr.i16[k] != (int16_t)op->ai[k];  break;
                    case EL_I32: bad = s->arr.i32[k] != (int32_t)op->ai[k];  break;
                    case EL_I64: bad = s->arr.i64[k] != op->ai[k];           break;
                    case EL_FP32: bad = !eq32((float)op->af[k], s->arr.f32a[k]); break;
                    case EL_FP64: bad = !eq64(op->af[k], s->arr.f64a[k]);        break;
                }
                if (bad) { snprintf(err, errlen, "array element %zu mismatch", k); return -1; }
            }
            break;
        case OP_SEQ_BEGIN:
        case OP_SEQ_END:
            break;
    }
    return 0;
}

static int decode_bytes(const op_t *ops, size_t nops, const uint8_t *bytes, size_t nbytes,
                        int one_byte, const uint32_t *skip_ids, size_t nskip,
                        char *err, size_t errlen)
{
    cursor_t c;
    memset(&c, 0, sizeof(c));
    c.ops = ops;
    c.n = nops;
    c.skip_ids = skip_ids;
    c.nskip = nskip;
    c.slots = (slot_t *)calloc(nops ? nops : 1, sizeof(slot_t));
    if (!c.slots) { snprintf(err, errlen, "oom"); return -1; }

    sofab_istream_t is;
    sofab_istream_init(&is, vec_field_cb, &c);

    sofab_ret_t r = SOFAB_RET_OK;
    if (one_byte)
    {
        // Feeding one byte at a time, every intermediate byte legitimately ends
        // mid-field: SOFAB_RET_INCOMPLETE is expected and not an error, so keep
        // going. A hard error stops the loop. The final byte of a valid vector
        // lands on a field boundary and returns SOFAB_RET_OK.
        for (size_t i = 0; i < nbytes; i++)
        {
            r = sofab_istream_feed(&is, &bytes[i], 1);
            if (r != SOFAB_RET_OK && r != SOFAB_RET_INCOMPLETE) break;
        }
    }
    else
    {
        r = sofab_istream_feed(&is, bytes, nbytes);
    }

    int rc = 0;
    if (r != SOFAB_RET_OK) { snprintf(err, errlen, "feed returned %d", (int)r); rc = -1; }

    /* consume any trailing end markers */
    while (rc == 0 && c.i < c.n && c.ops[c.i].kind == OP_SEQ_END) c.i++;

    if (rc == 0 && c.overflow) { snprintf(err, errlen, "decoder delivered too many fields"); rc = -1; }
    if (rc == 0 && c.i != c.n) { snprintf(err, errlen, "decoded %zu of %zu ops", c.i, c.n); rc = -1; }

    for (size_t i = 0; rc == 0 && i < nops; i++)
    {
        if (ops[i].kind == OP_SEQ_BEGIN || ops[i].kind == OP_SEQ_END) continue;
        /* A deliberately-skipped field (its id, or a parent sequence's id, is in
         * skip_ids) has no decoded value to compare; every other field must have
         * been delivered. */
        if (c.slots[i].skipped) continue;
        if (compare_slot(&ops[i], &c.slots[i], err, errlen)) rc = -1;
    }

    free(c.slots);
    return rc;
}

/* scenario runners **********************************************************/

static int run_encode(const vector_t *v, size_t tiny, char *err, size_t errlen)
{
    uint8_t out[ENCBUF];
    size_t  used = 0;
    if (encode_bytes(v->ops, v->nops, tiny, out, sizeof(out), &used, err, errlen)) return -1;
    if (used != v->nbytes) { snprintf(err, errlen, "encoded %zu bytes, expected %zu", used, v->nbytes); return -1; }
    if (memcmp(out, v->bytes, used) != 0) { snprintf(err, errlen, "encoded bytes differ from serialized.hex"); return -1; }
    return 0;
}

static int run_roundtrip(const vector_t *v, char *err, size_t errlen)
{
    uint8_t out[ENCBUF];
    size_t  used = 0;
    if (encode_bytes(v->ops, v->nops, 0, out, sizeof(out), &used, err, errlen)) return -1;
    return decode_bytes(v->ops, v->nops, out, used, 0, NULL, 0, err, errlen);
}

/* negative (invalid-UTF-8) vectors ******************************************
 *
 * The top-level "invalid_utf8" array holds `string` fields (id 0) whose bytes
 * are NOT valid UTF-8. Under a strict build (SOFAB_STRICT_UTF8) each one must be
 * rejected symmetrically: decoding serialized_hex (with the string MATERIALIZED,
 * not skipped) yields the INVALID outcome, and encoding string_hex as a `string`
 * yields the invalid-argument error (CORELIB_PLAN §6.4). A non-strict (or
 * fixlen-less) build counts them but does not exercise them.
 */
#if SOFAB_STRICT_UTF8 && !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
/* Materialize the string field so the strict decoder validates it (a skipped
 * field is never validated). noterm avoids the null-terminator length guard. */
static void neg_utf8_field_cb(sofab_istream_t *ctx, sofab_id_t id, size_t size, size_t count, void *usr)
{
    (void)id; (void)size; (void)count;
    sofab_istream_read_string_noterm(ctx, (char *)usr, STRMAX);
}
#endif

static void run_invalid_utf8(const sofab_json_t *root, sofab_test_vectors_result_t *out)
{
    const sofab_json_t *arr = sofab_json_get(root, "invalid_utf8");
    size_t n = sofab_json_array_size(arr);
    out->invalid_vectors = (int)n;

#if SOFAB_STRICT_UTF8 && !defined(SOFAB_DISABLE_FIXLEN_SUPPORT)
    for (size_t i = 0; i < n; i++)
    {
        const sofab_json_t *vj = sofab_json_array_at(arr, i);
        size_t nl; const char *name = sofab_json_string(sofab_json_get(vj, "name"), &nl);
        if (!name) name = "?";

        size_t shl; const char *shex = sofab_json_string(sofab_json_get(vj, "serialized_hex"), &shl);
        size_t phl; const char *phex = sofab_json_string(sofab_json_get(vj, "string_hex"), &phl);

        uint8_t *msg = NULL, *payload = NULL;
        size_t   msglen = 0, paylen = 0;
        if (!shex || hex2bin(shex, shl, &msg, &msglen) ||
            !phex || hex2bin(phex, phl, &payload, &paylen))
        {
            out->failures++;
            if (!out->first_error[0])
                snprintf(out->first_error, sizeof(out->first_error), "invalid_utf8[%zu]: malformed vector", i);
            free(msg); free(payload);
            continue;
        }

        /* decode (whole): a materialized invalid-UTF-8 string is INVALID */
        {
            out->invalid_checks++;
            char buf[STRMAX];
            sofab_istream_t is;
            sofab_istream_init(&is, neg_utf8_field_cb, buf);
            sofab_ret_t r = sofab_istream_feed(&is, msg, msglen);
            if (r != SOFAB_RET_E_INVALID_MSG)
            {
                out->failures++;
                if (!out->first_error[0])
                    snprintf(out->first_error, sizeof(out->first_error),
                             "%s/decode: expected INVALID, got %d", name, (int)r);
            }
        }

        /* decode (one byte at a time): every intermediate byte is INCOMPLETE
         * (a valid partial decode — never a premature INVALID for a well-formed
         * prefix), and the complete payload resolves to INVALID, never OK. */
        {
            out->invalid_checks++;
            char buf[STRMAX];
            sofab_istream_t is;
            sofab_istream_init(&is, neg_utf8_field_cb, buf);
            sofab_ret_t r = SOFAB_RET_OK;
            int saw_invalid = 0;
            for (size_t k = 0; k < msglen; k++)
            {
                r = sofab_istream_feed(&is, &msg[k], 1);
                if (r == SOFAB_RET_E_INVALID_MSG) { saw_invalid = 1; break; }
                if (r != SOFAB_RET_OK && r != SOFAB_RET_INCOMPLETE) break;
            }
            if (!saw_invalid)
            {
                out->failures++;
                if (!out->first_error[0])
                    snprintf(out->first_error, sizeof(out->first_error),
                             "%s/chunked-decode: expected INVALID, last=%d", name, (int)r);
            }
        }

        /* encode: writing the invalid payload as a `string` is refused */
        {
            out->invalid_checks++;
            uint8_t obuf[64];
            sofab_ostream_t os;
            sofab_ostream_init(&os, obuf, sizeof(obuf), 0, NULL, NULL);
            sofab_ret_t r = sofab_ostream_write_fixlen(
                &os, 0, payload, (int32_t)paylen, SOFAB_FIXLENTYPE_STRING);
            if (r != SOFAB_RET_E_ARGUMENT)
            {
                out->failures++;
                if (!out->first_error[0])
                    snprintf(out->first_error, sizeof(out->first_error),
                             "%s/encode: expected InvalidArgument, got %d", name, (int)r);
            }
        }

        free(msg); free(payload);
    }
#else
    (void)root;
    (void)n;
#endif /* SOFAB_STRICT_UTF8 && !SOFAB_DISABLE_FIXLEN_SUPPORT */
}

/* top-level *****************************************************************/

static char *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    if (n < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[rd] = '\0';
    *len = rd;
    return buf;
}

int sofab_test_vectors_run_all(const char *path, sofab_test_vectors_result_t *out)
{
    memset(out, 0, sizeof(*out));

    size_t flen = 0;
    char *text = read_file(path, &flen);
    if (!text) { snprintf(out->first_error, sizeof(out->first_error), "cannot open %s", path); return -1; }

    char perr[128];
    sofab_json_t *root = sofab_json_parse(text, flen, perr, sizeof(perr));
    free(text);
    if (!root) { snprintf(out->first_error, sizeof(out->first_error), "json parse error: %s", perr); return -1; }

    const sofab_json_t *vectors = sofab_json_get(root, "vectors");
    size_t nv = sofab_json_array_size(vectors);
    out->loaded = 1;
    out->vectors = (int)nv;

    static const size_t tiny_sizes[] = { 1, 3, 7 };

    const uint32_t caps = build_caps();

    for (size_t i = 0; i < nv; i++)
    {
        vector_t vec;
        if (load_vector(sofab_json_array_at(vectors, i), &vec))
        {
            out->failures++;
            if (out->first_error[0] == '\0')
                snprintf(out->first_error, sizeof(out->first_error), "vector %zu: failed to load", i);
            continue;
        }

        /* skip vectors needing a feature this build was compiled without */
        if (vec.req & ~caps)
        {
            out->skipped++;
            free_vector(&vec);
            continue;
        }

        char err[160];

        /* encode (normal) */
        out->checks++;
        if (run_encode(&vec, 0, err, sizeof(err))) { out->failures++;
            if (!out->first_error[0]) snprintf(out->first_error, sizeof(out->first_error), "%s/encode: %s", vec.name, err); }

        /* chunked encode (sweep tiny buffer sizes) */
        for (size_t t = 0; t < sizeof(tiny_sizes)/sizeof(tiny_sizes[0]); t++)
        {
            out->checks++;
            if (run_encode(&vec, tiny_sizes[t], err, sizeof(err))) { out->failures++;
                if (!out->first_error[0]) snprintf(out->first_error, sizeof(out->first_error), "%s/chunked-encode(%zu): %s", vec.name, tiny_sizes[t], err); }
        }

        /* decode (whole) */
        out->checks++;
        if (decode_bytes(vec.ops, vec.nops, vec.bytes, vec.nbytes, 0, NULL, 0, err, sizeof(err))) { out->failures++;
            if (!out->first_error[0]) snprintf(out->first_error, sizeof(out->first_error), "%s/decode: %s", vec.name, err); }

        /* chunked decode (one byte at a time) */
        out->checks++;
        if (decode_bytes(vec.ops, vec.nops, vec.bytes, vec.nbytes, 1, NULL, 0, err, sizeof(err))) { out->failures++;
            if (!out->first_error[0]) snprintf(out->first_error, sizeof(out->first_error), "%s/chunked-decode: %s", vec.name, err); }

        /* skip-ids decode: only when the vector declares skip_ids. Skip exactly
         * those field ids (at every nesting level) and verify the remaining
         * fields still decode and the message is fully consumed. Nothing is ever
         * skipped without a skip_ids list. */
        if (vec.nskip)
        {
            out->checks++;
            if (decode_bytes(vec.ops, vec.nops, vec.bytes, vec.nbytes, 0, vec.skip_ids, vec.nskip, err, sizeof(err))) { out->failures++;
                if (!out->first_error[0]) snprintf(out->first_error, sizeof(out->first_error), "%s/skip-ids: %s", vec.name, err); }

            /* and the same, fed one byte at a time (skip across chunk boundaries) */
            out->checks++;
            if (decode_bytes(vec.ops, vec.nops, vec.bytes, vec.nbytes, 1, vec.skip_ids, vec.nskip, err, sizeof(err))) { out->failures++;
                if (!out->first_error[0]) snprintf(out->first_error, sizeof(out->first_error), "%s/skip-ids-chunked: %s", vec.name, err); }
        }

        /* roundtrip */
        out->checks++;
        if (run_roundtrip(&vec, err, sizeof(err))) { out->failures++;
            if (!out->first_error[0]) snprintf(out->first_error, sizeof(out->first_error), "%s/roundtrip: %s", vec.name, err); }

        free_vector(&vec);
    }

    /* negative (invalid-UTF-8) conformance vectors */
    run_invalid_utf8(root, out);

    sofab_json_free(root);
    return out->failures ? -1 : 0;
}

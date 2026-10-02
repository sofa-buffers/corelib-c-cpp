/*!
 * @file consumer.c
 * @brief The reference consumer translation unit, written against the C API.
 *
 * SPDX-License-Identifier: MIT
 *
 * This file and its C++ twin (consumer.cpp) exist for one purpose: to make the
 * README's per-TU footprint table reproducible. That table reports what a
 * consumer's own translation unit costs -- not the library, which is measured by
 * tools/footprint.sh -- and the cost is paid per TU because the C++ wrapper is
 * header-only and its templates instantiate into the caller.
 *
 * The two files encode and decode the SAME message, so the numbers are
 * comparable: five scalar fields (u32, u16, bool, fp32 and the u32 that opens
 * the message), one nested sequence holding two scalars, and one string. Keep
 * them in step -- a field added to one and not the other silently turns the
 * table into a comparison of two different messages.
 *
 * Measured by tools/footprint-tu.sh, which compiles this file alone (-c) and
 * reports `size` on the object. Nothing links it, so the library calls stay
 * external references and only this TU's own code is counted.
 *
 * The round trip is `extern` and its results are consumed through a volatile,
 * so -Os cannot delete the work and leave an empty function behind.
 */

#include "sofab/istream.h"
#include "sofab/ostream.h"

#include <stdint.h>
#include <string.h>

/* What the decode produced, read back through a volatile so the optimiser has
 * to keep every store that feeds it. */
volatile uint32_t g_sink_u32;
volatile size_t   g_flushed;

/* --- the message ---------------------------------------------------------- */

typedef struct
{
    uint32_t id;
    float    value;
} child_t;

typedef struct
{
    uint32_t header;    /* id 1 */
    child_t  child;     /* id 2, a nested sequence */
    uint32_t footer;    /* id 3 */
    bool     flag;      /* id 4 */
    float    ratio;     /* id 5 */
    char     tag[17];   /* id 6, a string (16 + terminator) */
    uint16_t count;     /* id 7 */
} parent_t;

/* --- decode --------------------------------------------------------------- */

static void child_cb (sofab_istream_t *ctx, sofab_id_t id, size_t size,
                      size_t count, void *usrptr)
{
    child_t *c = (child_t *)usrptr;

    (void)size;
    (void)count;

    switch (id)
    {
        case 1: sofab_istream_read_u32(ctx, &c->id);     break;
        case 2: sofab_istream_read_fp32(ctx, &c->value); break;
        default: break;
    }
}

static void parent_cb (sofab_istream_t *ctx, sofab_id_t id, size_t size,
                       size_t count, void *usrptr)
{
    parent_t *p = (parent_t *)usrptr;
    /* One decoder per nested sequence, as sofab_istream_read_sequence requires.
     * Static rather than automatic because the decoder must outlive this
     * callback: the nested fields arrive on later feed() calls. */
    static sofab_istream_decoder_t child_decoder;

    (void)count;

    switch (id)
    {
        case 1: sofab_istream_read_u32(ctx, &p->header); break;
        case 2:
            sofab_istream_read_sequence(ctx, &child_decoder, &child_cb,
                                        &p->child);
            break;
        case 3: sofab_istream_read_u32(ctx, &p->footer); break;
        case 4: sofab_istream_read_bool(ctx, &p->flag);  break;
        case 5: sofab_istream_read_fp32(ctx, &p->ratio); break;
        case 6:
            if (size < sizeof(p->tag))
            {
                sofab_istream_read_string(ctx, p->tag, sizeof(p->tag));
            }
            break;
        case 7: sofab_istream_read_u16(ctx, &p->count); break;
        default: break;
    }
}

/* --- encode --------------------------------------------------------------- */

static void on_flush (sofab_ostream_t *ctx, const uint8_t *data, size_t len,
                      void *usrptr)
{
    (void)ctx;
    (void)data;
    (void)usrptr;

    g_flushed = len;
}

extern int sofab_tu_roundtrip (void)
{
    static uint8_t  buffer[256];
    sofab_ostream_t os;
    sofab_istream_t is;
    parent_t        decoded;

    memset(&decoded, 0, sizeof(decoded));

    sofab_ostream_init(&os, buffer, sizeof(buffer), 0, &on_flush, NULL);

    sofab_ostream_write_unsigned(&os, 1, 7u);
    sofab_ostream_write_sequence_begin_lazy(&os, 2);
    sofab_ostream_write_unsigned(&os, 1, 42u);
    sofab_ostream_write_fp32(&os, 2, 3.1415f);
    sofab_ostream_write_sequence_end(&os);
    sofab_ostream_write_unsigned(&os, 3, 99u);
    sofab_ostream_write_boolean(&os, 4, true);
    sofab_ostream_write_fp32(&os, 5, 2.5f);
    sofab_ostream_write_string(&os, 6, "tag42");
    sofab_ostream_write_unsigned(&os, 7, 3u);

    const size_t used = sofab_ostream_bytes_used(&os);

    sofab_istream_init(&is, &parent_cb, &decoded);
    if (sofab_istream_feed(&is, buffer, used) != SOFAB_RET_OK)
    {
        return -1;
    }

    g_sink_u32 = decoded.header + decoded.footer + decoded.child.id
               + (uint32_t)decoded.count + (uint32_t)decoded.flag
               + (uint32_t)decoded.ratio + (uint32_t)decoded.child.value
               + (uint32_t)(unsigned char)decoded.tag[0];

    return 0;
}

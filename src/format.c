/*
 * Copyright (c) 2015, Krisztian Litkey, <kli@iki.fi>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ripncode/ripncode.h>

static MRP_LIST_HOOK(formats);           /* list of known formats */


int rnc_format_init(rnc_t *rnc)
{
    static rnc_format_t builtin[] = {
#define BUILTIN(_type, _name)                                           \
        [RNC_ENCODING_##_type] = { { NULL,NULL }, _name, RNC_ENCODING_##_type }
        BUILTIN(UNKNOWN , "unknown" ),
        BUILTIN(SIGNED  , "signed"  ),
        BUILTIN(UNSIGNED, "unsigned"),
        BUILTIN(FLOATING, "floating"),
        BUILTIN(ALAW    , "alaw"    ),
        BUILTIN(ULAW    , "ulaw"    ),
#undef BUILTIN
    }, *f;

    mrp_list_init(&rnc->formats);
    mrp_list_move(&rnc->formats, &formats);

    for (f = builtin + RNC_ENCODING_ULAW; f >= builtin; f--) {
        mrp_list_init(&f->hook);
        mrp_list_prepend(&rnc->formats, &f->hook);
    }

    return 0;
}


int rnc_format_register(rnc_t *rnc, rnc_format_t *format)
{
    static int id = RNC_ENCODING_OTHER;

    mrp_list_init(&format->hook);
    format->id = id++;

    if (rnc == NULL)
        mrp_list_append(&formats, &format->hook);
    else
        mrp_list_append(&rnc->formats, &format->hook);

    return 0;
}


int rnc_format_lookup(rnc_t *rnc, const char *name)
{
    mrp_list_hook_t *p, *n;
    rnc_format_t    *f;

    mrp_list_foreach(&rnc->formats, p, n) {
        f = mrp_list_entry(p, typeof(*f), hook);

        if (!strcmp(f->name, name))
            return f->id;
    }

    return RNC_ENCODING_UNKNOWN;
}


const char *rnc_format_name(rnc_t *rnc, int id)
{
    mrp_list_hook_t *p, *n;
    rnc_format_t    *f;

    mrp_list_foreach(&rnc->formats, p, n) {
        f = mrp_list_entry(p, typeof(*f), hook);

        if (f->id == id)
            return f->name;
    }

    return NULL;
}

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

static char *builtins[] = { "PCM" };
static int   nbuiltin   = (int)MRP_ARRAY_SIZE(builtins);

static char **formats;                   /* known/registered formats */
int           nformat;                   /* number of known formats */


int rnc_format_init(rnc_t *rnc)
{
    rnc->formats = formats;
    rnc->nformat = nformat;

    formats = NULL;
    nformat = 0;

    return 0;
}


static void register_builtins(rnc_t *rnc)
{
    char **b = builtins;
    int    n = nbuiltin;
    int    i;

    nbuiltin = 0;
    for (i = 0; i < n; i++)
        if (rnc_compress_register(rnc, b[i]) < 0)
            exit(-1);
}


int rnc_compress_register(rnc_t *rnc, const char *name)
{
    char **f;
    int    n, id;

    if (nformat == 0)
        register_builtins(rnc);

    if (rnc == NULL) {
        f = formats;
        n = nformat;
    }
    else {
        f = rnc->formats;
        n = rnc->nformat;
    }

    if (!mrp_reallocz(f, n, n + 1))
        return -1;

    f[n] = mrp_strdup(name);

    if (f[n] == NULL)
        return -1;

    id = n++;

    if (rnc == NULL) {
        formats = f;
        nformat = n;
    }
    else {
        rnc->formats = f;
        rnc->nformat = n;
    }

    return id;
}


int rnc_compress_id(rnc_t *rnc, const char *name)
{
    char **f = rnc ? rnc->formats : formats;
    int    n = rnc ? rnc->nformat : nformat;
    int    i;

    for (i = 0; i < n; i++)
        if (!strcmp(f[i], name))
            return i;

    return -1;
}


const char *rnc_compress_name(rnc_t *rnc, int id)
{
    char **f = rnc ? rnc->formats : formats;
    int    n = rnc ? rnc->nformat : nformat;

    if (0 <= id && id < n)
        return f[id];

    return NULL;
}

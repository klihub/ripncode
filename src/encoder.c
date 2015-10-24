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

#include <errno.h>

#include <ripncode/ripncode.h>

static MRP_LIST_HOOK(encoders);


int rnc_encoder_init(rnc_t *rnc)
{
    rnc_enc_api_t   *api;
    mrp_list_hook_t *p, *n;
    int              i;

    mrp_list_init(&rnc->encoders);
    mrp_list_move(&rnc->encoders, &encoders);

    mrp_list_foreach(&rnc->encoders, p, n) {
        api = mrp_list_entry(p, typeof(*api), hook);

        for (i = 0; api->types[i] != NULL; i++) {
            if (rnc_compress_register(rnc, api->types[i]) < 0)
                return -1;
        }
    }

    return 0;
}


int rnc_encoder_register(rnc_t *rnc, const char *name, rnc_enc_api_t *api)
{
    int i;

    mrp_list_init(&api->hook);

    if (api->name == NULL)
        api->name = name;

    if (rnc == NULL)
        mrp_list_append(&encoders, &api->hook);
    else {
        mrp_list_append(&rnc->encoders, &api->hook);

        for (i = 0; api->types[i] != NULL; i++) {
            if (rnc_compress_register(rnc, api->types[i]) < 0)
                return -1;
        }
    }

    return 0;
}


static rnc_enc_api_t *api_lookup(rnc_t *rnc, const char *type)
{
    rnc_enc_api_t   *a;
    mrp_list_hook_t *p, *n;
    int              i;

    mrp_list_foreach(&rnc->encoders, p, n) {
        a = mrp_list_entry(p, typeof(*a), hook);

        for (i = 0; a->types[i] != NULL; i++)
            if (!strcmp(type, a->types[i]))
                return a;
    }

    return NULL;
}


rnc_encoder_t *rnc_encoder_create(rnc_t *rnc, uint32_t format)
{
    rnc_encoder_t *enc;
    rnc_enc_api_t *api;
    const char    *type;

    type = rnc_compress_name(rnc, RNC_FORMAT_CMPR(format));

    if (type == NULL)
        goto invalid;

    api = api_lookup(rnc, type);

    if (api == NULL)
        goto invalid;

    enc = mrp_allocz(sizeof(*enc));

    if (enc == NULL)
        goto nomem;

    enc->rnc = rnc;
    enc->api = api;

    if (api->create(enc, format) != 0)
        goto failed;

    return enc;

 invalid:
    errno = EINVAL;
 nomem:
    return NULL;

 failed:
    mrp_free(enc);
    return NULL;
}


void rnc_encoder_destroy(rnc_encoder_t *enc)
{
    if (enc == NULL)
        return;

    enc->api->close(enc);

    mrp_free(enc);
}


int rnc_encoder_set_quality(rnc_encoder_t *enc, uint16_t qlty, uint16_t cmpr)
{
    if (enc->api == NULL)
        goto invalid;

    if (enc->open)
        goto busy;

    return enc->api->set_quality(enc, qlty, cmpr);

 invalid:
    errno = EINVAL;
    return -1;

 busy:
    errno = EBUSY;
    return -1;
}


int rnc_encoder_set_metadata(rnc_encoder_t *enc, rnc_meta_t *meta)
{
    if (enc->api == NULL)
        goto invalid;

    return enc->api->set_metadata(enc, meta);

 invalid:
    errno = EINVAL;
    return -1;
}


int rnc_encoder_write(rnc_encoder_t *enc, void *buf, size_t size)
{
    if (enc->api == NULL)
        goto invalid;

    if (!enc->open) {
        if (enc->api->open(enc) < 0)
            return -1;

        enc->open = 1;
    }

    return enc->api->write(enc, buf, size);

 invalid:
    errno = EINVAL;
    return -1;
}


int rnc_encoder_finish(rnc_encoder_t *enc)
{
    if (enc->api == NULL)
        goto invalid;

    return enc->api->finish(enc);

 invalid:
    errno = EINVAL;
    return -1;
}


int rnc_encoder_read(rnc_encoder_t *enc, void *buf, size_t size)
{
    if (enc->api == NULL)
        goto invalid;

    return enc->api->read(enc, buf, size);

 invalid:
    errno = EINVAL;
    return -1;
}

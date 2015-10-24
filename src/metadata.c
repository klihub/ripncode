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

static MRP_LIST_HOOK(metadbs);


int rnc_meta_init(rnc_t *rnc)
{
    mrp_list_init(&rnc->metadbs);
    mrp_list_move(&rnc->metadbs, &metadbs);

    return 0;
}


int rnc_meta_register(rnc_t *rnc, rnc_meta_api_t *api)
{
    mrp_list_init(&api->hook);

    if (rnc == NULL)
        mrp_list_append(&metadbs, &api->hook);
    else
        mrp_list_append(&rnc->metadbs, &api->hook);

    return 0;
}


static rnc_meta_api_t *api_lookup(rnc_t *rnc, const char *type)
{
    rnc_meta_api_t   *a;
    mrp_list_hook_t *p, *n;

    mrp_list_foreach(&rnc->metadbs, p, n) {
        a = mrp_list_entry(p, typeof(*a), hook);

        if (!strcmp(a->type, type))
            return a;
    }

    return NULL;
}


rnc_metadb_t *rnc_meta_create(rnc_t *rnc, const char *type)
{
    rnc_meta_api_t *api;
    rnc_metadb_t   *db;

    api = api_lookup(rnc, type);

    if (api == NULL)
        goto invalid;

    db = mrp_allocz(sizeof(*db));

    if (db == NULL)
        goto nomem;

    db->api = api;
    db->rnc = rnc;

    if (db->api->create(db) < 0)
        goto failed;

    return db;

 invalid:
    errno = EINVAL;
 nomem:
    return NULL;

 failed:
    mrp_free(db);
    return NULL;
}


int rnc_meta_open(rnc_metadb_t *db, const char **options)
{
    if (db == NULL)
        goto invalid;

    return db->api->open(db, options);

 invalid:
    errno = EINVAL;
    return -1;
}


void rnc_meta_close(rnc_metadb_t *db)
{
    if (db == NULL)
        return;

    db->api->close(db);

    mrp_free(db);
}


rnc_meta_t *rnc_meta_lookup(rnc_metadb_t *db, int track)
{
    if (db == NULL)
        goto invalid;

    return db->api->lookup(db, track);

 invalid:
    errno = EINVAL;
    return NULL;
}


void rnc_meta_free(rnc_meta_t *m)
{
    if (m == NULL)
        return;

    mrp_free((char *)m->title);
    mrp_free((char *)m->album);
    mrp_free((char *)m->artist);
    mrp_free((char *)m->genre);
    mrp_free((char *)m->isrc);
    mrp_free((char *)m->performer);
    mrp_free((char *)m->copyright);
    mrp_free((char *)m->license);
    mrp_free((char *)m->organization);

    mrp_free(m);
}

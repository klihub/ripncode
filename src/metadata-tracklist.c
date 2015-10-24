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

#include <ripncode.h>


/*
 * data read from a tracklist file
 */
typedef struct {
    char  *album;
    char  *artist;
    char  *genre;
    int    year;
    char **tracks;
    int    ntrack;
} tracklist_t;


static void tracklist_free(tracklist_t *t)
{
    int i;

    if (t == NULL)
        return;

    mrp_free(t->album);
    mrp_free(t->artist);
    mrp_free(t->genre);
    for (i = 0; i < t->ntrack; i++)
        mrp_free(t->tracks[i]);
    mrp_free(t->tracks);

    mrp_free(t);
}


static tracklist_t *tracklist_parse(const char *path)
{
#define CHECK_STRTAG(_tag, _member)             \
    if (!strcasecmp(tag, _tag)) {               \
        mrp_free(t->_member);                   \
        t->_member = mrp_strdup(value);         \
        continue;                               \
    }

#define CHECK_INTTAG(_tag, _member)                             \
    if (!strcasecmp(tag, _tag)) {                               \
        t->_member = (int)strtoul(value, &e, 10);               \
        if (e && *e)                                            \
            goto invalid;                                       \
        continue;                                               \
    }

    tracklist_t *t;
    char         buf[16 * 1024], *tag, *sep, *value, *p, *e;
    int          fd, n;

    fd = open(path, O_RDONLY);

    if (fd < 0)
        goto failed;

    n = read(fd, buf, sizeof(buf) - 1);

    close(fd);

    if (n < 0)
        goto failed;

    if (n >= (int)sizeof(buf) - 1)
        goto nospace;

    buf[n] = '\0';

    t = mrp_allocz(sizeof(*t));

    if (t == NULL)
        goto nomem;

    p = buf;
    while (p && *p) {
        while (*p == ' ' || *p == '\t')
            p++;

        if (*p == '\n') {
            p++;
            continue;
        }

        tag = p;

        while (*p && *p != ':' && *p != '.' && *p != '\n')
            p++;

        if (*p == '\n')
            goto invalid;

        sep = p;
        p++;

        while (*p == ' ' || *p == '\t')
            p++;

        value = p;

        while (*p && *p != '\n')
            p++;

        if (p != '\0')
            *p++ = '\0';

        e = p - 1;
        while (e > value && (*e == ' ' || *e == '\t'))
            *e-- = '\0';

        if (*sep == ':') {
            *sep = '\0';

            CHECK_STRTAG("Album" , album);
            CHECK_STRTAG("Artist", artist);
            CHECK_STRTAG("Genre" , genre);
            CHECK_INTTAG("Year"  , year);

            mrp_log_warning("Ignoring unknown tag '%s'...", tag);
            continue;
        }

        if (*sep == '.') {
            *sep = '\0';

            n = strtoul(tag, &e, 10);

            if (e && *e)
                goto invalid;

            if (n < 1 || n > 99)         /* max. CDDA tracks */
                goto invalid;

            if (n > t->ntrack) {
                if (!mrp_reallocz(t->tracks, t->ntrack, n))
                    goto nomem;
                t->ntrack = n;
            }

            mrp_free(t->tracks[n - 1]);
            t->tracks[n - 1] = mrp_strdup(value);
        }
    }

    return t;

 nospace:
    errno = ENOBUFS;
 failed:
 nomem:
    return NULL;

 invalid:
    errno = EINVAL;
    tracklist_free(t);
    return NULL;
}


static int tracklist_create(rnc_metadb_t *db)
{
    db->data = NULL;

    return 0;
}


static int tracklist_open(rnc_metadb_t *db, const char **options)
{
    MRP_UNUSED(options);

    db->data = tracklist_parse("./tracklist");

    return db->data != NULL ? 0 : -1;
}


static void tracklist_close(rnc_metadb_t *db)
{
    tracklist_free(db->data);
    db->data = NULL;
}


static rnc_meta_t *tracklist_lookup(rnc_metadb_t *db, int track)
{
    tracklist_t *t = (tracklist_t *)db->data;
    rnc_meta_t  *m;

    if (t == NULL)
        goto invalid;

    track--;

    if (track < 0 || track >= t->ntrack)
        goto noentry;

    m = mrp_allocz(sizeof(*m));

    if (m == NULL)
        goto nomem;

    m->track  = track;
    m->title  = mrp_strdup(t->tracks[track]);
    m->album  = mrp_strdup(t->album);
    m->artist = mrp_strdup(t->artist);
    m->genre  = mrp_strdup(t->genre);

    m->date.tm_year = t->year;

    return m;

 invalid:
    errno = EINVAL;
    return NULL;
 noentry:
    errno = ENOENT;
 nomem:
    return NULL;
}


RNC_META_REGISTER(tracklist, {
        .type   = "tracklist",
        .create = tracklist_create,
        .open   = tracklist_open,
        .close  = tracklist_close,
        .lookup = tracklist_lookup,
    });

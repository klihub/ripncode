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

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>

#include <ripncode/ripncode.h>
#include <ripncode/setup.h>

#define rnc_fatal(_r, ...) do {                         \
        mrp_log_error("fatal error: " __VA_ARGS__);     \
        exit(1);                                        \
} while (0)

#define rnc_warning(_r, ...) do {                       \
        mrp_log_warning("warning: " __VA_ARGS__);       \
    } while (0)

#define rnc_error(_r, ...) do {                         \
        mrp_log_error("error: " __VA_ARGS__);           \
    } while (0)

#define rnc_info(_r, ...) do {                          \
        mrp_log_info(__VA_ARGS__);                      \
    } while (0);



static rnc_t *rnc_init(int argc, char *argv[], char *envp[])
{
    static rnc_t rnc;

    mrp_clear(&rnc);
    rnc_format_init(&rnc);
    rnc_device_init(&rnc);
    rnc_encoder_init(&rnc);
    rnc_meta_init(&rnc);

    rnc_cmdline_parse(&rnc, argc, argv, envp);

    return &rnc;
}


static void discover_tracks(rnc_t *rnc)
{
    int n;

    rnc->dev = rnc_device_open(rnc, rnc->device);

    if (rnc->dev == NULL)
        rnc_fatal(rnc, "failed to open device '%s'", rnc->device);

    rnc->ntrack = rnc_device_get_tracks(rnc->dev, NULL, 0);

    if (rnc->ntrack <= 0)
        rnc_fatal(rnc, "failed to find any audio tracks on '%s'", rnc->device);

    rnc->tracks = mrp_allocz_array(typeof(rnc->tracks[0]), rnc-> ntrack);

    if (rnc->tracks == NULL)
        rnc_fatal(rnc, "failed to allocate %d tracks", rnc->ntrack);

    n = rnc_device_get_tracks(rnc->dev, rnc->tracks, rnc->ntrack);

    if (n != rnc->ntrack)
        rnc_fatal(rnc, "inconsistent tracks read from '%s'", rnc->device);

    if (rnc->speed)
        rnc_device_set_speed(rnc->dev, rnc->speed);
}


int encode_track(rnc_t *rnc, rnc_track_t *t)
{
    rnc_encoder_t *enc;
    rnc_meta_t    *meta;
    int            cmpr, cmap, chnl, rate, bits, smpl, endn, fid;
    int            blksize, bufsize, n, i;
    char          *buf;
    double         gain, peak, loud, range;

    if (rnc_device_seek(rnc->dev, t, 0) < 0) {
        rnc_error(rnc, "failed to seek to beginning of track #%d", t->id);
        return -1;
    }

    cmpr = rnc_compress_id(rnc, rnc->format);
    cmap = RNC_CHANNELMAP_LEFTRIGHT;
    chnl = 2;
    rate = RNC_SAMPLERATE_44100;
    bits = 16;
    smpl = RNC_SAMPLE_SIGNED;
    endn = RNC_ENDIAN_LITTLE;

    if (cmpr < 0) {
        rnc_error(rnc, "failed to find encoder for format '%s'", rnc->format);
        return -1;
    }

    fid = RNC_FORMAT_ID(cmap, cmpr, chnl, rate, bits, smpl, endn);
    enc = rnc_encoder_create(rnc, fid);

    if (enc == NULL) {
        rnc_error(rnc, "failed to create encoder for format '%s'", rnc->format);
        return -1;
    }

    rnc_encoder_set_quality(enc, 0xffffU, 0xffffU);

    meta = rnc_meta_lookup(rnc->db, t->id);

    if (meta != NULL)
        rnc_encoder_set_metadata(enc, meta);

    if (rnc->gain == NULL) {
        rnc->gain = rnc_gain_create(rnc->ntrack, fid);

        if (rnc->gain == NULL)
            rnc_error(&rnc, "failed to initialize replaygain calculation");
    }

    blksize = rnc_device_get_blocksize(rnc->dev);
    bufsize = (256 + 128) * blksize;
    buf     = alloca(bufsize);

    for (i = 0; i < (int)t->nblk; i += n / blksize) {
        n = rnc_device_read(rnc->dev, buf, bufsize);

        if (n < 0) {
            rnc_error(rnc, "failed to read block #%d of track #%d", i, t->id);
            goto fail;
        }

        if (rnc_encoder_write(enc, buf, n) < 0) {
            rnc_error(rnc, "failed to encode blocks #%d-%d of track #%d",
                      i, i + bufsize / blksize, t->id);
            goto fail;
        }

        if (rnc_gain_analyze(rnc->gain, t->idx, buf, n / (2 * 2)) < 0)
            rnc_error(rnc, "replaygain analysis failed");

        printf("\rtrack #%d: %.2f %%", t->id,
               (100.0 * (i + n / blksize)) / t->nblk);
        fflush(stdout);
    }

    loud  = rnc_gain_track_loudness(rnc->gain, t->idx);
    range = rnc_gain_track_range(rnc->gain, t->idx);
    gain  = rnc_gain_track_gain(rnc->gain, t->idx);
    peak  = rnc_gain_track_peak(rnc->gain, t->idx);

    if (meta != NULL) {
        meta->track_gain = gain;
        meta->track_peak = peak;

        rnc_encoder_set_metadata(enc, meta);
    }

    if (rnc_encoder_finish(enc) < 0) {
        rnc_error(rnc, "failed to finalize encoding of track #%d", t->id);
        goto fail;
    }

    rnc_meta_free(meta);

    printf("\rtrack #%d: done     \n", t->id);
    printf("    loudness: %2.2f, range: %2.2f, peak: %2.2f, replaygain: %2.2f\n",
           loud, range, peak, gain);
    fflush(stdout);

    rnc->enc = enc;
    return 0;

 fail:
    rnc_encoder_destroy(enc);
    return -1;
}


int write_track(rnc_t *rnc, rnc_track_t *t)
{
    char path[PATH_MAX], buf[64 * 1024];
    int  r, w, n, fd;

    n = snprintf(path, sizeof(path), "%s-%d.%s", rnc->output, t->id,
                 rnc->format);

    if (n < 0 || n >= (int)sizeof(path)) {
        rnc_error(rnc, "invalid output file name for track #%d", t->id);
        goto fail;
    }

    fd = open(path, O_WRONLY | O_CREAT, 0644);

    if (fd < 0) {
        rnc_error(rnc, "failed to open '%s'", path);
        goto fail;
    }

    while ((r = rnc_encoder_read(rnc->enc, buf, sizeof(buf))) > 0) {
        w = 0;

        while (w < r) {
            n = write(fd, buf + w, r - w);

            if (n < 0) {
                if (errno == EINTR)
                    continue;
                else {
                    rnc_error(rnc, "failed to write to '%s' (%d: %s)", path,
                              errno, strerror(errno));
                    goto fail;
                }
            }

            w += n;
        }
    }

    close(fd);
    rnc_encoder_destroy(rnc->enc);
    rnc->enc = NULL;

    return 0;

 fail:
    rnc_encoder_destroy(rnc->enc);
    rnc->enc = NULL;

    return -1;
}


int rip_track(rnc_t *rnc, int idx)
{
    if (encode_track(rnc, rnc->tracks + idx) < 0)
        return -1;

    if (write_track(rnc, rnc->tracks + idx) < 0)
        return -1;

    return 0;
}


void select_tracks(rnc_t *rnc, int *first, int *last)
{
    char *e;

    if (!strcmp(rnc->rip, "all")) {
        *first = 0;
        *last  = rnc->ntrack - 1;

        return;
    }

    *first = strtoul(rnc->rip, &e, 10);

    if (*first < 1)
        goto invalid_tracks;

    if (!*e)
        *last = *first;
    else {
        if (*e && *e != '-')
            goto invalid_tracks;

        *last = strtoul(e + 1, &e, 10);

        if (*last < 1 || *e)
            goto invalid_tracks;
    }

    if (*last > rnc->ntrack)
        *last = rnc->ntrack;

    *first -= 1;
    *last  -= 1;

    return;

 invalid_tracks:
    rnc_fatal(rnc, "invalid track selection: '%s'", rnc->rip);
}


int fetch_metadata(rnc_t *rnc)
{
    rnc_track_t *t;
    rnc_meta_t  *m;
    int          i;

    rnc->db = rnc_meta_create(rnc, "tracklist");

    if (rnc->db == NULL) {
        rnc_warning(rnc, "failed to create tracklist metadata DB.");
        return -1;
    }

    if (rnc_meta_open(rnc->db, NULL) < 0) {
        rnc_warning(rnc, "failed to open tracklist metadata DB.");
        return -1;
    }

    for (i = 0; i < rnc->ntrack; i++) {
        t = rnc->tracks + i;

        m = rnc_meta_lookup(rnc->db, t->id);

        if (m == NULL) {
            rnc_warning(rnc, "failed to look up metadata for track #%d", t->id);
            continue;
        }

        printf("Track #%d: %s\n", t->id, m->title ? m->title : "unknown");

        rnc_meta_free(m);
    }

    return 0;
}


int main(int argc, char *argv[], char *envp[])
{
    rnc_t *rnc;
    rnc_track_t *t;
    int i, first, last, rip;

    rnc = rnc_init(argc, argv, envp);

    printf("input:  %s\n", rnc->device);
    printf("speed:  %d\n", rnc->speed);
    printf("output: %s\n", rnc->output);
    printf("format: %s\n", rnc->format ?  rnc->format : "flac");
    printf("tracks: %s\n", rnc->rip ? rnc->rip : "all");

    discover_tracks(rnc);
    select_tracks(rnc, &first, &last);
    fetch_metadata(rnc);

    printf("Track Selection:\n");
    for (i = 0; i < rnc->ntrack; i++) {
        t   = rnc->tracks + i;
        rip = first <= i && i <= last;

        printf("    #%2.2d (%s): %d min %2.2d sec, blocks %d - %d\n", t->id,
               rip ? "*" : "-", ((int)t->length / 60), ((int)t->length % 60),
               t->fblk, t->fblk + t->nblk - 1);
    }

    for (i = first; i <= last; i++)
        rip_track(rnc, i);

    printf("album gain: %2.2f dB\n", rnc_gain_album_gain(rnc->gain));


    return 0;
}

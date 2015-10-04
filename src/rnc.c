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

#include <ripncode/ripncode.h>
#include <ripncode/setup.h>

#define rnc_fatal(_r, ...) do {                         \
        mrp_log_error("fatal error: " __VA_ARGS__);     \
        exit(1);                                        \
} while (0)


#define rnc_info(_r, ...) do {                  \
        mrp_log_info(__VA_ARGS__);              \
    } while (0);


int main(int argc, char *argv[], char *envp[])
{
    rnc_t rnc;
    rnc_track_t tracks[64];
    int ntrack, i, j, fdf, fdr;
    int flac, format, bs;
    char *sbuf;
    int r, w, n;

    mrp_clear(&rnc);

    rnc_format_init(&rnc);
    rnc_device_init(&rnc);
    rnc_encoder_init(&rnc);

    rnc_cmdline_parse(&rnc, argc, argv, envp);

    printf("input:  %s\n", rnc.device);
    printf("output: %s\n", rnc.output);
    printf("format: %s\n", rnc.format ?  rnc.format : "flac");
    printf("tracks: %s\n", rnc.rip ? rnc.rip : "all");

    rnc.dev = rnc_device_open(&rnc, rnc.device);

    if (rnc.dev == NULL)
        rnc_fatal(&rnc, "failed to open device '%s'", rnc.device);

    rnc_info(&rnc, "%s opened successfully...", rnc.device);

    ntrack = rnc_device_get_tracks(rnc.dev, tracks, MRP_ARRAY_SIZE(tracks));

    if (ntrack < 0)
        rnc_fatal(&rnc, "failed to get tracklist");

    for (i = 0; i < ntrack; i++) {
        printf("#%d: %d:%2.2d, %d block starting at %d\n", i,
               (int)(tracks[i].length / 60), ((int)tracks[i].length % 60),
               tracks[i].nblk, tracks[i].fblk);
    }

    i = 0;
    if (rnc_device_seek(rnc.dev, tracks + i, 0) < 0)
        rnc_fatal(&rnc, "failed to seek to beginning of track #%d.", i);

    flac = rnc_format_lookup(&rnc, "flac");

    if (flac == RNC_ENCODING_UNKNOWN)
        rnc_fatal(&rnc, "failed to look up FLAC format id");

    format  = RNC_FORMAT_ID(flac, 44100, 2, 16, RNC_ENDIAN_LITTLE);
    rnc.enc = rnc_encoder_create(&rnc, format);

    if (rnc.enc == NULL)
        rnc_fatal(&rnc, "failed to open FLAC encoder");

    if (rnc_encoder_set_quality(rnc.enc, 0xffffU, 0xffffU) < 0)
        rnc_fatal(&rnc, "failed to set FLAC encoder quality");

    if (rnc_encoder_open(rnc.enc) < 0)
        rnc_fatal(&rnc, "failed to open FLAC encoder");

    bs   = rnc_device_get_blocksize(rnc.dev);
    sbuf = alloca(bs);

    if ((fdr = open("test.raw", O_WRONLY | O_CREAT, 0644)) < 0)
        rnc_fatal(&rnc, "failed to open test.raw");

    for (j = 0; j < (int)tracks[i].nblk; j++) {
        if ((n = rnc_device_read(rnc.dev, sbuf, bs)) < 0)
            rnc_fatal(&rnc, "failed to read data from device (%d: %s)",
                      errno, strerror(errno));

        if (rnc_encoder_write(rnc.enc, sbuf, bs) < 0)
            rnc_fatal(&rnc, "failed to write to FLAC encoder");

        if ((n = write(fdr, sbuf, bs)) < 0)
            rnc_fatal(&rnc, "failed to write to RAW data");
    }

    close(fdr);

    if (rnc_encoder_finish(rnc.enc) < 0)
        rnc_fatal(&rnc, "failed to properly finish FLAC encoding");

    if ((fdf = open("test.flac", O_WRONLY|O_CREAT, 0644)) < 0)
        rnc_fatal(&rnc, "failed to open test.flac");

    while ((r = rnc_encoder_read(rnc.enc, sbuf, bs)) > 0) {
        w = 0;
        while (w < r) {
            n = write(fdf, sbuf + w, r - w);

            if (n < 0) {
                if (errno == EINTR || errno == EAGAIN)
                    continue;
                else
                    rnc_fatal(&rnc, "failed to write to test.flac");
            }

            w += n;
        }
    }

    close(fdf);

    return 0;
}

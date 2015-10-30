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
#include <byteswap.h>
#include <ebur128.h>

#include <ripncode/ripncode.h>

#define REPLAYGAIN_REFERENCE (-18.0)

struct rnc_gain_s {
    int             ntrack;              /* number of tracks on album */
    ebur128_state **ebur;                /* libebur128 state */
    int             rate;                /* rate */
    int             bits;                /* bits per sample */
    int             smpl;                /* sample type */
    int             swap;                /* whether to swap endianness */
};


int rnc_gain_init(rnc_gain_t *g, int ntrack, uint32_t format)
{
    int cmap, chnl, rate, bits, smpl, endn, swap, host, i, mode;

    mrp_clear(g);

    cmap = RNC_FORMAT_CMAP(format);
    chnl = RNC_FORMAT_CHNL(format);
    rate = RNC_FORMAT_RATE(format);
    bits = RNC_FORMAT_BITS(format);
    smpl = RNC_FORMAT_SMPL(format);
    endn = RNC_FORMAT_ENDN(format);


    /*
     * we only support 16-bit stereo with left-right channel map
     */

    if (cmap != RNC_CHANNELMAP_LEFTRIGHT)
        goto unsupported_channelmap;

    if (chnl != 2)
        goto unsupported_channels;

    if (bits != 16)
        goto unsupported_bits;

    if (smpl != RNC_SAMPLE_SIGNED)
        goto unsupported_samples;

    host = 0x1;
    if (( *(char *)&host && endn == RNC_ENDIAN_BIG   ) ||
        (!*(char *)&host && endn == RNC_ENDIAN_LITTLE))
        swap = 1;

    g->rate = rate;
    g->bits = bits;
    g->smpl = smpl;
    g->swap = swap;

    g->ntrack = ntrack;
    g->ebur   = mrp_allocz(ntrack * sizeof(g->ebur[0]));

    if (g->ebur == NULL)
        goto nomem;

    mode = EBUR128_MODE_I | EBUR128_MODE_LRA | EBUR128_MODE_SAMPLE_PEAK;

    for (i = 0; i < ntrack; i++) {
        g->ebur[i] = ebur128_init(chnl, rnc_id_freq(rate), mode);

        if (g->ebur[i] == NULL)
            goto lib_error;

        ebur128_set_channel(g->ebur[i], 0, EBUR128_LEFT);
        ebur128_set_channel(g->ebur[i], 1, EBUR128_RIGHT);
    }

    return 0;

 unsupported_channelmap:
 unsupported_channels:
 unsupported_bits:
 unsupported_samples:
    errno = EOPNOTSUPP;
    return -1;

 lib_error:
    errno = EINVAL;
 nomem:
    if (g->ebur != NULL) {
        for (i = 0; i < ntrack; i++) {
            if (g->ebur[i] != NULL)
                ebur128_destroy(g->ebur + i);
        }
        mrp_free(g->ebur);
    }
    return -1;
}


void rnc_gain_exit(rnc_gain_t *g)
{
    int i;

    if (g == NULL)
        return;

    for (i = 0; i < g->ntrack; i++)
        ebur128_destroy(g->ebur + i);
}


rnc_gain_t *rnc_gain_create(int ntrack, uint32_t format)
{
    rnc_gain_t *g;

    if ((g = mrp_allocz(sizeof(*g))) == NULL)
        goto fail;

    if (rnc_gain_init(g, ntrack, format) < 0)
        goto fail;

    return g;

 fail:
    mrp_free(g);
    return NULL;
}


void rnc_gain_destroy(rnc_gain_t *g)
{
    rnc_gain_exit(g);
    mrp_free(g);
}


int rnc_gain_analyze(rnc_gain_t *g, int track, char *samples, int nsample)
{
    ebur128_state *ebur = g->ebur[track];
    int            r;

    r = ebur128_add_frames_short(ebur, (short *)samples, (size_t)nsample);

    if (r != EBUR128_SUCCESS)
        goto lib_error;

    return 0;

 lib_error:
    errno = EINVAL;
    return -1;
}


static double replaygain(double loudness)
{
    double gain = REPLAYGAIN_REFERENCE - loudness;

    if (gain < -51.0)
        gain = -51.0;
    else if (gain > 51.0)
        gain = 51.0;

    return gain;
}


double rnc_gain_track_range(rnc_gain_t *g, int track)
{
    double range;

    if (track >= g->ntrack)
        goto invalid_track;

    ebur128_loudness_range(g->ebur[track], &range);

    return range;

 invalid_track:
    errno = EINVAL;
    return 0.0;

}


double rnc_gain_track_loudness(rnc_gain_t *g, int track)
{
    double loudness;

    if (track >= g->ntrack)
        goto invalid_track;

    ebur128_loudness_global(g->ebur[track], &loudness);

    return loudness;

 invalid_track:
    errno = EINVAL;
    return 0.0;

}


double rnc_gain_track_gain(rnc_gain_t *g, int track)
{
    double loudness;

    ebur128_loudness_global(g->ebur[track], &loudness);

    return replaygain(loudness);
}


double rnc_gain_track_peak(rnc_gain_t *g, int track)
{
    double l, r;

    if (track >= g->ntrack)
        goto invalid_track;

    ebur128_sample_peak(g->ebur[track], 0, &l);
    ebur128_sample_peak(g->ebur[track], 1, &r);

    return l > r ? l : r;

 invalid_track:
    errno = EINVAL;
    return 0.0;
}


double rnc_gain_album_gain(rnc_gain_t *g)
{
    double loudness;

    ebur128_loudness_global_multiple(g->ebur, g->ntrack, &loudness);

    return replaygain(loudness);
}

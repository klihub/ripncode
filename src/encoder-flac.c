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
#include <FLAC/stream_encoder.h>
#include <FLAC/metadata.h>

#include <murphy/common/debug.h>
#include <ripncode/ripncode.h>


#define BUFFER_CHUNK (64 * 1024)

typedef struct {
    FLAC__StreamEncoder *enc;
    rnc_enc_data_cb_t    data_cb;
    int                  chnl;
    int                  bits;
    rnc_buf_t           *buf;
    uint32_t             gain_offs;
    double               track_gain;
    double               track_peak;
    double               album_gain;
    int                  swap : 1;
    int                  busy : 1;
} flen_t;


static FLAC__StreamEncoderWriteStatus \
    __flen_write(const FLAC__StreamEncoder *se, const FLAC__byte buffer[],
                 size_t bytes, unsigned samples, unsigned current_frame,
                 void *client_data);

static FLAC__StreamEncoderSeekStatus \
    __flen_seek(const FLAC__StreamEncoder *se, FLAC__uint64 abs_offset,
                void *client_data);

static FLAC__StreamEncoderTellStatus \
    __flen_tell(const FLAC__StreamEncoder *se, FLAC__uint64 *abs_offset,
                void *client_data);

static void __flen_meta(const FLAC__StreamEncoder *se,
                        const FLAC__StreamMetadata *meta, void *client_data);



int flen_create(rnc_encoder_t *enc, uint32_t format)
{
    flen_t *fe;
    FLAC__StreamEncoder *se;
    int chnl, rate, bits, smpl, endn, one;

    mrp_debug("creating FLAC encoder for format 0x%x", format);

    fe = mrp_allocz(sizeof(*fe));

    if (fe == NULL)
        goto nomem;

    se = fe->enc = FLAC__stream_encoder_new();

    if (enc == NULL)
        goto nomem;

    chnl = RNC_FORMAT_CHNL(format);
    rate = rnc_id_freq(RNC_FORMAT_RATE(format));
    bits = RNC_FORMAT_BITS(format);
    smpl = RNC_FORMAT_SMPL(format);
    endn = RNC_FORMAT_ENDN(format);

    if (smpl != RNC_SAMPLE_SIGNED) /* XXX should convert instead */
        goto invalid;

    fe->buf = rnc_buf_create("FLAC-encoder", 0, BUFFER_CHUNK);

    if (fe->buf == NULL)
        goto nomem;

    one = 1;
    if ((endn == RNC_ENDIAN_BIG    &&  *((char *)&one)) ||
        (endn == RNC_ENDIAN_LITTLE && !*((char *)&one)))
        fe->swap = true;

    enc->data = fe;
    fe->chnl  = chnl;
    fe->bits  = bits;

    mrp_debug("setting stream to %d Hz, %d channels, %d bits", rate,
              chnl, bits);

    if (!FLAC__stream_encoder_set_sample_rate(se, (unsigned)rate))
        goto invalid;

    if (!FLAC__stream_encoder_set_channels(se, (unsigned)chnl))
        goto invalid;

    if (!FLAC__stream_encoder_set_bits_per_sample(se, (unsigned)bits))
        goto invalid;

    if (!FLAC__stream_encoder_set_compression_level(se, 8))
        goto invalid;

    if (!FLAC__stream_encoder_set_blocksize(se, 0))
        goto invalid;

    return 0;

 invalid:
    if (se)
        FLAC__stream_encoder_delete(se);
    mrp_free(fe);
    errno = EINVAL;
    return -1;

 nomem:
    mrp_free(fe);
    return -1;
}


int flen_open(rnc_encoder_t *enc)
{
    flen_t *fe;
    FLAC__StreamEncoder *se;
    int status;

    mrp_debug("opening FLAC encoder");

    if ((fe = enc->data) == NULL || (se = fe->enc) == NULL)
        goto invalid;

    status = FLAC__stream_encoder_init_stream(se, __flen_write, __flen_seek,
                                              __flen_tell, __flen_meta, enc);

    if (status != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
        goto invalid;

    return 0;

 invalid:
    errno = EINVAL;
    return -1;
}


void flen_close(rnc_encoder_t *enc)
{
    flen_t *fe;
    FLAC__StreamEncoder *se;

    mrp_debug("closing FLAC encoder %p", enc);

    if (enc == NULL || (fe = enc->data) == NULL || (se = fe->enc) == NULL)
        return;

    FLAC__stream_encoder_delete(se);
    enc->data = NULL;
}


int flen_set_quality(rnc_encoder_t *enc, uint16_t qlty, uint16_t cmpr)
{
    flen_t *fe;
    FLAC__StreamEncoder *se;

    MRP_UNUSED(qlty);
    MRP_UNUSED(cmpr);

    mrp_debug("setting FLAC quality/compression to %u/%u", qlty, cmpr);

    if (enc == NULL || (fe = enc->data) == NULL || (se = fe->enc) == NULL)
        goto invalid;


    /* we could cmpr = (int)ceil(cmpr * 8.0 / 0xffffU)... but nah. */

    FLAC__stream_encoder_set_compression_level(se, 8); /* doesn't go to 11... */

    return 0;

 invalid:
    errno = EINVAL;
    return -1;
}


int flen_set_metadata(rnc_encoder_t *enc, rnc_meta_t *meta)
{
#define TAG(_tag, ...) do {                                                    \
        const char *_v;                                                        \
                                                                               \
        n = snprintf(p, l, __VA_ARGS__);                                       \
        if (n < 0)                                                             \
            goto invalid;                                                      \
        if (n + 1 >= l)                                                        \
            goto overflow;                                                     \
        _v = p;                                                                \
        p += n + 1;                                                            \
        l -= n + 1;                                                            \
                                                                               \
        FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, \
                                                                       _tag,   \
                                                                       _v);    \
        FLAC__metadata_object_vorbiscomment_append_comment(data[0], entry, 0); \
    } while (0)

    flen_t *fe;
    FLAC__StreamEncoder *se;
    FLAC__StreamMetadata_VorbisComment_Entry entry;
    FLAC__StreamMetadata *data[3];
    char tags[16 * 1024], *p;
    int  l, n;

    mrp_debug("setting FLAC metadata");

    if (enc == NULL || (fe = enc->data) == NULL || (se = fe->enc) == NULL)
        goto invalid;

    if (fe->busy)
        goto busy;

    data[0] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);
    data[1] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PADDING);
    data[1]->length = 16;

    entry.entry  = (unsigned char *)"RipNCode";
    entry.length = 8;
    FLAC__metadata_object_vorbiscomment_set_vendor_string(data[0], entry, true);

    l = sizeof(tags) - 1;
    p = tags;

    if (meta->title) {
        TAG("TITLE", "%s", meta->title);
    }

    if (meta->album) {
        TAG("ALBUM", "%s", meta->album);
    }

    if (meta->track > 0) {
        TAG("TRACKNUMBER", "%d", meta->track);
    }

    if (meta->artist) {
        TAG("ARTIST", "%s", meta->artist);
    }

    if (meta->genre) {
        TAG("GENRE", "%s", meta->genre);
    }

    if (meta->date.tm_year) {
        TAG("DATE", "%d", meta->date.tm_year);
    }

    if (meta->isrc) {
        TAG("ISRC", "%s", meta->isrc);
    }

    if (meta->performer) {
        TAG("PERFORMER", "%s", meta->performer);
    }

    if (meta->copyright) {
        TAG("COPYRIGHT", "%s", meta->copyright);
    }

    if (meta->license) {
        TAG("LICENSE", "%s", meta->license);
    }

    if (meta->organization) {
        TAG("ORGANIZATION", "%s", meta->organization);
    }

    TAG("REPLAYGAIN_TRACK_GAIN", "#TRK# dB");    /* dummy placeholder */
    TAG("REPLAYGAIN_TRACK_PEAK", "#PK#");        /* dummy placeholder */
    TAG("REPLAYGAIN_ALBUM_GAIN", "#ALB# dB");    /* dummy placeholder */

    if (!fe->gain_offs)
        fe->gain_offs = rnc_buf_wseek(fe->buf, 0, SEEK_CUR);

    if (FLAC__stream_encoder_set_metadata(se, data, 2))
        return 0;
    else
        return -1;

 busy:
    errno = EBUSY;
    return -1;
 overflow:
    errno = ENOBUFS;
    return -1;
 invalid:
    errno = EINVAL;
    return -1;
#undef TAG
}


int flen_set_gain(rnc_encoder_t *enc, double gain, double peak, double album)
{
    flen_t *fe;
    FLAC__StreamEncoder *se;

    mrp_debug("updating replaygain in FLAC metadata");

    if (enc == NULL || (fe = enc->data) == NULL || (se = fe->enc) == NULL)
        goto invalid;

    fe->track_gain = gain;
    fe->track_peak = peak;
    fe->album_gain = album;

    mrp_debug("replaygain offset: %u", fe->gain_offs);

    return 0;

 invalid:
    errno = EINVAL;
    return -1;
}

int flen_write(rnc_encoder_t *enc, void *buf, size_t size)
{
    flen_t *fe;
    FLAC__StreamEncoder *se;
    unsigned nsample, i;
    int32_t l[size], r[size];
    const FLAC__int32 const *samples[2];
    int16_t *p;

    if (enc == NULL || (fe = enc->data) == NULL || (se = fe->enc) == NULL)
        goto invalid;

    nsample = size / (fe->chnl * (fe->bits / 8));

    mrp_debug("writing %zu bytes (%u samples) of FLAC data", size, nsample);

    p = (int16_t *)buf;
    i = 0;
    if (fe->swap) {
        while (i < nsample) {
            l[i] = bswap_16(*p++);
            r[i] = bswap_16(*p++);
            i++;
        }
    }
    else {
        while (i < nsample) {
            l[i] = *p++;
            r[i] = *p++;
            i++;
        }
    }

    samples[0] = l;
    samples[1] = r;

    if (!FLAC__stream_encoder_process(se, samples, nsample))
        goto ioerror;

    return 0;

 ioerror:
    errno = EIO;
    return -1;
 invalid:
    errno = EINVAL;
    return -1;
}


int flen_finish(rnc_encoder_t *enc)
{
    flen_t *fe;
    FLAC__StreamEncoder *se;

    mrp_debug("finalizing FLAC encoding");

    if (enc == NULL || (fe = enc->data) == NULL || (se = fe->enc) == NULL)
        goto invalid;

    if (!FLAC__stream_encoder_finish(se))
        goto ioerror;

    return 0;

 ioerror:
    errno = EIO;
    return -1;
 invalid:
    errno = EINVAL;
    return -1;
}


int flen_set_data_cb(rnc_encoder_t *enc, rnc_enc_data_cb_t cb)
{
    flen_t *fe;

    mrp_debug("FLAC data available callback set to %p", cb);

    if (enc == NULL || (fe = enc->data) == NULL)
        goto invalid;

    fe->data_cb = cb;
    return 0;

 invalid:
    errno = EINVAL;
    return -1;
}


int flen_read(rnc_encoder_t *enc, void *buf, size_t size)
{
    flen_t *fe;

    mrp_debug("reading %zu bytes of FLAC data", size);

    if (enc == NULL || (fe = enc->data) == NULL)
        goto invalid;

    return rnc_buf_read(fe->buf, buf, size);

 invalid:
    errno = EINVAL;
    return -1;
}


static FLAC__StreamEncoderWriteStatus
__flen_write(const FLAC__StreamEncoder *se, const FLAC__byte buffer[],
             size_t bytes, unsigned samples, unsigned current_frame,
             void *client_data)
{
    rnc_encoder_t *enc;
    flen_t *fe;

    MRP_UNUSED(se);
    MRP_UNUSED(samples);
    MRP_UNUSED(current_frame);

    mrp_debug("writing %zu bytes of FLAC %sdata", bytes,
              samples > 0 ? "sample " : "meta");

    if ((enc = client_data) == NULL || (fe = enc->data) == NULL)
        goto invalid;

    if (rnc_buf_write(fe->buf, buffer, bytes) < 0)
        goto nomem;

    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;

 invalid:
    errno = EINVAL;
 nomem:
    return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
}


static FLAC__StreamEncoderSeekStatus
__flen_seek(const FLAC__StreamEncoder *se, FLAC__uint64 abs_offset,
            void *client_data)
{
    rnc_encoder_t *enc;
    flen_t *fe;

    MRP_UNUSED(se);

    mrp_debug("seeking to offset %lu within FLAC stream", abs_offset);

    if ((enc = client_data) == NULL || (fe = enc->data) == NULL)
        goto invalid;

    if (rnc_buf_wseek(fe->buf, abs_offset, SEEK_SET) < 0)
        goto invalid;

    return FLAC__STREAM_ENCODER_SEEK_STATUS_OK;

 invalid:
    errno = EINVAL;
    return FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;
}


static FLAC__StreamEncoderTellStatus
    __flen_tell(const FLAC__StreamEncoder *se, FLAC__uint64 *abs_offset,
                void *client_data)
{
    rnc_encoder_t *enc;
    flen_t *fe;

    MRP_UNUSED(se);

    mrp_debug("responding to FLAC offset query");

    if ((enc = client_data) == NULL || (fe = enc->data) == NULL)
        goto invalid;

    *abs_offset = rnc_buf_wseek(fe->buf, 0, SEEK_CUR);
    return FLAC__STREAM_ENCODER_TELL_STATUS_OK;

 invalid:
    errno = EINVAL;
    return FLAC__STREAM_ENCODER_TELL_STATUS_ERROR;
}


static void __flen_meta(const FLAC__StreamEncoder *se,
                        const FLAC__StreamMetadata *meta, void *client_data)
{
    MRP_UNUSED(se);
    MRP_UNUSED(meta);
    MRP_UNUSED(client_data);

    mrp_debug("%s() called back...", __FUNCTION__);

    return;
}


static const char *flac_types[] = { "flac", NULL };

RNC_ENCODER_REGISTER(flac, {
        .types        = flac_types,
        .create       = flen_create,
        .open         = flen_open,
        .close        = flen_close,
        .set_quality  = flen_set_quality,
        .set_metadata = flen_set_metadata,
        .set_gain     = flen_set_gain,
        .write        = flen_write,
        .finish       = flen_finish,
        .set_data_cb  = flen_set_data_cb,
        .read         = flen_read,
    });

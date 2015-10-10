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

#include <murphy/common/debug.h>
#include <ripncode/ripncode.h>


#define BUFFER_CHUNK (64 * 1024)

typedef struct {
    FLAC__StreamEncoder *enc;
    rnc_enc_data_cb_t    data_cb;
    int                  chnl;
    int                  bits;
    rnc_buf_t           *buf;
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
    FLAC__StreamEncoder *se;

    mrp_debug("closing FLAC encoder %p", enc);

    if (enc == NULL || (se = enc->data) == NULL)
        return;

    FLAC__stream_encoder_delete(se);
    enc->data = NULL;
}


int flen_set_quality(rnc_encoder_t *enc, uint16_t qlty, uint16_t cmpr)
{
    FLAC__StreamEncoder *se;

    MRP_UNUSED(qlty);
    MRP_UNUSED(cmpr);

    mrp_debug("setting FLAC quality/compression to %u/%u", qlty, cmpr);

    if (enc == NULL || (se = enc->data) == NULL)
        goto invalid;

    /* XXX maybe we should cmpr = (int)ceil(cmpr * 8.0 / 0xffffU); */

    FLAC__stream_encoder_set_compression_level(se, 8); /* doesn't go to 11... */

    return 0;

 invalid:
    errno = EINVAL;
    return -1;
}


int flen_set_metadata(rnc_encoder_t *enc, rnc_meta_t *meta)
{
    flen_t *fe;
    FLAC__StreamEncoder *se;
    FLAC__StreamMetadata_VorbisComment_Entry comments[32], *c;
    FLAC__StreamMetadata *metadata[1], metablk;
#define TAG(_c, _tag, ...) do {                 \
        _c->entry = (FLAC__byte *)p;            \
        n = snprintf(p, l, _tag"="__VA_ARGS__); \
        if (n < 0)                              \
            goto invalid;                       \
        if (n + 1 >= l)                         \
            goto overflow;                      \
        p += n + 1;                             \
        l -= n + 1;                             \
        _c->length = strlen((char *)_c->entry); \
    } while (0)
    char tags[16 * 1024], *p;
    int  l, n;

    mrp_debug("setting FLAC metadata");

    if (enc == NULL || (fe = enc->data) == NULL || (se = fe->enc) == NULL)
        goto invalid;

    if (fe->busy)
        goto busy;

    l = sizeof(tags) - 1;
    p = tags;
    c = comments;

    if (meta->title) {
        TAG(c, "TITLE", "%s", meta->title);
        c++;
    }

    if (meta->album) {
        TAG(c, "ALBUM", "%s", meta->album);
        c++;
    }

    if (meta->track > 0) {
        TAG(c, "TRACKNUMBER", "%d", meta->track);
        c++;
    }

    if (meta->artist) {
        TAG(c, "ARTIST", "%s", meta->artist);
        c++;
    }

    if (meta->genre) {
        TAG(c, "GENRE", "%s", meta->genre);
        c++;
    }

    /* XXX TODO: add the rest of the tags... */

    metablk.type = FLAC__METADATA_TYPE_VORBIS_COMMENT;
    metablk.is_last = true;
    metablk.data.vorbis_comment.vendor_string.entry  = (FLAC__byte *)"RipNCode";
    metablk.data.vorbis_comment.vendor_string.length = 8;
    metablk.data.vorbis_comment.num_comments = c - comments;
    metablk.data.vorbis_comment.comments     = comments;

    metadata[0] = &metablk;

    if (FLAC__stream_encoder_set_metadata(se, metadata, 1))
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

    mrp_debug("deinitializing FLAC encoder %p", enc);

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
        .write        = flen_write,
        .finish       = flen_finish,
        .set_data_cb  = flen_set_data_cb,
        .read         = flen_read,
    });

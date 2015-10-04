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
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>

#include <cdio/cdio.h>
#include <cdio/paranoia.h>

#include <murphy/common/debug.h>
#include <ripncode/ripncode.h>


typedef struct {
    int      id;
    int      idx;
    uint32_t fblk;
    uint32_t lblk;
} cdpa_track_t;


typedef struct {
    CdIo_t           *cdio;
    cdrom_drive_t    *cdda;
    cdrom_paranoia_t *cdpa;
    cdpa_track_t     *tracks;
    int               ntrack;
    int               ctrack;
    char             *errmsg;
    int               error;
} cdpa_t;


static int32_t seek_track(cdpa_t *cdpa, int idx, uint32_t blk);


static bool cdpa_probe(rnc_dev_api_t *api, const char *device)
{
    int fd, is_cd;

    MRP_UNUSED(api);

    mrp_debug("probing device '%s' with cdparanoia", device);

    if (!strcmp(device, "/dev/cdrom"))
        return true;

    if (!strncmp(device, "/dev/cd", 7))
        return true;

    if (!strncmp(device, "/dev/sr", 7))
        return true;

    fd = open(device, O_RDONLY);

    if (fd < 0)
        return -1;

    is_cd = (ioctl(fd, CDROM_DRIVE_STATUS) != -1);

    close(fd);

    return is_cd;
}


static int cdpa_open(rnc_dev_t *dev, const char *device)
{
    cdpa_t *cdpa;
    char   *msg;

    mrp_debug("opening device '%s' with cdparanoia", device);

    msg  = NULL;
    cdpa = dev->data = mrp_allocz(sizeof(*cdpa));

    if (cdpa == NULL)
        goto fail;

    cdpa->cdio = cdio_open(device, DRIVER_LINUX);

    if (cdpa->cdio == NULL) {
        msg = "failed open device";
        goto fail;
    }

    cdpa->cdda = cdio_cddap_identify_cdio(cdpa->cdio, CDDA_MESSAGE_LOGIT, &msg);

    if (cdpa->cdda == NULL)
        goto fail;

    cdio_cddap_free_messages(msg);
    msg = NULL;

    if (cdio_cddap_open(cdpa->cdda) < 0) {
        msg = "failed to CDDAP-open device";
        goto fail;
    }

    cdpa->cdpa = cdio_paranoia_init(cdpa->cdda);

    if (cdpa->cdpa == NULL) {
        msg = "failed to initialize cd-paranoia";
        goto fail;
    }

    cdpa->ctrack = -1;

    return 0;

 fail:
    if (cdpa != NULL && msg != NULL)
        cdpa->errmsg = mrp_strdup(msg);

    return -1;
}


static void cdpa_close(rnc_dev_t *dev)
{
    cdpa_t *cdpa = dev->data;

    mrp_debug("closing device");

    if (cdpa == NULL)
        return;

    /* Hmm... what is the opposite of cdio_open ? */

    if (cdpa->cdpa)
        cdio_paranoia_free(cdpa->cdpa);

    if (cdpa->cdio)
        cdio_cddap_close(cdpa->cdda);

    mrp_free(cdpa->errmsg);
    mrp_free(cdpa);
}


static int cdpa_get_tracks(rnc_dev_t *dev, rnc_track_t *buf, size_t size)
{
    cdpa_t       *cdpa = dev->data;
    cdpa_track_t *tracks, *trk;
    int           ntrack, base, naudio;
    rnc_track_t  *t;
    int           id, i;

    mrp_debug("getting tracks");

    if (cdpa->ntrack == 0) {
        ntrack = cdio_get_num_tracks(cdpa->cdio);
        base   = cdio_get_first_track_num(cdpa->cdio);

        if (ntrack == 0)
            return 0;

        tracks = mrp_allocz_array(typeof(*tracks), ntrack);

        if (tracks == NULL)
            return -1;

        naudio = 0;
        trk    = tracks;
        for (i = 0; i < ntrack; i++) {
            id = base + i;

            if (cdio_get_track_format(cdpa->cdio, id) != TRACK_FORMAT_AUDIO)
                continue;
            trk->id  = id;
            trk->idx = i;
            trk->fblk = cdio_get_track_lsn(cdpa->cdio, id);
            trk->lblk = cdio_get_track_last_lsn(cdpa->cdio, id);

            naudio++;
            trk++;
        }

        cdpa->tracks = tracks;
        cdpa->ntrack = naudio;
    }
    else
        ntrack = cdpa->ntrack;

    if ((int)size > cdpa->ntrack)
        size = cdpa->ntrack;

    for (i = 0, t = buf, trk = cdpa->tracks; i < (int)size; i++, t++, trk++) {
        t->idx    = trk - cdpa->tracks;
        t->id     = i;
        t->fblk   = trk->fblk;
        t->nblk   = trk->lblk - trk->fblk + 1;
        t->length = 1.0 * t->nblk / 75.0;
    }

    if (cdpa->ctrack < 0) {
        cdio_paranoia_modeset(cdpa->cdpa, PARANOIA_MODE_FULL);
        cdpa->ctrack = 0;
        seek_track(cdpa, 0, 0);
    }

    return ntrack;
}


static int cdpa_get_formats(rnc_dev_t *dev, uint32_t *buf, size_t size)
{
    cdpa_t *cdpa = dev->data;
    int cmap = RNC_CHANNELMAP_LEFTRIGHT;
    int cmpr = RNC_ENCODING_PCM;
    int chnl = 2;
    int rate = RNC_SAMPLERATE_44100;
    int bits = 16;
    int frmt = RNC_SAMPLE_SIGNED;
    int endn = data_bigendianp(cdpa->cdda) ? RNC_ENDIAN_BIG : RNC_ENDIAN_LITTLE;

    mrp_debug("getting supported device format(s)");

    if (size > 0)
        *buf = RNC_FORMAT_ID(cmap, cmpr, chnl, rate, bits, frmt, endn);

    return 1;
}


static int cdpa_set_format(rnc_dev_t *dev, uint32_t f)
{
    cdpa_t *cdpa = dev->data;
    int cmap = RNC_CHANNELMAP_LEFTRIGHT;
    int cmpr = RNC_ENCODING_PCM;
    int chnl = 2;
    int rate = RNC_SAMPLERATE_44100;
    int bits = 16;
    int frmt = RNC_SAMPLE_SIGNED;
    int endn = data_bigendianp(cdpa->cdda) ? RNC_ENDIAN_BIG : RNC_ENDIAN_LITTLE;

    mrp_debug("setting active device format");

    if (f != RNC_FORMAT_ID(cmap, cmpr, chnl, rate, bits, frmt, endn))
        return -1;
    else
        return 0;
}


static uint32_t cdpa_get_format(rnc_dev_t *dev)
{
    cdpa_t *cdpa = dev->data;
    int cmap = RNC_CHANNELMAP_LEFTRIGHT;
    int cmpr = RNC_ENCODING_PCM;
    int chnl = 2;
    int rate = RNC_SAMPLERATE_44100;
    int bits = 16;
    int frmt = RNC_SAMPLE_SIGNED;
    int endn = data_bigendianp(cdpa->cdda) ? RNC_ENDIAN_BIG : RNC_ENDIAN_LITTLE;

    mrp_debug("getting active device format");

    return RNC_FORMAT_ID(cmap, cmpr, chnl, rate, bits, frmt, endn);
}


static int cdpa_get_blocksize(rnc_dev_t *dev)
{
    MRP_UNUSED(dev);

    mrp_debug("getting device block size");

    return CDIO_CD_FRAMESIZE_RAW;
}


static int32_t seek_track(cdpa_t *cdpa, int idx, uint32_t blk)
{
    int32_t offs;

    mrp_debug("seeking to track #%d, block %u", idx, blk);

    if (idx < 0 || idx >= cdpa->ntrack)
        goto invalid;

    if (cdpa->tracks[idx].fblk + blk > cdpa->tracks[idx].lblk)
        goto invalid;

    offs = (cdpa->tracks[idx].fblk + blk) * CDIO_CD_FRAMESIZE_RAW;
    return (int32_t)cdio_paranoia_seek(cdpa->cdpa, offs, SEEK_SET);

 invalid:
    errno = EINVAL;
    return -1;
}


static int32_t cdpa_seek(rnc_dev_t *dev, rnc_track_t *trk, uint32_t blk)
{
    cdpa_t  *cdpa = dev->data;

    return seek_track(cdpa, trk->idx, blk);
}


static void read_status(long int i, paranoia_cb_mode_t mode)
{
#if 1
    MRP_UNUSED(i);
    MRP_UNUSED(mode);
#else
    mrp_debug("roffset: %ld, mode: 0x%x (%s)", i, mode,
              paranoia_cb_mode2str[mode]);
#endif
}


static int cdpa_read(rnc_dev_t *dev, void *buf, size_t size)
{
    cdpa_t *cdpa = dev->data;
    char   *s, *p;
    size_t  n;

    mrp_debug("reading %zu bytes", size);

    if ((size % CDIO_CD_FRAMESIZE_RAW) != 0)
        goto invalid;

    p = buf;
    n = size;
    while (n > 0) {
        s = (char *)cdio_paranoia_read(cdpa->cdpa, read_status);

        if (s == NULL)
            goto ioerror;

        memcpy(p, s, CDIO_CD_FRAMESIZE_RAW);
        p +=  CDIO_CD_FRAMESIZE_RAW;
        n -=  CDIO_CD_FRAMESIZE_RAW;
    }

    return size;

 invalid:
    errno = EINVAL;
    return -1;

 ioerror:
    errno = EIO;
    return -1;
}


static int cdpa_error(rnc_dev_t *dev, const char **error)
{
    cdpa_t *cdpa = dev->data;

    if (error != NULL && cdpa->errmsg != NULL)
        *error = cdpa->errmsg;

    return cdpa->error;
}


RNC_DEVICE_REGISTER(cdio, {
        .name          = "cdio",
        .probe         = cdpa_probe,
        .open          = cdpa_open,
        .close         = cdpa_close,
        .get_tracks    = cdpa_get_tracks,
        .get_formats   = cdpa_get_formats,
        .set_format    = cdpa_set_format,
        .get_format    = cdpa_get_format,
        .get_blocksize = cdpa_get_blocksize,
        .seek          = cdpa_seek,
        .read          = cdpa_read,
        .error         = cdpa_error,
});


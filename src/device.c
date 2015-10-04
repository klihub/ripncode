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

static MRP_LIST_HOOK(devices);


int rnc_device_init(rnc_t *rnc)
{
    mrp_list_init(&rnc->devices);
    mrp_list_move(&rnc->devices, &devices);

    return 0;
}


int rnc_device_register(rnc_t *rnc, const char *name, rnc_dev_api_t *api)
{
    mrp_list_init(&api->hook);

    if (api->name == NULL)
        api->name = name;

    if (rnc == NULL)
        mrp_list_append(&devices, &api->hook);
    else
        mrp_list_append(&rnc->devices, &api->hook);

    return 0;
}


rnc_dev_t *rnc_device_open(rnc_t *rnc, const char *device)
{
    rnc_dev_api_t   *api;
    rnc_dev_t       *dev;
    mrp_list_hook_t *p, *n;

    dev = NULL;

    mrp_list_foreach(&rnc->devices, p, n) {
        api = mrp_list_entry(p, typeof(*api), hook);

        if (!api->probe(api, device))
            continue;

        dev = mrp_allocz(sizeof(*dev));

        if (dev == NULL)
            goto fail;

        dev->rnc = rnc;
        dev->api = api;
        dev->dev = mrp_strdup(device);

        if (dev->dev == NULL)
            goto fail;

        if (api->open(dev, device) < 0)
            goto fail;

        return dev;
    }

    errno = ENOTSUP;
    return NULL;

 fail:
    if (dev) {
        if (dev->api)
            dev->api->close(dev);
        mrp_free(dev->dev);
        mrp_free(dev);
    }
    return NULL;
}


void rnc_device_close(rnc_dev_t *dev)
{
    if (dev == NULL)
        return;

    if (dev->api)
        dev->api->close(dev);

    mrp_free(dev->dev);
    mrp_free(dev);
}


int rnc_device_get_tracks(rnc_dev_t *dev, rnc_track_t *buf, size_t size)
{
    return dev->api->get_tracks(dev, buf, size);
}


int rnc_device_get_formats(rnc_dev_t *dev, uint32_t *buf, size_t size)
{
    return dev->api->get_formats(dev, buf, size);
}


int rnc_device_set_format(rnc_dev_t *dev, uint32_t id)
{
    return dev->api->set_format(dev, id);
}


uint32_t rnc_device_get_format(rnc_dev_t *dev)
{
    return dev->api->get_format(dev);
}


int rnc_device_get_blocksize(rnc_dev_t *dev)
{
    return dev->api->get_blocksize(dev);
}


int32_t rnc_device_seek(rnc_dev_t *dev, rnc_track_t *trk, uint32_t blk)
{
    return dev->api->seek(dev, trk, blk);
}


int rnc_device_read(rnc_dev_t *dev, void *buf, size_t size)
{
    return dev->api->read(dev, buf, size);
}


int rnc_device_error(rnc_dev_t *dev, const char **errstr)
{
    return dev->api->error(dev, errstr);
}

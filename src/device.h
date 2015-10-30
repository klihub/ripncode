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

#ifndef __RIPNCODE_DEVICE_H__
#define __RIPNCODE_DEVICE_H__

#include <ripncode/ripncode.h>

MRP_CDECL_BEGIN

/**
 * @brief RNC device backend API abstraction.
 *
 * The device backend API abstraction contains functions for probing and
 * opening a particular device (of supported type), querying and selecting
 * supported audio formats, querying tracks, and for reading audio data.
 */
struct rnc_dev_api_s {
    mrp_list_hook_t  hook;               /* to list of known backends */
    const char      *name;               /* backend name */
    /* probe if the given device is supported by us */
    bool (*probe)(rnc_dev_api_t *api, const char *device);
    /* open the given device */
    int (*open)(rnc_dev_t *d, const char *device);
    /* close the device */
    void (*close)(rnc_dev_t *d);
    /* set speed */
    int (*set_speed)(rnc_dev_t *d, int speed);
    /* get track info */
    int (*get_tracks)(rnc_dev_t *d, rnc_track_t *buf, size_t size);
    /* get supported formats */
    int (*get_formats)(rnc_dev_t *d, uint32_t *buf, size_t size);
    /* request the given format */
    int (*set_format)(rnc_dev_t *d, uint32_t f);
    /* get current format */
    uint32_t (*get_format)(rnc_dev_t *d);
    /* get minimum block size (== unit of data) readable */
    int (*get_blocksize)(rnc_dev_t *d);
    /* seek to the given logical block within the given track */
    int32_t (*seek)(rnc_dev_t *d, rnc_track_t *trk, uint32_t blk);
    /* read data */
    int (*read)(rnc_dev_t *d, void *buf, size_t size);
    /* get last error code and string */
    int (*error)(rnc_dev_t *d, const char **errstr);
};


/**
 * @brief An RNC device.
 *
 * This structure represents an RNC-supported device that can be used
 * to obtain audio data.
 **/
struct rnc_dev_s {
    rnc_t         *rnc;                  /* RNC back pointer */
    char          *dev;                  /* device id (eg. /dev entry) */
    rnc_dev_api_t *api;                  /* device API */
    void          *data;                 /* opaque device data */
};


/**
 * @brief Initialize devices known to RNC.
 */
int rnc_device_init(rnc_t *rnc);


/**
 * @brief Open the given RNC-supported device.
 *
 * Find a suitable backend for handling the given device and open it.
 *
 * @param [in] device  device to open
 *
 * @return Returns the opened device on success, NULL on failure.
 */
rnc_dev_t *rnc_device_open(rnc_t *rnc, const char *device);


/**
 * @brief Close an open device.
 *
 * Close a device perviously opened by rnc_device_open.
 *
 * @param [in] dev  device to close
 */
void rnc_device_close(rnc_dev_t *dev);


/**
 * @brief Set device speed.
 *
 * @param [in] dev    device to set speed for
 * @param [in] speed  device speed to set
 *
 * @return Returns 0 upon success, -1 otherwise.
 */
int rnc_device_set_speed(rnc_dev_t *dev, int speed);

/**
 * @brief Get tracklist of the device.
 *
 * Get the list of audio tracks available on the given device.
 *
 *
 * @param [in]  dev   device to query for tracks
 * @param [out] buf   buffer to write track info into
 * @param [in]  size  number of tracks buf has space for
 *
 * @return Returns the number of audio tracks available on device. Note
 *         that this may be larger than size, in which case buf is filled
 *         with only the first size entries.
 */
int rnc_device_get_tracks(rnc_dev_t *dev, rnc_track_t *buf, size_t size);

/**
 * @brief Get the formats supported by a device.
 *
 * Get the audio formats supported by the given device.
 *
 * @param [in]  dev   device to get supported formats for.
 * @param [our] buf   buffer to write supported formats into
 * @param [in]  size  number of format entries buf has space for
 *
 * @return Returns the number of formats spported by the device.
 */
int rnc_device_get_formats(rnc_dev_t *dev, uint32_t *buf, size_t size);

/**
 * @brief Set the desired audio format for device.
 *
 * Set the format audio os to be read from the given device.
 *
 * @param [in] dev  device to reconfigure
 * @param [in] id   format identifier to use
 *
 * @return Returns 0 on sucess, -1 upon error.
 */
int rnc_device_set_format(rnc_dev_t *dev, uint32_t id);

/**
 * @brief Get the current audio format of the device.
 *
 * Get the currently configured format identifier of the given device.
 *
 * @param [in] dev  device to get current format for
 *
 * @return Returns the currently configured format identifier for the
 *         given device.
 */
uint32_t rnc_device_get_format(rnc_dev_t *dev);

/**
 * @brief Get the blocksize of a device.
 *
 * Get the block size of the given device. All seeks and reads for the
 * device must use multiples of the blocksize.
 *
 * @param [in] dev  device to get blocksize for
 *
 * @return Returns the blocksize of the device.
 */
int rnc_device_get_blocksize(rnc_dev_t *dev);

/**
 * @brief Set position for the next read.
 *
 * Set the track and block for which the next read is going to return
 * audio data from the given device.
 *
 * @param [in] dev  device to reposition
 * @param [in] trk  track to position device to
 * @param [in] blk  relative block number within track
 *
 * @return Returns 0 on success, -1 on error.
 */
int32_t rnc_device_seek(rnc_dev_t *dev, rnc_track_t *trk, uint32_t blk);

/**
 * @brief Read audio data.
 *
 * Read audio data from the given device.
 *
 * @param [in]  dev   device to read audio from
 * @param [out] buf   buffer to put audio data to
 * @param [in]  size  size of buf / amount of audio to read
 *
 * @return Returns 0 upon success, -1 upon error.
 */
int rnc_device_read(rnc_dev_t *dev, void *buf, size_t size);

/**
 * @brief Return the last error for a device.
 *
 * Read the last error code and optionally string from a device.
 *
 * @param [in]  dev     device to trad error for
 * @param [out] errstr  buffer to store error string into, or NULL
 *
 * @return Returns the last error code for the device.
 */
int rnc_device_error(rnc_dev_t *dev, const char **errstr);

/**
 * @brief Register an RNC-supported device backend.
 *
 * Register the given device backend with the given name for
 * use with RNC. Once registered, devices of compatible type
 * can be used to obtain audio data.
 *
 * @param [in] rnc   RNC instance to register device backend with
 * @param [in] name  name to register device with
 * @param [in] api   backend API to use for devices of this type
 *
 * @return Returns 0 upon success, -1 on error.
 */
int rnc_device_register(rnc_t *rnc, const char *name, rnc_dev_api_t *api);


/**
 * @brief Automatically register a device backend.
 *
 * Use this macro toautomatically registered a device backend with RNC
 * on startup.
 *
 * @param [in] _prfx  device identification prefix, eg. 'cdrom:'
 * @param [in] _api   backend API function table to use
 */
#define RNC_DEVICE_REGISTER(_prfx, ...)                         \
    static void MRP_INIT __rnc_device_register_##_prfx(void) {  \
        static rnc_dev_api_t api = __VA_ARGS__;                 \
                                                                \
        rnc_device_register(NULL, #_prfx, &api);                \
    }

MRP_CDECL_END

#endif /* __RIPNCODE_DEVICE_H__ */

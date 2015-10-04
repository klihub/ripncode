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

#ifndef __RIPNCODE_ENCODER_H__
#define __RIPNCODE_ENCODER_H__

#include <ripncode/ripncode.h>

MRP_CDECL_BEGIN

/**
 * @brief RNC encoder backend API abstraction.
 *
 */

typedef void (*rnc_enc_data_cb_t)(rnc_encoder_t *enc, size_t size);

struct rnc_enc_api_s {
    mrp_list_hook_t   hook;              /* to list of known backends */
    const char       *name;              /* backend name */
    const char      **types;             /* supported output types */
    int (*create)(rnc_encoder_t *enc, uint32_t format);
    /* open and initialize a new instance */
    int (*open)(rnc_encoder_t *enc);
    /* close the encoder instance */
    void (*close)(rnc_encoder_t *enc);
    /* set compression/quality, if supported */
    int (*set_quality)(rnc_encoder_t *enc, uint16_t qlty, uint16_t cmpr);
    /* add/set metadata */
    int (*set_metadata)(rnc_encoder_t *enc, rnc_meta_t *meta);
    /* add new audio data to encode */
    int (*write)(rnc_encoder_t *enc, void *buf, size_t size);
    /* finish the encoding process */
    int (*finish)(rnc_encoder_t *enc);
    /* set data available callback */
    int (*set_data_cb)(rnc_encoder_t *enc, rnc_enc_data_cb_t cb);
    /* retrieve encoded data */
    int (*read)(rnc_encoder_t *enc, void *buf, size_t size);
};


/**
 * @brief An RNC encoder.
 */
struct rnc_encoder_s {
    rnc_t         *rnc;
    rnc_enc_api_t *api;
    void          *data;
    void          *user_data;
};


/**
 * @brief Initialize encoders known to RNC.
 *
 * Initialize the given RNC instance with the set of enoders
 * known to RNC.
 *
 * @param [in] rnc  RNC instance to initialize
 *
 * @return Returns 0 upon success, -1 upon failure.
 */
int rnc_encoder_init(rnc_t *rnc);


/**
 * @bief Create and initialize an encoder instance.
 *
 * Create a new encoder and initialize it for encoding to the
 * the given format.
 *
 * @param [in] rnc     RNC instance
 * @param [in] format  format to create an encoder for
 *
 * @return Returns the newly created encoder upon success, NULL upon failure.
 */
rnc_encoder_t *rnc_encoder_create(rnc_t *rnc, uint32_t format);


int rnc_encoder_set_quality(rnc_encoder_t *enc, uint16_t qlty, uint16_t cmpr);

/**
 * @brief Open the given encoder for encoding samples.
 *
 * Prepare the given encoder for encoding audio samples according to the
 * format it was opened for.
 *
 * @param [in] enc  encoder to prepare for encoding
 *
 * @return Returns 0 upon success, -1 upon failure.
 */
int rnc_encoder_open(rnc_encoder_t *enc);


/**
 * @brief Write samples to the given encoder.
 *
 * Push samples through the given encoder. The encoder must have been opened
 * for encoding by calling @rnc_encoder_open. The given sample buffer must be
 * according to the format specified in @rnc_encoder_create.
 *
 * @param [in] enc   encoder to write samples tu
 * @param [in] buf   buffer of samples to write
 * @param [in] size  amount of data in the given sapmle buffer
 *
 * @return Returns 0 upon success, -1 otherwise.
 */
int rnc_encoder_write(rnc_encoder_t *enc, void *buf, size_t size);


int rnc_encoder_finish(rnc_encoder_t *enc);

int rnc_encoder_read(rnc_encoder_t *enc, void *buf, size_t size);

/**
 * @brief Register an encoder backend.
 *
 * Register the given encoder backend with the given name for
 * use with RNC. Once registered, streams of one of the types
 * supported by the backend can be created with RNC.
 *
 * @param [in] rnc   RNC instante to register backend with
 * @param [in] name  name for the encoder backend
 * @param [in] api   backend API to use
 *
 * @return Returns 0 upon success, -1 otherwise.
 */
int rnc_encoder_register(rnc_t *rnc, const char *name, rnc_enc_api_t *api);


/**
 * @brief Automatically register an encoder backend.
 *
 * Use this macro to automatically register an encoder backend with RNC
 * on startup.
 *
 * @param [in] _prfx  backend indentification prefix, eg. flac.
 * @param [in] _api   backend API function table to use
 */
#define RNC_ENCODER_REGISTER(_prfx, ...)                        \
    static void MRP_INIT __rnc_encoder_register_##_prfx(void) { \
        static rnc_enc_api_t api = __VA_ARGS__;                 \
                                                                \
        rnc_encoder_register(NULL, #_prfx, &api);               \
    }

MRP_CDECL_END

#endif /* __RIPNCODE_ENCODER_H__ */
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

#ifndef __RIPNCODE_H__
#define __RIPNCODE_H__

#include <stdint.h>
#include <stdbool.h>

#include <murphy/common/macros.h>
#include <murphy/common/log.h>
#include <murphy/common/debug.h>
#include <murphy/common/mm.h>
#include <murphy/common/list.h>

typedef struct rnc_dev_api_s  rnc_dev_api_t;
typedef struct rnc_dev_s      rnc_dev_t;
typedef struct rnc_track_s    rnc_track_t;
typedef struct rnc_meta_s     rnc_meta_t;
typedef struct rnc_metadb_s   rnc_metadb_t;
typedef struct rnc_meta_api_s rnc_meta_api_t;
typedef struct rnc_buf_s      rnc_buf_t;
typedef struct rnc_enc_api_s  rnc_enc_api_t;
typedef struct rnc_encoder_s  rnc_encoder_t;
typedef struct rnc_s          rnc_t;

struct rnc_s {
    char            **formats;           /* registered compression schemes */
    int               nformat;           /* number of registered schemes */
    mrp_list_hook_t   devices;           /* known devices */
    mrp_list_hook_t   encoders;          /* known encoders */
    mrp_list_hook_t   metadbs;           /* known metadata DBs */
    rnc_dev_t        *dev;               /* device to read audio from */
    rnc_track_t      *tracks;            /* tracks on device */
    int               ntrack;            /* number of tracks */
    rnc_encoder_t    *enc;               /* active encoder */
    rnc_metadb_t     *db;                /* metadata DB */

    /* command line arguments */
    const char *argv0;                   /* our executable */
    const char *device;                  /* device to use */
    const char *rip;                     /* tracks to rip */
    const char *metadata;                /* metadata file to use */
    const char *output;                  /* output to write */
    const char *format;                  /* format to encode to */
    const char *pattern;
    int         log_mask;                /* what to log */
    const char *log_target;              /* where to log it to */
    int         dry_run;                 /* don't rip/encode */
};

#include <ripncode/format.h>
#include <ripncode/device.h>
#include <ripncode/track.h>
#include <ripncode/metadata.h>
#include <ripncode/buffer.h>
#include <ripncode/encoder.h>

#endif /* __RIPNCODE_H__ */

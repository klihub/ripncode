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

#ifndef __RIPNCODE_METADATA_H__
#define __RIPNCODE_METADATA_H__

#include <time.h>
#include <ripncode/ripncode.h>

MRP_CDECL_BEGIN

/**
 * @brief Metadata about a single track.
 */
struct rnc_meta_s {
    int         track;                   /* track number */
    const char *title;                   /* track title */
    const char *album;                   /* album name */
    const char *artist;                  /* album artist */
    const char *genre;                   /* album genre */
    struct tm   date;                    /* recording/release date */
    const char *isrc;                    /* ISRC code */
    const char *performer;               /* artist(s) performing */
    const char *copyright;               /* copyright attribution */
    const char *license;                 /* license information */
    const char *organization;            /* producing organization */
    double      track_gain;              /* replaygain for track */
    double      track_peak;              /* peak for track */
    double      album_gain;              /* replaygain for album */
};


/**
 * @brief Metadata DB backend API abstraction.
 */
struct rnc_meta_api_s {
    mrp_list_hook_t  hook;               /* to list of known backends */
    const char      *type;               /* metadata DB type */
    /* create a new metadata DB instance of the given type */
    int (*create)(rnc_metadb_t *db);
    /* open and initialize the given metadata DB */
    int (*open)(rnc_metadb_t *db, const char **options);
    /* close the given instance */
    void (*close)(rnc_metadb_t *db);
    /* look up metadata for the given track */
    rnc_meta_t *(*lookup)(rnc_metadb_t *db, int track);
};


/**
 * @brief A metadata DB instance.
 */
struct rnc_metadb_s {
    rnc_t          *rnc;                 /* RNC backpointer */
    rnc_meta_api_t *api;                 /* backend API */
    void           *data;                /* backend data */
};


/**
 * @brief Initialize metadata backends known to RNC.
 */
int rnc_meta_init(rnc_t *rnc);


/**
 * @brief Register a metadata backend.
 */
int rnc_meta_register(rnc_t *rnc, rnc_meta_api_t *api);


/**
 * @brief Macro for automatically registering a metadata backend.
 */
#define RNC_META_REGISTER(_prfx, ...)                        \
    static void MRP_INIT __rnc_meta_register_##_prfx(void) { \
        static rnc_meta_api_t api = __VA_ARGS__;             \
                                                             \
        rnc_meta_register(NULL, &api);                       \
    }


/**
 * @brief Create a metadata DB instance of the given type.
 */
rnc_metadb_t *rnc_meta_create(rnc_t *rnc, const char *type);

/**
 * @brief Open the given metadata DB.
 */
int rnc_meta_open(rnc_metadb_t *db, const char **options);

/**
 * @brief Close the given metadata DB.
 */
void rnc_meta_close(rnc_metadb_t *db);

/**
 * @brief Look up metadata for the given track.
 */
rnc_meta_t *rnc_meta_lookup(rnc_metadb_t *db, int track);

/**
 * @brief Free the result of a metadata lookup.
 */
void rnc_meta_free(rnc_meta_t *m);

MRP_CDECL_END

#endif /* __RIPNCODE_METADATA_H__ */

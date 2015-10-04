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

#ifndef __RIPNCODE_FORMAT_H__
#define __RIPNCODE_FORMAT_H__

#include <ripncode/ripncode.h>


/**
 * @brief Audio format identification.
 *
 * We encode the details of a particular audio format into a single 32-bit
 * word, generally referred to as the audio format identifier. This format
 * identifier specifies the following properties of the audio stream:
 *
 *  1) channel map (not really supported ATM)
 *
 *    Channel mapping. Currenntly only the left-right stereo mapping is
 *    supported.
 *
 *  2) audio compression method (5 bits)
 *
 *     The compression method is either 0 (RNC_COMPRESS_PCM), for uncompressed
 *     audio, or the id of a registered compressed audio format, corresponding
 *     to a lossless compression scheme (such as FLAC), or a lossy compression
 *     scheme (such as OGG, or MP3).
 *
 *  3) audio channels (3 bits)
 *
 *    The number of audio channels in the stream. The supported channel
 *    numbers are 1 - 8.
 *
 *  4) sampling rate (2 bits)
 *
 *    The sampling rate of the audio stream. We support only four different
 *    sampling rates: 44.1 kHz, 48 kHz, 96 kHz, and 192 kHz.
 *
 *  5) bits per sample (2 bits)
 *
 *    The number of audio bits in a single sample. Only four different bps
 *    values are supported: 8, 16, 24, and 32
 *
 *  6) sample type (2 bits)
 *
 *     The sample type specifies how a single sample is represented in the
 *     audio stream, and it can be one of signed integer, unsigned integer,
 *     or floating point.
 *
 *  7) sample endianness (1 bit)
 *
 *    The sample endianness specifies whether integer samples are represented
 *    in LSB or MSB first order.
 *
 *
 *
 * Here is the layout of how these properties are curently encoded into
 * an audio format identifier:
 *
 * |         :  14-10   :    9-7   :  6-5 :  4-3 :   2-1  :    0   |
 * | chnlmap : compress : channels : rate : bits : sample : endian |
 * |         :     5    :     3    :   2  :   2  :    2   :    1   |
 *
 *
 * With this setup, excluding the channel map, we currently use
 *
 *         channel map: 0 (well, 0 as not really supported ATM)
 *         compression: 5 (0 == uncompressed, others registered id)
 *            channels: 3 (1 - 8)
 *         sample rate: 2 (44.1 kHz, 48 kHz, 96 kHz, 192 kHz)
 *     bits per sample: 2 (8, 16, 24, 32)
 *         sample type: 2 (unsigned, signed, floating)
 *   sample endianness: 1 (little, big)
 *   ----------------------------------------------------------------
 *                     15 bits
 *
 * leaving us with 17 bits to spare for future expansion and channel map
 * support if we ever need it.
 *
 */

/**
 * @brief Channel map.
 *
 * This is not much more than a placeholder/work-in-progress. Right now,
 * we don't really support anything else than basic 2 channel left-right
 * stereo audio.
 */
typedef enum {
    RNC_CHANNELMAP_LEFTRIGHT,            /* we only support this for now */
} rnc_chnlmap_t;


/**
 * @brief Audio compression scheme.
 *
 * The audio compression scheme indicates which compression algorithm the
 * audio stream is encoded with, if any. No compression (0) corresponds to
 * plain uncompressed PCM. Other ids correspond to dynamically registered
 * compression algorithms.
 */
typedef enum {
    RNC_COMPRESS_NONE = 0,
    RNC_ENCODING_PCM  = 0,               /* uncompressed PCM audio */
    RNC_COMPRESS_OTHER,                  /* compressed audio */
    /*                                    * dynamically registered range */
    RNC_COMPRESS_MAX  = 32               /* max encoder id */
} rnc_compress_t;


/**
 * @brief Sample type.
 *
 * Specifies how a single sample is represented in the audio stream,
 * an signed integer, an unsigned integer, or a floating-point number.
 */
typedef enum {
    RNC_SAMPLE_SIGNED,                   /* singed integer samples */
    RNC_SAMPLE_UNSIGNED,                 /* unsigned integer samples */
    RNC_SAMPLE_FLOATING,                 /* floating point samples */
} rnc_sampletype_t;


/**
 * @brief Sample endiannes.
 *
 * Specifies whether integer samples are LFB-first (little endian) or
 * MSB-first (big endian).
 */
typedef enum {
    RNC_ENDIAN_LITTLE = 0,               /* Hiawatha */
    RNC_ENDIAN_BIG,                      /* Winnetou */
} rnc_endian_t;


/**
 * @brief Helper functions/macros to map sampling rates to/from ids.
 */
typedef enum {
    RNC_SAMPLERATE_44100 = 0,
    RNC_SAMPLERATE_48000,
    RNC_SAMPLERATE_96000,
    RNC_SAMPLERATE_192000
} rnc_samplerate_t;
#define RNC_SAMPLING_RATES 44100, 48000, 96000, 192000

static inline int rnc_freq_id(int freq) {
    int freqs[] = { RNC_SAMPLING_RATES, -1};
    int i;

    for (i = 0; freqs[i] != -1; i++)
        if (freqs[i] == freq)
            return i;

    return -1;
}

static inline int rnc_id_freq(int id) {
    int freqs[] = { RNC_SAMPLING_RATES };

    if (0 <= id && id < (int)MRP_ARRAY_SIZE(freqs))
        return freqs[id];

    return -1;
}


/**
 * @brief Assigned bit-widths and offsets within a format identifier.
 *
 * These macros correspond to the layout for audio format identifiers
 * described above. If you change the layout, be sure to update these
 * macros and vice versa.
 */
#define __RNC_CMAP_BITS 5
#define __RNC_CMPR_BITS 5
#define __RNC_CHNL_BITS 3
#define __RNC_RATE_BITS 2
#define __RNC_BITS_BITS 2
#define __RNC_SMPL_BITS 2
#define __RNC_ENDN_BITS 1
#define __RNC_CMAP_OFFS (1 + 2 + 2 + 2 + 3 + 5)
#define __RNC_CMPR_OFFS (1 + 2 + 2 + 2 + 3)
#define __RNC_CHNL_OFFS (1 + 2 + 2 + 2)
#define __RNC_RATE_OFFS (1 + 2 + 2)
#define __RNC_BITS_OFFS (1 + 2)
#define __RNC_SMPL_OFFS (1)
#define __RNC_ENDN_OFFS (0)

/**
 * @brief Bit-fiddling helper macros.
 */
/**
 * @brief Bit-fiddling helper macros.
 */
#define __RNC_MASK(_nbit, _offs) (((1 << (((_nbit)-1) + 1)) - 1) << (_offs))
#define __RNC_BITS(_word, _nbit, _offs)                         \
    (((_word) & __RNC_MASK(_nbit, _offs)) >> _offs)
#define __RNC_FORMAT_BITS(_id, _what)                           \
    __RNC_BITS(_id, __RNC_##_what##_BITS, __RNC_##_what##_OFFS)

/**
 * @brief (Blindly) encode audio format into a format identifier.
 */
#define RNC_FORMAT_ID(_cmap, _cmpr, _chnl, _rate, _bits, _smpl, _endn)   \
    ((uint32_t)                                                          \
     ( (_cmap)           << __RNC_CMAP_OFFS) |                           \
     ( (_cmpr)           << __RNC_CMPR_OFFS) |                           \
     (((_chnl) - 1)      << __RNC_CHNL_OFFS) |                           \
     ( (_rate)           << __RNC_RATE_OFFS) |                           \
     (((_bits) / 8 - 1)  << __RNC_BITS_OFFS) |                           \
     ( (_smpl)           << __RNC_SMPL_OFFS) |                           \
     ( (_endn)           << __RNC_ENDN_OFFS))

/**
 * @brief Decode format information from a format identifier.
 */
#define RNC_FORMAT_CMAP(_id) ( __RNC_FORMAT_BITS(_id, CMAP)         )
#define RNC_FORMAT_CMPR(_id) ( __RNC_FORMAT_BITS(_id, CMPR)         )
#define RNC_FORMAT_CHNL(_id) ( __RNC_FORMAT_BITS(_id, CHNL) + 1     )
#define RNC_FORMAT_RATE(_id) ( __RNC_FORMAT_BITS(_id, RATE)         )
#define RNC_FORMAT_BITS(_id) ((__RNC_FORMAT_BITS(_id, BITS) + 1) * 8)
#define RNC_FORMAT_SMPL(_id) ( __RNC_FORMAT_BITS(_id, SMPL)         )
#define RNC_FORMAT_ENDN(_id) ( __RNC_FORMAT_BITS(_id, ENDN)         )


/**
 * @brief Initialize formats known to RNC.
 *
 * Initialize the format-handling bits of RNC.
 *
 * @param [in] rnc  RNC instance to initialize
 *
 * @return Returns 0 on success, -1 on error.
 */
int rnc_format_init(rnc_t *rnc);


/**
 * @brief Register a new audio compression/encoding scheme.
 *
 * Register an audio compression/encoding scheme with ripncode.
 *
 * @param [in] rnc     RNC instance to register scheme with
 * @param [in] format  format to register
 *
 * @return Returns the id of the scheme upon success, -1 on error.
 */
int rnc_compress_register(rnc_t *rnc, const char *name);


/**
 * @brief Look up the id of a given compression/encoding scheme by name.
 *
 * @param [in] rnc   RNC instance
 * @param [in] name  scheme name to look up id for
 *
 * @return Returns the id for the given scheme or -1 if it was not found.
 */
int rnc_compress_id(rnc_t *rnc, const char *name);


/**
 * @brief Look up the name of a given compression/encoding scheme name by id.
 *
 * @param [in] rnc  RNC instance
 * @param [in] id   scheme id
 *
 * @return Returns the name for the given id, or NULL if it was not found.
 */
const char *rnc_compress_name(rnc_t *rnc, int id);


#endif /* __RIPNCODE_FORMAT_H__ */

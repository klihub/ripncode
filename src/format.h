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
 * A registered audio format specification.
 */

typedef enum {
    RNC_ENCODING_UNKNOWN,                /* signed integer PCM */
    RNC_ENCODING_SIGNED,                 /* signed integer PCM */
    RNC_ENCODING_UNSIGNED,               /* unsigned integer PCM */
    RNC_ENCODING_FLOATING,               /* floating-point PCM */
    RNC_ENCODING_ALAW,                   /* a-law */
    RNC_ENCODING_ULAW,                   /* u-law */
    RNC_ENCODING_OTHER,                  /* compressed audio */
} rnc_encoding_t;

typedef enum {
    RNC_ENDIAN_LITTLE = 0,               /* Hiawatha */
    RNC_ENDIAN_BIG,                      /* Winnetou */
} rnc_endian_t;

struct rnc_format_s {
    mrp_list_hook_t   hook;              /* to list of registered formats */
    const char       *name;              /* registered format name */
    int               id;
};


/**
 * @brief Encoding for format identifiers.
 *
 * We encode audio formats into a single 32-bit word. The format
 * identifier contains the audio coding, the sampling rate, the
 * number of channels in use, the number of bits per sample, and
 * the endianness of samples. Currently we use only 23 bits in a
 * way depicted below. We have 9 more bits to spare in case we
 * need to squeeze more data into a format id. Here is the current
 * layout in use:
 *
 *     encoding              :  8 bits (registered encoding id)
 *     sample rate, 0 - 51200:  9 bits (rate / 100)
 *     channels, 1 - 8       :  3 bits (channels - 1)
 *     bits, 8/16/24/32      :  2 bits (bits / 8 - 1)
 *     endianness, little/big:  1 bit  (0 = little, 1 = big)
 * ------------------------------------
 *                             23 bits
 *
 * Format id layout:
 *
 * |  22-15 : 14-6 :   5-3    :  2-1 :    0   |
 * | format : rate : channels : bits : endian |
 * |    8   :   9  :    3     :   2  :    1   |
 *
 */

/**
 * @brief Various bit-widths and offsets within a format identifier.
 */
#define __RNC_FRMT_BITS 8
#define __RNC_RATE_BITS 9
#define __RNC_CHNL_BITS 3
#define __RNC_BITS_BITS 2
#define __RNC_ENDN_BITS 1
#define __RNC_FRMT_SHFT (1 + 2 + 3 + 9)
#define __RNC_RATE_SHFT (1 + 2 + 3)
#define __RNC_CHNL_SHFT (1 + 2)
#define __RNC_BITS_SHFT (1)
#define __RNC_ENDN_SHFT (0)


/**
 * @brief Bit-fiddling helper macros.
 */
#define __RNC_MASK(_nbit, _offs) (((1 << (((_nbit)-1) + 1)) - 1) << (_offs))
#define __RNC_BITS(_word, _nbit, _offs)                         \
    (((_word) & __RNC_MASK(_nbit, _offs)) >> _offs)
#define __RNC_FORMAT_BITS(_id, _what)                           \
    __RNC_BITS(_id, __RNC_##_what##_BITS, __RNC_##_what##_SHFT)

/**
 * @brief Encode format information into a format identifier.
 */
#define RNC_FORMAT_ID(_frmt, _rate, _chnl, _bits, _endn)   \
    ((uint32_t)                                            \
     (( (_frmt)           << __RNC_FRMT_SHFT) |            \
      (((_rate) / 100  )  << __RNC_RATE_SHFT) |            \
      (((_chnl) -   1  )  << __RNC_CHNL_SHFT) |            \
      (((_bits) / 8 - 1)  << __RNC_BITS_SHFT) |            \
      ( (_endn)           << __RNC_ENDN_SHFT)))

/**
 * @brief Decode various format information from a format identifier.
 */
#define RNC_FORMAT_ENDN(_id) ( __RNC_FORMAT_BITS(_id, ENDN)         )
#define RNC_FORMAT_BITS(_id) ((__RNC_FORMAT_BITS(_id, BITS) + 1) * 8)
#define RNC_FORMAT_CHNL(_id) ( __RNC_FORMAT_BITS(_id, CHNL) + 1     )
#define RNC_FORMAT_RATE(_id) ( __RNC_FORMAT_BITS(_id, RATE) * 100   )
#define RNC_FORMAT_FRMT(_id) ( __RNC_FORMAT_BITS(_id, FRMT)         )


/**
 * @brief Initialize formats known to RNC.
 *
 * Initialize the format-handling bits of RNC.
 *
 * @param [in] rnc  ripncode instance to initialize
 *
 * @return Returns 0 on success, -1 on error.
 */
int rnc_format_init(rnc_t *rnc);

/**
 * @brief Register a new audio format.
 *
 * Register an audio format with ripncode.
 *
 * @param [in] format  format to register
 *
 * @return Returns the id of the format upon success, -1 on error.
 */
int rnc_format_register(rnc_t *rnc, rnc_format_t *format);


/**
 * @brief Look up the id of a given format name.
 *
 * @param [in] rnc   RNC instance
 * @param [in] name  format name to look up id for
 *
 * @return Returns the id for the given format or -1 if it was not found.
 */
int rnc_format_lookup(rnc_t *rnc, const char *name);

/**
 * @brief Look up the name of a given format id.
 *
 * @param [in] rnc  RNC instance
 * @param [in] id   format id
 *
 * @return Returns the name for the given id, or NULL if it was not found.
 */
const char *rnc_format_name(rnc_t *rnc, int id);

#endif /* __RIPNCODE_FORMAT_H__ */

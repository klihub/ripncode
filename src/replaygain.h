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

#ifndef __RIPNCODE_REPLAYGAIN_H__
#define __RIPNCODE_REPLAYGAIN_H__

#include <ripncode/ripncode.h>

MRP_CDECL_BEGIN

/**
 * @brief Analyze album/tracks for EBU R128 loudness and replaygain.
 */

/**
 * @brief Create and initialize replaygain analyzer context.
 *
 * Create and initialize a replaygain/EBU R128 analyzer context
 * for the given number of tracks in the given format. This function
 * dynamically allocates an analyzer context and calls rnc_gain_init
 * on it.
 *
 * @param [in] ntrack  number of tracks to create calculator for
 * @param [in] format  format of tracks
 *
 * @return Returns a replaygain analyzer context.
 */
rnc_gain_t *rnc_gain_create(int ntrack, uint32_t format);

/**
 * @brief Initialize given replaygain analyzer context.
 *
 * Initialize the given analyzer context for the given number of
 * tracks in the given format.
 *
 * @param [in] ntrack  number of tracks to create calculator for
 * @param [in] format  format of tracks
 *
 * @return Returns a replaygain analyzer context.
 */
int rnc_gain_init(rnc_gain_t *g, int ntrack, uint32_t format);

/**
 * @brief Destroy an analyzer context.
 *
 * Destroy an analyzer context created with rnc_gain_create.
 * This function calls rnc_gain_exit on the given context then
 * frees it. Note that currently only 16-bit signed stereo format
 * is supported.
 *
 * @param [in] g  replaygain analyzer context to destroy
 */
void rnc_gain_destroy(rnc_gain_t *g);

/**
 * @brief Deinitialize the given analyzer context.
 *
 * Free all resources allocated and internally associated with the
 * given context.
 *
 * @param [in] g  replaygain analyzer context to uninitialize.
 */
void rnc_gain_exit(rnc_gain_t *g);

/**
 * @brief Analyze the given samples of the given track.
 *
 * Analyze the given samples of the given track for EBU R128 loudness and
 * replaygain. samples must point to a buffer of interleaved samples with
 * nsample entries *per channel*.
 *
 * @param [in] g        replaygain analyzer context
 * @param [in] track    track to associate samples with
 * @param [in] samples  interleaved sample buffer to analyze
 * @param [in] nsample  number of samples per channel
 *
 * @return Returns 0 on success, -1 otherwise.
 */
int rnc_gain_analyze(rnc_gain_t *g, int track, char *samples, int nsample);

/**
 * @brief Calculate EBU R128 integrated loudness for the given track.
 *
 * Calculate EBU R128 integrated loudness for the given track.
 *
 * @brief [in] g      replaygain analyzer context
 * @brief [in] track  track to calculate loudness for
 *
 * @return Returns the calculated loudness on success, 0.0 on error in which
 *         case errno is also set.
 */
double rnc_gain_track_loudness(rnc_gain_t *g, int track);

/**
 * @brief Calculate EBU R128 loudness range for the given track.
 *
 * Calculate EBU R128 loudness range for the given track.
 *
 * @brief [in] g      replaygain analyzer context
 * @brief [in] track  track to calculate loudness range for
 *
 * @return Returns the calculated loudness range on success, 0.0 on error
 *         in which case errno is also set.
 */
double rnc_gain_track_range(rnc_gain_t *g, int track);

/**
 * @brief Calculate ReplayGain 1.0 gain for the given track.
 *
 * Calculate ReplayGain 1.0 compliant gain for the given track.
 *
 * @brief [in] g      replaygain analyzer context
 * @brief [in] track  track to calculate gain for
 *
 * @return Returns the calculated gain on success, 0.0 on error
 *         in which case errno is also set.
 */
double rnc_gain_track_gain(rnc_gain_t *g, int track);

/**
 * @brief Calculate peak for the given track.
 *
 * Calculate peakfor the given track.
 *
 * @brief [in] g      replaygain analyzer context
 * @brief [in] track  track to calculate peak for
 *
 * @return Returns the calculated peak on success, 0.0 on error
 *         in which case errno is also set.
 */
double rnc_gain_track_peak(rnc_gain_t *g, int track);

/**
 * @brief Calculate ReplayGain 1.0 gain for the whole album (all tracks).
 *
 * Calculate ReplayGain 1.0 compliant gain for the whole album.
 *
 * @brief [in] g      replaygain analyzer context
 *
 * @return Returns the calculated gain on success, 0.0 on error
 *         in which case errno is also set.
 */
double rnc_gain_album_gain(rnc_gain_t *g);

MRP_CDECL_END

#endif /* __RIPNCODE_REPLAYGAIN_H__ */

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

#ifndef __RIPNCODE_BUFFER_H__
#define __RIPNCODE_BUFFER_H__

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <ripncode/ripncode.h>


/**
 * @brief Create a new data collection buffer.
 */
rnc_buf_t *rnc_buf_create(const char *name, size_t pre_alloc, size_t chunk_size);


/**
 * @brief Create a new buffer frontend to a file.
 */
rnc_buf_t *rnc_buf_open(const char *path, int flags, mode_t mode);


/**
 * @brief Destroy a buffer.
 */
int rnc_buf_close(rnc_buf_t *buf);


/**
 * @brief Destroy a buffer, remove any corresponding file.
 */
int rnc_buf_unlink(rnc_buf_t *buf);


/**
 * @brief Write data to a buffer.
 */
int rnc_buf_write(rnc_buf_t *b, const void *buf, size_t size);


/**
 * @brief Read data from a buffer.
 */
int rnc_buf_read(rnc_buf_t *b, void *buf, size_t size);


/**
 * @brief Reposition the write pointer within the buffer.
 */
off_t rnc_buf_wseek(rnc_buf_t *b, off_t offset, int whence);


/**
 * @brief Reposition the read pointer within the buffer.
 */
off_t rnc_buf_rseek(rnc_buf_t *b, off_t offset, int whence);


/**
 * @brief Get the current write offset within the buffer.
 */
off_t rnc_buf_tell(rnc_buf_t *b);


#endif /* __RIPNCODE_BUFFER_H__ */

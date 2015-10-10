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

#include <ripncode/ripncode.h>

#define DEFAULT_CHUNK_SIZE (64 * 1024)


/*
 * buffer API functions
 */
typedef struct {
    union {
        int (*mopen)(rnc_buf_t *b, size_t pre_alloc);
        int (*fopen)(rnc_buf_t *b, int flags, mode_t mode);
    };
    int (*write)(rnc_buf_t *b, const void *buf, size_t size);
    int (*read)(rnc_buf_t *b, void *buf, size_t size);
    off_t (*wseek)(rnc_buf_t *b, off_t offset, int whence);
    off_t (*rseek)(rnc_buf_t *b, off_t offset, int whence);
    int (*close)(rnc_buf_t *b);
    int (*unlink)(rnc_buf_t *b);
} buf_api_t;


/*
 * generic buffer type
 */
struct rnc_buf_s {
    char      *name;
    buf_api_t *api;
};


/*
 * an in-memory buffer
 */
typedef struct {
    char      *name;                     /* buffer name */
    buf_api_t *api;                      /* buffer API functions */
    size_t     chunk;                    /* buffer allocation chunk */
    char      *buf;                      /* actual buffer */
    size_t     size;                     /* current buffer size */
    size_t     data;                     /* amount of data in buffer */
    char      *w;                        /* write pointer */
    char      *r;                        /* read pointer */
} mem_buf_t;


/*
 * a file buffer
 */
typedef struct {
    char      *name;                     /* buffer name */
    buf_api_t *api;                      /* buffer API functions */
    char      *path;                     /* buffer file path */
    int        wfd;                      /* file descriptor for write */
    int        rfd;                      /* file descriptor for read */
} file_buf_t;


static int mem_open(rnc_buf_t *b, size_t pre_alloc);
static int mem_write(rnc_buf_t *b, const void *buf, size_t size);
static int mem_read(rnc_buf_t *b, void *buf, size_t size);
static off_t mem_wseek(rnc_buf_t *b, off_t offset, int whence);
static off_t mem_rseek(rnc_buf_t *b, off_t offset, int whence);
static int mem_close(rnc_buf_t *b);
static int mem_unlink(rnc_buf_t *b);
static int file_open(rnc_buf_t *b, int flags, mode_t mode);
static int file_write(rnc_buf_t *b, const void *buf, size_t size);
static int file_read(rnc_buf_t *b, void *buf, size_t size);
static off_t file_wseek(rnc_buf_t *b, off_t offset, int whence);
static off_t file_rseek(rnc_buf_t *b, off_t offset, int whence);
static int file_close(rnc_buf_t *b);
static int file_unlink(rnc_buf_t *b);


static rnc_buf_t *buf_alloc(const char *name, buf_api_t *api, size_t size)
{
    rnc_buf_t *b;

    if ((b = mrp_allocz(size)) == NULL || (b->name = mrp_strdup(name)) == NULL)
        goto nomem;

    b->api = api;

    return b;

 nomem:
    mrp_free(b);
    return NULL;
}


static void buf_free(rnc_buf_t *b)
{
    if (b == NULL)
        return;

    mrp_free(b->name);
    mrp_free(b);
}


rnc_buf_t *rnc_buf_create(const char *name, size_t pre_alloc, size_t chunk_size)
{
    static buf_api_t api = {
        { .mopen  = mem_open, },
          .write  = mem_write,
          .read   = mem_read,
          .wseek  = mem_wseek,
          .rseek  = mem_rseek,
          .close  = mem_close,
          .unlink = mem_unlink,
    };

    mem_buf_t *b;

    b = (mem_buf_t *)buf_alloc(name, &api, sizeof(*b));

    if (b == NULL)
        return NULL;

    b->chunk = chunk_size ? chunk_size : DEFAULT_CHUNK_SIZE;

    if (b->api->mopen((rnc_buf_t *)b, pre_alloc) < 0)
        goto fail;

    return (rnc_buf_t *)b;

 fail:
    buf_free((rnc_buf_t *)b);

    return NULL;
}


rnc_buf_t *rnc_buf_open(const char *path, int flags, mode_t mode)
{
    static buf_api_t api = {
        { .fopen  = file_open, },
          .write  = file_write,
          .read   = file_read,
          .wseek  = file_wseek,
          .rseek  = file_rseek,
          .close  = file_close,
          .unlink = file_unlink,
    };

    file_buf_t *b;

    b = (file_buf_t *)buf_alloc(path, &api, sizeof(*b));

    if (b == NULL)
        return NULL;

    b->path = b->name;

    if (b->api->fopen((rnc_buf_t *)b, flags, mode) < 0)
        goto fail;

    return (rnc_buf_t *)b;

 fail:
    buf_free((rnc_buf_t *)b);

    return NULL;
}


int rnc_buf_close(rnc_buf_t *b)
{
    return b->api->close(b);
}


int rnc_buf_unlink(rnc_buf_t *b)
{
    return b->api->unlink(b);
}


int rnc_buf_write(rnc_buf_t *b, const void *buf, size_t size)
{
    mrp_debug("writing %zu bytes of data to buffer '%s'", size, b->name);

    return b->api->write(b, buf, size);
}


int rnc_buf_read(rnc_buf_t *b, void *buf, size_t size)
{
    return b->api->read(b, buf, size);
}


off_t rnc_buf_wseek(rnc_buf_t *b, off_t offset, int whence)
{
    return b->api->wseek(b, offset, whence);
}


off_t rnc_buf_rseek(rnc_buf_t *b, off_t offset, int whence)
{
    return b->api->rseek(b, offset, whence);
}


off_t rnc_buf_tell(rnc_buf_t *b)
{
    return b->api->wseek(b, 0, SEEK_CUR);
}


static int mem_open(rnc_buf_t *b, size_t pre_alloc)
{
    mem_buf_t *m = (mem_buf_t *)b;

    if (!pre_alloc)
        return 0;

    mrp_debug("preallocating %zu bytes for buffer '%s'", pre_alloc, b->name);

    m->buf = mrp_allocz(pre_alloc);

    if (m->buf == NULL)
        return -1;

    m->w = m->r = m->buf;
    m->size = pre_alloc;

    return 0;
}


static int mem_write(rnc_buf_t *b, const void *buf, size_t size)
{
    mem_buf_t *m = (mem_buf_t *)b;
    size_t     diff, woffs;

    if (m->w + size > m->buf + m->size) {
        diff = m->w + size - (m->buf + m->size);

        if (diff < m->chunk)
            diff = m->chunk;

        woffs = m->w - m->buf;

        if (!mrp_reallocz(m->buf, m->size, m->size + diff))
            goto nomem;

        m->size += diff;
        m->w     = m->buf + woffs;
    }

    memcpy(m->w, buf, size);
    m->w += size;
    m->r  = m->buf;

    if (m->w - m->buf > (ptrdiff_t)m->data)
        m->data = m->w - m->buf;

    return size;

 nomem:
    return -1;
}


static int mem_read(rnc_buf_t *b, void *buf, size_t size)
{
    mem_buf_t *m = (mem_buf_t *)b;

    mrp_debug("reading %zu bytes of data from buffer '%s'", size, b->name);

    if (m->r + size >= m->buf + m->data)
        size = m->data - (m->r - m->buf);

    if (size == 0)
        return 0;

    memcpy(buf, m->r, size);
    m->r    += size;

    return (int)size;
}


static off_t mem_wseek(rnc_buf_t *b, off_t offset, int whence)
{
    mem_buf_t *m = (mem_buf_t *)b;

    mrp_debug("seeking to %ld offset (whence: %d)", offset, whence);

    switch (whence) {
    case SEEK_SET:
        if (offset >= (off_t)m->size)
            goto invalid;

        m->w = m->buf + offset;
        break;

    case SEEK_CUR:
        if (m->w + offset - m->buf > (ptrdiff_t)m->data)
            goto invalid;

        m->w += offset;
        break;

    case SEEK_END:
    default:
        goto invalid;
    }

    return (off_t)(m->w - m->buf);

 invalid:
    errno = EINVAL;
    return -1;
}


static off_t mem_rseek(rnc_buf_t *b, off_t offset, int whence)
{
    mem_buf_t *m = (mem_buf_t *)b;

    mrp_debug("seeking to read offset %ld (whence: %d)", offset, whence);

    switch (whence) {
    case SEEK_SET:
        if (offset >= (off_t)m->size)
            goto invalid;

        m->r = m->buf + offset;
        break;

    case SEEK_CUR:
        if (m->r + offset - m->buf > (ptrdiff_t)m->data)
            goto invalid;

        m->r += offset;
        break;

    case SEEK_END:
        if (offset > 0 || -offset > (off_t)m->data)
            goto invalid;

        m->r = m->buf + m->data + offset;
        break;

    default:
        goto invalid;
    }

    return (off_t)(m->r - m->buf);

 invalid:
    errno = EINVAL;
    return -1;
}


static int mem_close(rnc_buf_t *b)
{
    mem_buf_t *m = (mem_buf_t *)b;

    mrp_free(m->buf);
    m->buf  = NULL;
    m->w    = NULL;
    m->r    = NULL;
    m->size = 0;
    m->data = 0;

    return 0;
}


static int mem_unlink(rnc_buf_t *b)
{
    return mem_close(b);
}


static int file_open(rnc_buf_t *b, int flags, mode_t mode)
{
    file_buf_t *f = (file_buf_t *)b;

    mrp_debug("opening file '%s' for buffer '%s'", f->path, f->name);

    f->wfd = open(f->path, flags, mode);
    f->rfd = open(f->path, flags, mode);

    if (f->wfd < 0 || f->rfd < 0)
        goto ioerror;

    return 0;

 ioerror:
    errno = EIO;
    close(f->wfd);
    close(f->rfd);
    f->wfd = f->rfd = -1;
    return -1;
}


static int file_write(rnc_buf_t *b, const void *buf, size_t size)
{
    file_buf_t *f = (file_buf_t *)b;

    if (write(f->wfd, buf, size) != (ssize_t)size)
        return -1;
    else
        return 0;
}


static int file_read(rnc_buf_t *b, void *buf, size_t size)
{
    file_buf_t *f = (file_buf_t *)b;

    mrp_debug("reading %zu bytes of data from buffer '%s'", size, b->name);

    /* XXX TODO: hmm... should we have a separate read and write fds ? */

    return read(f->rfd, buf, size);
}


static off_t file_wseek(rnc_buf_t *b, off_t offset, int whence)
{
    file_buf_t *f = (file_buf_t *)b;

    return lseek(f->wfd, offset, whence);
}


static off_t file_rseek(rnc_buf_t *b, off_t offset, int whence)
{
    file_buf_t *f = (file_buf_t *)b;

    return lseek(f->rfd, offset, whence);
}


static int file_close(rnc_buf_t *b)
{
    file_buf_t *f = (file_buf_t *)b;

    close(f->wfd);
    close(f->rfd);
    f->wfd = -1;
    f->rfd = -1;

    mrp_free(f->path);
    f->path = f->name = NULL;

    mrp_free(f);

    return 0;
}


static int file_unlink(rnc_buf_t *b)
{
    file_buf_t *f = (file_buf_t *)b;

    unlink(f->path);

    return file_close(b);
}

/**
 * fif_io.h
 */
#ifndef __FIF_IO_H
#define __FIF_IO_H

// types
#include "libfif/fif_types.h"

// opaque io handle type
typedef uintptr_t fif_io_userdata;

// IO callbacks
typedef int(*fif_io_read)(fif_io_userdata userdata, void *buffer, unsigned int count);
typedef int(*fif_io_write)(fif_io_userdata userdata, const void *buffer, unsigned int count);
typedef int64_t(*fif_io_seek)(fif_io_userdata userdata, fif_offset_t offset, enum FIF_SEEK_MODE mode);
typedef int(*fif_io_zero)(fif_io_userdata userdata, fif_offset_t offset, unsigned int count);
typedef int(*fif_io_ftruncate)(fif_io_userdata userdata, fif_offset_t newsize);
typedef int64_t(*fif_io_filesize)(fif_io_userdata userdata);

// IO callbacks
typedef struct
{
    fif_io_read io_read;
    fif_io_write io_write;
    fif_io_seek io_seek;
    fif_io_zero io_zero;
    fif_io_ftruncate io_ftruncate;
    fif_io_filesize io_filesize;
    fif_io_userdata userdata;
} fif_io;

// local helper functions
LIBFIF_API int fif_io_open_local_file(const char *path, unsigned int mode, fif_io *out_io);
LIBFIF_API int fif_io_close_local_file(const fif_io *io);

// memory helper functions
LIBFIF_API int fif_io_open_memory(fif_io *out_io);
LIBFIF_API int fif_io_close_memory(const fif_io *io);

#endif      // __FIF_IO_H

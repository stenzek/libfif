#ifndef __TRACE_STREAM_H
#define __TRACE_STREAM_H

#include "fif_internal.h"
#include "trace_format.h"
#include <zlib.h>

struct fif_trace_stream
{
    fif_io io;
    z_stream zstream;
    char *in_buffer;
    unsigned int in_buffer_position;
    unsigned int in_buffer_size;
    char *out_buffer;
    unsigned int out_buffer_position;
    unsigned int out_buffer_size;
};

int trace_stream_reader_init(struct fif_trace_stream **out_stream, const fif_io *io);
int trace_stream_read_bytes(struct fif_trace_stream *stream, void *buffer, unsigned int nbytes);
int trace_stream_read_byte(struct fif_trace_stream *stream, unsigned char *out_byte);
int trace_stream_read_int(struct fif_trace_stream *stream, int *out_int);
int trace_stream_read_uint(struct fif_trace_stream *stream, unsigned int *out_uint);
int trace_stream_read_long(struct fif_trace_stream *stream, long long *out_long);
int trace_stream_read_ulong(struct fif_trace_stream *stream, unsigned long long *out_ulong);
int trace_stream_read_string(struct fif_trace_stream *stream, char **out_string);
int trace_stream_reader_finish(struct fif_trace_stream *stream);

int trace_stream_writer_init(struct fif_trace_stream **out_stream, const fif_io *io);
int trace_stream_write_bytes(struct fif_trace_stream *stream, const void *buffer, unsigned int nbytes);
int trace_stream_write_byte(struct fif_trace_stream *stream, unsigned char value);
int trace_stream_write_int(struct fif_trace_stream *stream, int value);
int trace_stream_write_uint(struct fif_trace_stream *stream, unsigned int value);
int trace_stream_write_long(struct fif_trace_stream *stream, long long value);
int trace_stream_write_ulong(struct fif_trace_stream *stream, unsigned long long value);
int trace_stream_write_string(struct fif_trace_stream *stream, const char *str);
int trace_stream_writer_finish(struct fif_trace_stream *stream);

#endif      // __TRACE_STREAM_H
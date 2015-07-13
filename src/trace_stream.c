#include "trace_stream.h"
#include <stdio.h>

#define TRACE_STREAM_BUFFER_SIZE (4096)

static int trace_stream_alloc(struct fif_trace_stream **out_stream)
{
    struct fif_trace_stream *stream = (struct fif_trace_stream *)malloc(sizeof(struct fif_trace_stream));
    if (stream == NULL)
        return FIF_ERROR_OUT_OF_MEMORY;

    memset(stream, 0, sizeof(struct fif_trace_stream));
    if ((stream->in_buffer = (char *)malloc(TRACE_STREAM_BUFFER_SIZE)) == NULL)
    {
        free(stream);
        return FIF_ERROR_OUT_OF_MEMORY;
    }
    if ((stream->out_buffer = (char *)malloc(TRACE_STREAM_BUFFER_SIZE)) == NULL)
    {
        free(stream);
        return FIF_ERROR_OUT_OF_MEMORY;
    }
    
    *out_stream = stream;
    return FIF_ERROR_SUCCESS;
}

static int trace_stream_fill_buffer(struct fif_trace_stream *stream)
{
    int result;
    if ((result = stream->io.io_read(stream->io.userdata, stream->in_buffer, TRACE_STREAM_BUFFER_SIZE)) < 0)
        return result;

    stream->in_buffer_position = 0;
    stream->in_buffer_size = (unsigned int)result;

    // update inflate state
    stream->zstream.next_in = (Bytef *)stream->in_buffer;
    stream->zstream.avail_in = stream->in_buffer_size;
    return result;
}

static int trace_stream_flush_buffer(struct fif_trace_stream *stream)
{
    unsigned int write_count = TRACE_STREAM_BUFFER_SIZE - stream->zstream.avail_out;

    int result;
    if ((result = stream->io.io_write(stream->io.userdata, stream->out_buffer, write_count)) != (int)write_count)
        return (result >= 0) ? FIF_ERROR_IO_ERROR : result;

    stream->out_buffer_position = 0;

    // update deflate state
    stream->zstream.next_out = (Bytef *)stream->out_buffer;
    stream->zstream.avail_out = TRACE_STREAM_BUFFER_SIZE;
    return FIF_ERROR_SUCCESS;
}

int trace_stream_reader_init(struct fif_trace_stream **out_stream, const fif_io *io)
{
    int result;
    struct fif_trace_stream *stream;
    if ((result = trace_stream_alloc(&stream)) != FIF_ERROR_SUCCESS)
        return result;

    // setup io
    memcpy(&stream->io, io, sizeof(fif_io));
    if ((result = (int)stream->io.io_seek(stream->io.userdata, 0, FIF_SEEK_MODE_SET)) != FIF_ERROR_SUCCESS)
        return result;

    // initialize inflate
    if ((result = inflateInit(&stream->zstream)) != Z_OK)
        return FIF_ERROR_COMPRESSOR_ERROR;

    // read in a buffer chunk
    if ((result = trace_stream_fill_buffer(stream)) < 0)
        return result;

    // ready to go
    *out_stream = stream;
    return FIF_ERROR_SUCCESS;
}

int trace_stream_read_bytes(struct fif_trace_stream *stream, void *buffer, unsigned int nbytes)
{
    // setup output pointer
    stream->zstream.next_out = buffer;
    stream->zstream.avail_out = nbytes;

    // loop
    int result;
    while (stream->zstream.avail_out > 0)
    {
        // try to inflate
        result = inflate(&stream->zstream, Z_NO_FLUSH);
        if (result == Z_STREAM_END)
        {
            if (stream->zstream.avail_out > 0)
                return FIF_ERROR_END_OF_FILE;
            else
                break;
        }

        // check error codes
        if (result != Z_OK && result != Z_BUF_ERROR)
        {
            fprintf(stderr, "trace_stream_read_bytes: inflate() returned %i", result);
            return FIF_ERROR_COMPRESSOR_ERROR;
        }

        // needs more input?
        if (stream->zstream.avail_in == 0 && stream->zstream.avail_out > 0)
        {
            // add more input
            if ((result = trace_stream_fill_buffer(stream)) < 0)
                return result;

            // has more input?
            if (result == 0)
                return FIF_ERROR_END_OF_FILE;
        }
    }

    return (int)(nbytes - stream->zstream.avail_out);
}

int trace_stream_read_byte(struct fif_trace_stream *stream, unsigned char *out_byte)
{
    int result;
    if ((result = trace_stream_read_bytes(stream, out_byte, sizeof(unsigned char))) != sizeof(unsigned char))
        return (result >= 0) ? FIF_ERROR_BAD_OFFSET : result;

    return FIF_ERROR_SUCCESS;
}

int trace_stream_read_int(struct fif_trace_stream *stream, int *out_int)
{
    int result;
    if ((result = trace_stream_read_bytes(stream, out_int, sizeof(int))) != sizeof(int))
        return (result >= 0) ? FIF_ERROR_BAD_OFFSET : result;

    return FIF_ERROR_SUCCESS;
}

int trace_stream_read_uint(struct fif_trace_stream *stream, unsigned int *out_uint)
{
    int result;
    if ((result = trace_stream_read_bytes(stream, out_uint, sizeof(unsigned int))) != sizeof(unsigned int))
        return (result >= 0) ? FIF_ERROR_BAD_OFFSET : result;

    return FIF_ERROR_SUCCESS;
}

int trace_stream_read_long(struct fif_trace_stream *stream, long long *out_long)
{
    int result;
    if ((result = trace_stream_read_bytes(stream, out_long, sizeof(long long))) != sizeof(long long))
        return (result >= 0) ? FIF_ERROR_BAD_OFFSET : result;

    return FIF_ERROR_SUCCESS;
}

int trace_stream_read_ulong(struct fif_trace_stream *stream, unsigned long long *out_ulong)
{
    int result;
    if ((result = trace_stream_read_bytes(stream, out_ulong, sizeof(unsigned long long))) != sizeof(unsigned long long))
        return (result >= 0) ? FIF_ERROR_BAD_OFFSET : result;

    return FIF_ERROR_SUCCESS;
}

int trace_stream_read_string(struct fif_trace_stream *stream, char **out_string)
{
    unsigned int str_reserve = 16;
    unsigned int str_length = 0;
    char *str = (char *)malloc(str_reserve);
    if (str == NULL)
        return FIF_ERROR_OUT_OF_MEMORY;

    int result;
    for (;;)
    {
        char ch;
        if ((result = trace_stream_read_bytes(stream, &ch, 1)) != 1)
            return (result < 0) ? result : FIF_ERROR_BAD_OFFSET;

        if (ch == '\0')
            break;

        if ((str_length + 1) == str_reserve)
        {
            str_reserve *= 2;
            char *new_str = (char *)realloc(str, str_reserve);
            if (new_str == NULL)
            {
                free(str);
                return FIF_ERROR_OUT_OF_MEMORY;
            }

            str = new_str;
        }

        str[str_length++] = ch;
    }

    str[str_length] = '\0';
    *out_string = str;
    return FIF_ERROR_SUCCESS;
}

int trace_stream_reader_finish(struct fif_trace_stream *stream)
{
    inflateEnd(&stream->zstream);
    free(stream->in_buffer);
    free(stream->out_buffer);
    return FIF_ERROR_SUCCESS;
}

int trace_stream_writer_init(struct fif_trace_stream **out_stream, const fif_io *io)
{
    int result;
    struct fif_trace_stream *stream;
    if ((result = trace_stream_alloc(&stream)) != FIF_ERROR_SUCCESS)
        return result;

    // setup io
    memcpy(&stream->io, io, sizeof(fif_io));
    if ((result = (int)stream->io.io_seek(stream->io.userdata, 0, FIF_SEEK_MODE_SET)) != FIF_ERROR_SUCCESS)
        return result;

    // initialize deflate
    stream->zstream.avail_out = TRACE_STREAM_BUFFER_SIZE;
    stream->zstream.next_out = (Bytef *)stream->out_buffer;
    if ((result = deflateInit(&stream->zstream, Z_DEFAULT_COMPRESSION)) != Z_OK)
        return FIF_ERROR_COMPRESSOR_ERROR;

    // ready to go
    *out_stream = stream;
    return FIF_ERROR_SUCCESS;
}

int trace_stream_write_bytes(struct fif_trace_stream *stream, const void *buffer, unsigned int nbytes)
{
    stream->zstream.next_in = (Bytef *)buffer;
    stream->zstream.avail_in = nbytes;

    while (stream->zstream.avail_in > 0)
    {
        int result = deflate(&stream->zstream, Z_NO_FLUSH);
        if (result != Z_OK && result != Z_BUF_ERROR)
        {
            fprintf(stderr, "trace_stream_write_bytes: deflate() returned %i", result);
            return FIF_ERROR_COMPRESSOR_ERROR;
        }

        if (stream->zstream.avail_out == 0)
        {
            if ((result = trace_stream_flush_buffer(stream)) != FIF_ERROR_SUCCESS)
                return result;
        }
    }

    return (int)nbytes;
}

int trace_stream_write_byte(struct fif_trace_stream *stream, unsigned char value)
{
    int result;
    if ((result = trace_stream_write_bytes(stream, &value, sizeof(unsigned char))) != sizeof(unsigned char))
        return (result >= 0) ? FIF_ERROR_BAD_OFFSET : result;

    return FIF_ERROR_SUCCESS;
}

int trace_stream_write_int(struct fif_trace_stream *stream, int value)
{
    int result;
    if ((result = trace_stream_write_bytes(stream, &value, sizeof(int))) != sizeof(int))
        return (result >= 0) ? FIF_ERROR_BAD_OFFSET : result;

    return FIF_ERROR_SUCCESS;
}

int trace_stream_write_uint(struct fif_trace_stream *stream, unsigned int value)
{
    int result;
    if ((result = trace_stream_write_bytes(stream, &value, sizeof(unsigned long long))) != sizeof(unsigned long long))
        return (result >= 0) ? FIF_ERROR_BAD_OFFSET : result;

    return FIF_ERROR_SUCCESS;
}

int trace_stream_write_long(struct fif_trace_stream *stream, long long value)
{
    int result;
    if ((result = trace_stream_write_bytes(stream, &value, sizeof(long long))) != sizeof(long long))
        return (result >= 0) ? FIF_ERROR_BAD_OFFSET : result;

    return FIF_ERROR_SUCCESS;
}

int trace_stream_write_ulong(struct fif_trace_stream *stream, unsigned long long value)
{
    int result;
    if ((result = trace_stream_write_bytes(stream, &value, sizeof(unsigned long long))) != sizeof(unsigned long long))
        return (result >= 0) ? FIF_ERROR_BAD_OFFSET : result;

    return FIF_ERROR_SUCCESS;
}

int trace_stream_write_string(struct fif_trace_stream *stream, const char *str)
{
    int length = (int)strlen(str);

    int result;
    if ((result = trace_stream_write_bytes(stream, str, length + 1)) != (length + 1))
        return (result >= 0) ? FIF_ERROR_BAD_OFFSET : result;

    return FIF_ERROR_SUCCESS;
}

int trace_stream_writer_finish(struct fif_trace_stream *stream)
{
    int result;

    for (;;)
    {
        result = deflate(&stream->zstream, Z_FINISH);
        if (result == Z_STREAM_END)
            break;

        if (result != Z_OK && result != Z_BUF_ERROR)
        {
            fprintf(stderr, "trace_stream_writer_finish: deflate() returned %i", result);
            return FIF_ERROR_COMPRESSOR_ERROR;
        }

        if (stream->zstream.avail_out == 0)
        {
            // flush output buffer
            if ((result = trace_stream_flush_buffer(stream)) != FIF_ERROR_SUCCESS)
                return result;
        }
    }

    if (stream->zstream.avail_out != TRACE_STREAM_BUFFER_SIZE)
    {
        if ((result = trace_stream_flush_buffer(stream)) != FIF_ERROR_SUCCESS)
            return result;
    }

    deflateEnd(&stream->zstream);
    free(stream->in_buffer);
    free(stream->out_buffer);
    return FIF_ERROR_SUCCESS;
}

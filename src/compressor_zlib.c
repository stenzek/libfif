#include "fif_internal.h"
#include <zlib.h>

#define INTERNAL_ZLIB_BUFFER_SIZE (32768)

struct zlib_state
{
    z_stream stream;
    unsigned char compressed_data_buffer[INTERNAL_ZLIB_BUFFER_SIZE];
    unsigned int offset;
    unsigned int transferred;
};

int zlib_compressor_init(fif_mount_handle mount, fif_inode_index_t inode_index, FIF_VOLUME_FORMAT_INODE *inode, void **out_compressor_data, int compression_level)
{
    (void)mount;
    (void)inode;
    (void)inode_index;

    // allocate state
    struct zlib_state *state = (struct zlib_state *)malloc(sizeof(struct zlib_state));
    if (state == NULL)
        return FIF_ERROR_OUT_OF_MEMORY;

    if (compression_level < Z_NO_COMPRESSION)
        compression_level = Z_NO_COMPRESSION;
    else if (compression_level > Z_BEST_COMPRESSION)
        compression_level = Z_BEST_COMPRESSION;

    // set up initial deflate state
    memset(&state->stream, 0, sizeof(state->stream));
    state->stream.next_out = state->compressed_data_buffer;
    state->stream.avail_out = INTERNAL_ZLIB_BUFFER_SIZE;
    state->offset = 0;
    state->transferred = 0;

    // initialize zlib
    if (deflateInit(&state->stream, compression_level) != Z_OK)
    {
        free(state);
        return FIF_ERROR_COMPRESSOR_ERROR;
    }

    *out_compressor_data = state;
    return FIF_ERROR_SUCCESS; 
}

int zlib_compressor_write(fif_mount_handle mount, fif_inode_index_t inode_index, FIF_VOLUME_FORMAT_INODE *inode, void *compressor_data, unsigned int offset, const void *buffer, unsigned int bytes)
{
    struct zlib_state *state = (struct zlib_state *)compressor_data;

    // check the expected offset
    if (offset != state->stream.total_in)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "zlib_compressor_write: bad offset, got %u expected %u", offset, state->stream.total_in);
        return FIF_ERROR_COMPRESSOR_ERROR;
    }

    // shouldn't really be getting passed nothing..
    if (bytes == 0)
        return FIF_ERROR_SUCCESS;

    // pass to zlib
    assert(state->stream.avail_in == 0);
    state->stream.next_in = (Bytef *)buffer;
    state->stream.avail_in = bytes;

    // loop while writing data
    for (;;)
    {
        // deflate it
        int status = deflate(&state->stream, Z_NO_FLUSH);
        if (status == Z_STREAM_END)
        {
            assert(state->stream.avail_in == 0);
            return FIF_ERROR_SUCCESS;
        }

        // ok/buf error are nonfatal, it means the output buffer has to be flushed
        if (status != Z_OK && status != Z_BUF_ERROR)
        {
            // fatal error
            fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "zlib_compressor_write: deflate() returned %i", status);
            return FIF_ERROR_COMPRESSOR_ERROR;
        }
         
        // only flush the buffer if avail_out == 0
        if (state->stream.avail_out == 0)
        {
            unsigned int bytes_to_write = INTERNAL_ZLIB_BUFFER_SIZE - state->stream.avail_out;
            if (fif_write_file_data(mount, inode_index, inode, state->offset, state->compressed_data_buffer, bytes_to_write) != (int)bytes_to_write)
            {
                fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "zlib_compressor_write: failed to write buffered data");
                return FIF_ERROR_IO_ERROR;
            }

            // update buffer pointer, and try again
            state->stream.next_out = state->compressed_data_buffer;
            state->stream.avail_out = INTERNAL_ZLIB_BUFFER_SIZE;
            state->offset += bytes_to_write;
            continue;
        }
        else
        {
            // done
            break;
        }
    }
    
    // done the whole thing
    return bytes;
}

int zlib_compressor_end(fif_mount_handle mount, fif_inode_index_t inode_index, FIF_VOLUME_FORMAT_INODE *inode, void *compressor_data)
{
    struct zlib_state *state = (struct zlib_state *)compressor_data;
    assert(state->stream.avail_in == 0);

    // loop while writing data
    for (;;)
    {
        // end the deflate stream
        int status = deflate(&state->stream, Z_FINISH);

        // ok/buf error are nonfatal, it means the output buffer has to be flushed
        if (status != Z_OK && status != Z_STREAM_END)
        {
            // fatal error
            fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "zlib_compressor_end: deflate() returned %i", status);
            return FIF_ERROR_COMPRESSOR_ERROR;
        }

        // flush buffer
        unsigned int bytes_to_write = INTERNAL_ZLIB_BUFFER_SIZE - state->stream.avail_out;
        if (bytes_to_write > 0 && fif_write_file_data(mount, inode_index, inode, state->offset, state->compressed_data_buffer, bytes_to_write) != (int)bytes_to_write)
        {
            fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "zlib_compressor_end: failed to write buffered data");
            return FIF_ERROR_IO_ERROR;
        }

        // if we're set to Z_STREAM_END, it means we're done
        if (status == Z_STREAM_END)
            break;

        // update buffer pointer, and try again
        state->stream.next_out = state->compressed_data_buffer;
        state->stream.avail_out = INTERNAL_ZLIB_BUFFER_SIZE;
        state->offset += bytes_to_write;
    }

    // all done
    return FIF_ERROR_SUCCESS; 
}

int zlib_compressor_cleanup(fif_mount_handle mount, void *compressor_data)
{
    (void)mount;

    // cleanup the state
    struct zlib_state *state = (struct zlib_state *)compressor_data;
    if (state != NULL)
    {
        deflateEnd(&state->stream);
        free(state);
    }

    return FIF_ERROR_SUCCESS;
}

int zlib_decompressor_init(fif_mount_handle mount, fif_inode_index_t inode_index, FIF_VOLUME_FORMAT_INODE *inode, void **out_decompressor_data)
{ 
    (void)mount;
    (void)inode_index;
    (void)inode;

    // allocate state
    struct zlib_state *state = (struct zlib_state *)malloc(sizeof(struct zlib_state));
    if (state == NULL)
        return FIF_ERROR_OUT_OF_MEMORY;

    // set up initial inflate state
    memset(&state->stream, 0, sizeof(state->stream));
    state->offset = 0;
    state->transferred = 0;

    // initialize zlib
    if (inflateInit(&state->stream) != Z_OK)
    {
        free(state);
        return FIF_ERROR_COMPRESSOR_ERROR;
    }

    *out_decompressor_data = state;
    return FIF_ERROR_SUCCESS;
}

int zlib_decompressor_read(fif_mount_handle mount, fif_inode_index_t inode_index, FIF_VOLUME_FORMAT_INODE *inode, void *decompressor_data, unsigned int offset, void *buffer, unsigned int bytes)
{
    struct zlib_state *state = (struct zlib_state *)decompressor_data;
    //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "zlib_decompressor_read(%u, %u, %u) current transferred = %u", inode_index, offset, bytes, state->transferred);

    // check the expected offset
    if (offset != state->transferred)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "zlib_decompressor_read: bad offset, got %u expected %u", offset, state->transferred);
        return FIF_ERROR_COMPRESSOR_ERROR;
    }

    // setup output
    assert(state->stream.avail_out == 0);
    state->stream.next_out = (Bytef *)buffer;
    state->stream.avail_out = bytes;

    // loop while reading data
    for (;;)
    {
        int status = inflate(&state->stream, Z_NO_FLUSH);
        if (status == Z_STREAM_END)
            break;

        // handle fatal errors
        if (status != Z_OK && status != Z_BUF_ERROR)
        {
            fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "zlib_decompressor_read: inflate() returned %i", status);
            return FIF_ERROR_COMPRESSOR_ERROR;
        }

        // do we need to read more bytes from the input stream?
        if (state->stream.avail_in == 0)
        {
            unsigned int bytes_to_read = inode->data_size - state->offset;
            if (bytes_to_read > INTERNAL_ZLIB_BUFFER_SIZE)
                bytes_to_read = INTERNAL_ZLIB_BUFFER_SIZE;

            // end of stream?
            if (bytes_to_read == 0)
            {
                fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "zlib_decompressor_read: end of file reached, and zlib is still expecting data");
                return FIF_ERROR_COMPRESSOR_ERROR;
            }

            // read them from the volume
            int bytes_read = fif_read_file_data(mount, inode_index, inode, state->offset, state->compressed_data_buffer, bytes_to_read);
            if (bytes_read != (int)bytes_to_read)
            {
                fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "zlib_decompressor_read: failed to read data from volume, only got %i of %u bytes", bytes_read, bytes_to_read);
                return FIF_ERROR_IO_ERROR;
            }

            // add to zlib
            state->stream.next_in = (Bytef *)state->compressed_data_buffer;
            state->stream.avail_in = bytes_read;
            state->offset += bytes_to_read;
        }

        // if we're out of output bytes, wait until the next call in
        if (state->stream.avail_out == 0)
            break;
    }
    
    // reset output state
    unsigned int bytes_read = (bytes - state->stream.avail_out);
    state->stream.next_out = NULL;
    state->stream.avail_out = 0;
    state->transferred += bytes_read;
    return bytes_read;
}

int zlib_decompressor_skip(fif_mount_handle mount, fif_inode_index_t inode_index, FIF_VOLUME_FORMAT_INODE *inode, void *decompressor_data, unsigned int count)
{
    struct zlib_state *state = (struct zlib_state *)decompressor_data;
    //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "zlib_decompressor_skip(%u, %u) current transferred = %u", inode_index, count, state->transferred);

    // skip is implemented as "read skip bytes and discard them"
    char skip_buffer[512];
    unsigned int remaining = count;
    while (remaining > 0)
    {
        unsigned int pass_count = (remaining > sizeof(skip_buffer)) ? sizeof(skip_buffer) : remaining;
        //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "  calling zlib_decompressor_read(%u, %u, %u)", inode_index, state->transferred, pass_count);

        int result;
        if ((result = zlib_decompressor_read(mount, inode_index, inode, decompressor_data, state->transferred, skip_buffer, pass_count)) != (int)pass_count)
            return (result >= 0) ? FIF_ERROR_COMPRESSOR_ERROR : result;

        remaining -= pass_count;
    }

    return count;
}

int zlib_decompressor_cleanup(fif_mount_handle mount, void *decompressor_data)
{
    (void)mount;

    // cleanup the state
    struct zlib_state *state = (struct zlib_state *)decompressor_data;
    if (state != NULL)
    {
        inflateEnd(&state->stream);
        free(state);
    }

    return FIF_ERROR_SUCCESS;
}

static const struct fif_compressor_functions zlib_compressor_functions =
{
    zlib_compressor_init,
    zlib_compressor_write,
    zlib_compressor_end,
    zlib_compressor_cleanup
};

static const struct fif_decompressor_functions zlib_decompressor_functions =
{
    zlib_decompressor_init,
    zlib_decompressor_read,
    zlib_decompressor_skip,
    zlib_decompressor_cleanup
};

const struct fif_compressor_functions *fif_zlib_compressor_functions()
{
    return &zlib_compressor_functions;
}

const struct fif_decompressor_functions *fif_zlib_decompressor_functions()
{
    return &zlib_decompressor_functions;
}

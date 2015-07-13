#include "libfif/fif_io.h"
#include <stdlib.h>
#include <string.h>

struct io_memory_state
{
    unsigned char *buffer;
    size_t buffer_reserve;
    size_t buffer_position;
    size_t buffer_size;
};

static int io_memory_resize(struct io_memory_state *state, size_t new_size)
{
    if (new_size > state->buffer_reserve)
    {
        size_t new_reserve = state->buffer_reserve * 2;
        if (new_size > new_reserve)
            new_size = new_reserve;

        unsigned char *new_buffer = (unsigned char *)realloc(state->buffer, new_reserve);
        if (new_buffer == NULL)
            return FIF_ERROR_OUT_OF_MEMORY;

        state->buffer = new_buffer;
        state->buffer_reserve = new_reserve;
    }

    state->buffer_size = new_size;
    return FIF_ERROR_SUCCESS;
}

static int io_memory_read(fif_io_userdata userdata, void *buffer, unsigned int count)
{
    struct io_memory_state *state = (struct io_memory_state *)userdata;

    size_t remaining = state->buffer_size - state->buffer_position;
    size_t copylen = (remaining < (size_t)count) ? remaining : (size_t)count;
    if (copylen > 0)
    {
        memcpy(buffer, state->buffer + state->buffer_position, copylen);
        state->buffer_position += copylen;
    }

    return (int)copylen;
}

static int io_memory_write(fif_io_userdata userdata, const void *buffer, unsigned int count)
{
    struct io_memory_state *state = (struct io_memory_state *)userdata;

    size_t new_position = state->buffer_position + (size_t)count;
    if (new_position > state->buffer_size)
    {
        int result;
        if ((result = io_memory_resize(state, new_position)) != FIF_ERROR_SUCCESS)
            return result;
    }

    memcpy(state->buffer + state->buffer_position, buffer, count);
    state->buffer_position += count;
    return (int)count;
}

static int64_t io_memory_seek(fif_io_userdata userdata, fif_offset_t offset, enum FIF_SEEK_MODE mode)
{
    struct io_memory_state *state = (struct io_memory_state *)userdata;

    size_t new_offset;
    if (mode == FIF_SEEK_MODE_SET)
        new_offset = (size_t)offset;
    else if (mode == FIF_SEEK_MODE_CUR)
        new_offset = (size_t)((fif_offset_t)state->buffer_position + offset);
    else if (mode == FIF_SEEK_MODE_END)
        new_offset = state->buffer_size;
    else
        return FIF_ERROR_IO_ERROR;

    if (new_offset > state->buffer_size)
        return FIF_ERROR_BAD_OFFSET;

    state->buffer_position = new_offset;
    return FIF_ERROR_SUCCESS;
}

static int io_memory_zero(fif_io_userdata userdata, fif_offset_t offset, unsigned int count)
{
    struct io_memory_state *state = (struct io_memory_state *)userdata;

    size_t check_size = (size_t)(offset + (fif_offset_t)count);
    if (check_size > state->buffer_size)
    {
        int result;
        if ((result = io_memory_resize(state, check_size)) != FIF_ERROR_SUCCESS)
            return result;
    }

    memset(state->buffer + (size_t)offset, 0, count);
    return (int)count;
}

static int io_memory_ftruncate(fif_io_userdata userdata, fif_offset_t newsize)
{
    struct io_memory_state *state = (struct io_memory_state *)userdata;
    
    size_t old_size = state->buffer_size;
    size_t new_size = (size_t)newsize;
    if (new_size > old_size)
    {
        int result;
        if ((result = io_memory_resize(state, new_size)) != FIF_ERROR_SUCCESS)
            return result;

        memset(state->buffer + old_size, 0, new_size - old_size);
    }

    state->buffer_size = new_size;
    return FIF_ERROR_SUCCESS;
}

static int64_t io_memory_filesize(fif_io_userdata userdata)
{
    struct io_memory_state *state = (struct io_memory_state *)userdata;
    return (int64_t)state->buffer_size;
}

int fif_io_open_memory(fif_io *out_io)
{
    struct io_memory_state *state = (struct io_memory_state *)malloc(sizeof(struct io_memory_state));
    state->buffer = NULL;
    state->buffer_reserve = 0;
    state->buffer_position = 0;
    state->buffer_size = 0;

    out_io->io_read = io_memory_read;
    out_io->io_write = io_memory_write;
    out_io->io_seek = io_memory_seek;
    out_io->io_zero = io_memory_zero;
    out_io->io_ftruncate = io_memory_ftruncate;
    out_io->io_filesize = io_memory_filesize;
    out_io->userdata = (fif_io_userdata)state;

    return FIF_ERROR_SUCCESS;
}

int fif_io_close_memory(const fif_io *io)
{
    struct io_memory_state *state = (struct io_memory_state *)io->userdata;
    free(state->buffer);
    free(state);

    return FIF_ERROR_SUCCESS;
}

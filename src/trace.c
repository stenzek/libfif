#include "fif_internal.h"
#include "trace.h"
#include "trace_stream.h"

int fif_trace_write_stat(fif_mount_handle mount, const char *path)
{
    int result;

    if ((result = trace_stream_write_byte(mount->trace_stream, FIF_TRACE_COMMAND_STAT)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_string(mount->trace_stream, path)) != FIF_ERROR_SUCCESS)
    {
        return result;
    }

    return FIF_ERROR_SUCCESS;
}

int fif_trace_write_fstat(fif_mount_handle mount, fif_file_handle file)
{
    int result;

    if ((result = trace_stream_write_byte(mount->trace_stream, FIF_TRACE_COMMAND_FSTAT)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_uint(mount->trace_stream, file->handle_index)) != FIF_ERROR_SUCCESS)
    {
        return result;
    }

    return FIF_ERROR_SUCCESS;
}


int fif_trace_write_open(fif_mount_handle mount, const char *path, unsigned int mode)
{
    int result;

    if ((result = trace_stream_write_byte(mount->trace_stream, FIF_TRACE_COMMAND_OPEN)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_string(mount->trace_stream, path)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_uint(mount->trace_stream, mode)) != FIF_ERROR_SUCCESS)
    {
        return result;
    }

    return FIF_ERROR_SUCCESS;
}

int fif_trace_write_read(fif_mount_handle mount, fif_file_handle file, unsigned int count)
{
    int result;

    if ((result = trace_stream_write_byte(mount->trace_stream, FIF_TRACE_COMMAND_READ)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_uint(mount->trace_stream, file->handle_index)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_uint(mount->trace_stream, count)) != FIF_ERROR_SUCCESS)
    {
        return result;
    }

    return FIF_ERROR_SUCCESS;
}


int fif_trace_write_write(fif_mount_handle mount, fif_file_handle file, const void *in_buffer, unsigned int count)
{
    int result;

    if ((result = trace_stream_write_byte(mount->trace_stream, FIF_TRACE_COMMAND_WRITE)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_uint(mount->trace_stream, file->handle_index)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_uint(mount->trace_stream, count)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_bytes(mount->trace_stream, in_buffer, count)) != (int)count)
    {
        return result;
    }

    return FIF_ERROR_SUCCESS;
}


int fif_trace_write_seek(fif_mount_handle mount, fif_file_handle file, fif_offset_t offset, enum FIF_SEEK_MODE mode)
{
    int result;

    if ((result = trace_stream_write_byte(mount->trace_stream, FIF_TRACE_COMMAND_SEEK)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_uint(mount->trace_stream, file->handle_index)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_long(mount->trace_stream, offset)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_byte(mount->trace_stream, (unsigned char)mode)) != FIF_ERROR_SUCCESS)
    {
        return result;
    }

    return FIF_ERROR_SUCCESS;
}


int fif_trace_write_tell(fif_mount_handle mount, fif_file_handle file)
{
    int result;

    if ((result = trace_stream_write_byte(mount->trace_stream, FIF_TRACE_COMMAND_TELL)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_uint(mount->trace_stream, file->handle_index)) != FIF_ERROR_SUCCESS)
    {
        return result;
    }

    return FIF_ERROR_SUCCESS;
}


int fif_trace_write_ftruncate(fif_mount_handle mount, fif_file_handle file, fif_offset_t size)
{
    int result;

    if ((result = trace_stream_write_byte(mount->trace_stream, FIF_TRACE_COMMAND_FTRUNCATE)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_uint(mount->trace_stream, file->handle_index)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_long(mount->trace_stream, size)) != FIF_ERROR_SUCCESS)
    {
        return result;
    }

    return FIF_ERROR_SUCCESS;
}


int fif_trace_write_close(fif_mount_handle mount, fif_file_handle file)
{
    int result;

    if ((result = trace_stream_write_byte(mount->trace_stream, FIF_TRACE_COMMAND_CLOSE)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_uint(mount->trace_stream, file->handle_index)) != FIF_ERROR_SUCCESS)
    {
        return result;
    }

    return FIF_ERROR_SUCCESS;
}


int fif_trace_write_unlink(fif_mount_handle mount, const char *filename)
{
    int result;

    if ((result = trace_stream_write_byte(mount->trace_stream, FIF_TRACE_COMMAND_UNLINK)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_string(mount->trace_stream, filename)) != FIF_ERROR_SUCCESS)
    {
        return result;
    }

    return FIF_ERROR_SUCCESS;
}

int fif_trace_write_get_file_contents(fif_mount_handle mount, const char *filename, unsigned int max_count)
{
    int result;

    if ((result = trace_stream_write_byte(mount->trace_stream, FIF_TRACE_COMMAND_GET_FILE_CONTENTS)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_string(mount->trace_stream, filename)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_uint(mount->trace_stream, max_count)) != FIF_ERROR_SUCCESS)
    {
        return result;
    }

    return FIF_ERROR_SUCCESS;
}


int fif_trace_write_put_file_contents(fif_mount_handle mount, const char *filename, const void *buffer, unsigned int count)
{
    int result;

    if ((result = trace_stream_write_byte(mount->trace_stream, FIF_TRACE_COMMAND_PUT_FILE_CONTENTS)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_string(mount->trace_stream, filename)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_uint(mount->trace_stream, count)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_bytes(mount->trace_stream, buffer, count)) != (int)count)
    {
        return result;
    }

    return FIF_ERROR_SUCCESS;
}


int fif_trace_write_compress_file(fif_mount_handle mount, const char *filename, enum FIF_COMPRESSION_ALGORITHM new_compression_algorithm, unsigned int new_compression_level)
{
    int result;

    if ((result = trace_stream_write_byte(mount->trace_stream, FIF_TRACE_COMMAND_COMPRESS_FILE)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_string(mount->trace_stream, filename)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_byte(mount->trace_stream, (unsigned char)new_compression_algorithm)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_byte(mount->trace_stream, (unsigned char)new_compression_level)) != FIF_ERROR_SUCCESS)
    {
        return result;
    }

    return FIF_ERROR_SUCCESS;
}

int fif_trace_write_enumdir(fif_mount_handle mount, const char *dirname)
{
    int result;

    if ((result = trace_stream_write_byte(mount->trace_stream, FIF_TRACE_COMMAND_ENUMDIR)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_string(mount->trace_stream, dirname)) != FIF_ERROR_SUCCESS)
    {
        return result;
    }

    return FIF_ERROR_SUCCESS;
}

int fif_trace_write_mkdir(fif_mount_handle mount, const char *dirname)
{
    int result;

    if ((result = trace_stream_write_byte(mount->trace_stream, FIF_TRACE_COMMAND_MKDIR)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_string(mount->trace_stream, dirname)) != FIF_ERROR_SUCCESS)
    {
        return result;
    }

    return FIF_ERROR_SUCCESS;
}

int fif_trace_write_rmdir(fif_mount_handle mount, const char *dirname)
{
    int result;

    if ((result = trace_stream_write_byte(mount->trace_stream, FIF_TRACE_COMMAND_RMDIR)) != FIF_ERROR_SUCCESS ||
        (result = trace_stream_write_string(mount->trace_stream, dirname)) != FIF_ERROR_SUCCESS)
    {
        return result;
    }

    return FIF_ERROR_SUCCESS;
}

int fif_trace_create_volume(fif_mount_handle *out_mount_handle, const fif_io *io, fif_log_callback log_callback, const fif_volume_options *archive_options, const fif_mount_options *mount_options, const fif_io *trace_io)
{
    int result;
    fif_mount_handle mount;

    // create the volume
    if ((result = fif_create_volume(&mount, io, log_callback, archive_options, mount_options)) != FIF_ERROR_SUCCESS)
        return result;

    // initialize the trace
    struct fif_trace_stream *trace_stream;
    if ((result = trace_stream_writer_init(&trace_stream, trace_io)) != FIF_ERROR_SUCCESS)
    {
        fif_unmount_volume(mount);
        return result;
    }

    // set pointer
    mount->trace_stream = trace_stream;
    *out_mount_handle = mount;
    return FIF_ERROR_SUCCESS;
}

int fif_trace_mount_volume(fif_mount_handle *out_mount_handle, const fif_io *io, fif_log_callback log_callback, const fif_mount_options *mount_options, const fif_io *trace_io)
{
    int result;
    fif_mount_handle mount;

    // create the volume
    if ((result = fif_mount_volume(&mount, io, log_callback, mount_options)) != FIF_ERROR_SUCCESS)
        return result;

    // initialize the trace
    struct fif_trace_stream *trace_stream;
    if ((result = trace_stream_writer_init(&trace_stream, trace_io)) != FIF_ERROR_SUCCESS)
    {
        fif_unmount_volume(mount);
        return result;
    }

    // set pointer
    mount->trace_stream = trace_stream;
    *out_mount_handle = mount;
    return FIF_ERROR_SUCCESS;
}

static int trace_dummy_enumdir_callback(void *userdata, const char *filename)
{
    (void)userdata;
    (void)filename;
    return FIF_ERROR_SUCCESS;
}

static int trace_replay_command(fif_mount_handle mount, struct fif_trace_stream *trace_stream, enum FIF_TRACE_COMMAND command)
{
    int result;

    switch (command)
    {
    case FIF_TRACE_COMMAND_STAT:
        {
            char *filename;
            if ((result = trace_stream_read_string(trace_stream, &filename)) != FIF_ERROR_SUCCESS)
                return result;

            fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "[trace replay] fif_stat(filename: %s)", filename);

            fif_fileinfo fileinfo;
            fif_stat(mount, filename, &fileinfo);
            free(filename);
            return FIF_ERROR_SUCCESS;
        }
        break;

    case FIF_TRACE_COMMAND_FSTAT:
        {
            unsigned int handle_index;
            if ((result = trace_stream_read_uint(trace_stream, &handle_index)) != FIF_ERROR_SUCCESS)
                return result;

            if (handle_index >= mount->open_file_count || mount->open_files[handle_index] == NULL)
                return FIF_ERROR_GENERIC_ERROR;

            fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "[trace replay] fif_fstat(handle: %u)", handle_index);

            fif_fileinfo fileinfo;
            fif_fstat(mount, mount->open_files[handle_index], &fileinfo);
            return FIF_ERROR_SUCCESS;
        }
        break;

    case FIF_TRACE_COMMAND_OPEN:
        {
            char *filename;
            unsigned int mode;
            if ((result = trace_stream_read_string(trace_stream, &filename)) != FIF_ERROR_SUCCESS ||
                (result = trace_stream_read_uint(trace_stream, &mode)) != FIF_ERROR_SUCCESS)
            {
                return result;
            }

            fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "[trace replay] fif_open(filename: %s, mode: %x)", filename, mode);

            fif_file_handle file;
            fif_open(mount, filename, mode, &file);
            free(filename);
            return FIF_ERROR_SUCCESS;
        }
        break;

    case FIF_TRACE_COMMAND_READ:
        {
            unsigned int handle_index;
            unsigned int count;
            if ((result = trace_stream_read_uint(trace_stream, &handle_index)) != FIF_ERROR_SUCCESS ||
                (result = trace_stream_read_uint(trace_stream, &count)) != FIF_ERROR_SUCCESS)
            {
                return result;
            }

            if (handle_index >= mount->open_file_count || mount->open_files[handle_index] == NULL)
                return FIF_ERROR_GENERIC_ERROR;

            //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "[trace replay] fif_read(handle: %u, count: %u)", handle_index, count);

            void *buffer = malloc(count);
            fif_read(mount, mount->open_files[handle_index], buffer, count);
            free(buffer);
            return FIF_ERROR_SUCCESS;
        }
        break;

    case FIF_TRACE_COMMAND_WRITE:
        {
            unsigned int handle_index;
            unsigned int count;
            if ((result = trace_stream_read_uint(trace_stream, &handle_index)) != FIF_ERROR_SUCCESS ||
                (result = trace_stream_read_uint(trace_stream, &count)) != FIF_ERROR_SUCCESS)
            {
                return result;
            }

            if (handle_index >= mount->open_file_count || mount->open_files[handle_index] == NULL)
                return FIF_ERROR_GENERIC_ERROR;

            void *buffer = malloc(count);
            if (buffer == NULL)
                return FIF_ERROR_OUT_OF_MEMORY;
            if ((result = trace_stream_read_bytes(trace_stream, buffer, count)) != (int)count)
                return (result >= 0) ? FIF_ERROR_END_OF_FILE : result;

            //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "[trace replay] fif_write(handle: %u, count: %u)", handle_index, count);

            fif_write(mount, mount->open_files[handle_index], buffer, count);
            free(buffer);
            return FIF_ERROR_SUCCESS;
        }
        break;

    case FIF_TRACE_COMMAND_SEEK:
        {
            unsigned int handle_index;
            fif_offset_t offset;
            unsigned char mode;
            if ((result = trace_stream_read_uint(trace_stream, &handle_index)) != FIF_ERROR_SUCCESS ||
                (result = trace_stream_read_long(trace_stream, &offset)) != FIF_ERROR_SUCCESS ||
                (result = trace_stream_read_byte(trace_stream, &mode)) != FIF_ERROR_SUCCESS)
            {
                return result;
            }

            if (handle_index >= mount->open_file_count || mount->open_files[handle_index] == NULL)
                return FIF_ERROR_GENERIC_ERROR;

            fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "[trace replay] fif_seek(handle: %u, offset: %i, mode: %u)", handle_index, offset, (unsigned int)mode);

            fif_seek(mount, mount->open_files[handle_index], offset, (enum FIF_SEEK_MODE)mode);
            return FIF_ERROR_SUCCESS;
        }
        break;

    case FIF_TRACE_COMMAND_TELL:
        {
            unsigned int handle_index;
            if ((result = trace_stream_read_uint(trace_stream, &handle_index)) != FIF_ERROR_SUCCESS)
                return result;

            if (handle_index >= mount->open_file_count || mount->open_files[handle_index] == NULL)
                return FIF_ERROR_GENERIC_ERROR;

            fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "[trace replay] fif_tell(handle: %u)", handle_index);

            fif_tell(mount, mount->open_files[handle_index]);
            return FIF_ERROR_SUCCESS;
        }
        break;

    case FIF_TRACE_COMMAND_FTRUNCATE:
        {
            unsigned int handle_index;
            fif_offset_t new_size;
            if ((result = trace_stream_read_uint(trace_stream, &handle_index)) != FIF_ERROR_SUCCESS ||
                (result = trace_stream_read_long(trace_stream, &new_size)) != FIF_ERROR_SUCCESS)
            {
                return result;
            }

            if (handle_index >= mount->open_file_count || mount->open_files[handle_index] == NULL)
                return FIF_ERROR_GENERIC_ERROR;

            fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "[trace replay] fif_ftruncate(handle: %u, new_size: %u)", handle_index, new_size);

            fif_ftruncate(mount, mount->open_files[handle_index], new_size);
            return FIF_ERROR_SUCCESS;
        }
        break;

    case FIF_TRACE_COMMAND_CLOSE:
        {
            unsigned int handle_index;
            if ((result = trace_stream_read_uint(trace_stream, &handle_index)) != FIF_ERROR_SUCCESS)
                return result;

            if (handle_index >= mount->open_file_count || mount->open_files[handle_index] == NULL)
                return FIF_ERROR_GENERIC_ERROR;

            fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "[trace replay] fif_close(handle: %u)", handle_index);

            fif_close(mount, mount->open_files[handle_index]);
            return FIF_ERROR_SUCCESS;
        }
        break;

    case FIF_TRACE_COMMAND_UNLINK:
        {
            char *filename;
            if ((result = trace_stream_read_string(trace_stream, &filename)) != FIF_ERROR_SUCCESS)
                return result;

            fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "[trace replay] fif_unlink(filename: %s)", filename);

            fif_unlink(mount, filename);
            return FIF_ERROR_SUCCESS;
        }
        break;

    case FIF_TRACE_COMMAND_GET_FILE_CONTENTS:
        {
            char *filename;
            unsigned int maxcount;
            if ((result = trace_stream_read_string(trace_stream, &filename)) != FIF_ERROR_SUCCESS ||
                (result = trace_stream_read_uint(trace_stream, &maxcount)) != FIF_ERROR_SUCCESS)
            {
                return result;
            }

            void *buffer = malloc(maxcount);
            if (buffer == NULL)
                return FIF_ERROR_OUT_OF_MEMORY;

            fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "[trace replay] fif_get_file_contents(filename: %s, maxcount: %u)", filename, maxcount);
            fif_get_file_contents(mount, filename, buffer, maxcount);
            free(buffer);
            return FIF_ERROR_SUCCESS;
        }
        break;

    case FIF_TRACE_COMMAND_PUT_FILE_CONTENTS:
        {
            char *filename;
            unsigned int count;
            if ((result = trace_stream_read_string(trace_stream, &filename)) != FIF_ERROR_SUCCESS ||
                (result = trace_stream_read_uint(trace_stream, &count)) != FIF_ERROR_SUCCESS)
            {
                return result;
            }

            void *buffer = malloc(count);
            if (buffer == NULL)
            {
                free(filename);
                return FIF_ERROR_OUT_OF_MEMORY;
            }
            if ((result = trace_stream_read_bytes(trace_stream, buffer, count)) != (int)count)
            {
                free(buffer);
                free(filename);
                return result;
            }

            fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "[trace replay] fif_put_file_contents(filename: %s, count: %u)", filename, count);

            fif_put_file_contents(mount, filename, buffer, count);
            free(buffer);
            free(filename);
            return FIF_ERROR_SUCCESS;
        }
        break;

    case FIF_TRACE_COMMAND_ENUMDIR:
        {
            char *filename;
            if ((result = trace_stream_read_string(trace_stream, &filename)) != FIF_ERROR_SUCCESS)
                return result;

            fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "[trace replay] fif_enumdir(dirname: %s)", filename);

            fif_enumdir(mount, filename, trace_dummy_enumdir_callback, NULL);
            free(filename);
            return FIF_ERROR_SUCCESS;
        }
        break;

    case FIF_TRACE_COMMAND_MKDIR:
        {
            char *filename;
            if ((result = trace_stream_read_string(trace_stream, &filename)) != FIF_ERROR_SUCCESS)
                return result;

            fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "[trace replay] fif_mkdir(dirname: %s)", filename);

            fif_mkdir(mount, filename);
            free(filename);
            return FIF_ERROR_SUCCESS;
        }
        break;

    case FIF_TRACE_COMMAND_RMDIR:
        {
            char *filename;
            if ((result = trace_stream_read_string(trace_stream, &filename)) != FIF_ERROR_SUCCESS)
                return result;

            fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "[trace replay] fif_rmdir(dirname: %s)", filename);

            fif_rmdir(mount, filename);
            free(filename);
            return FIF_ERROR_SUCCESS;
        }
        break;
    }

    return FIF_ERROR_GENERIC_ERROR;
}

int fif_trace_replay(fif_mount_handle mount, const fif_io *tracefile_io)
{
    int result;

    struct fif_trace_stream *trace_stream;
    if ((result = trace_stream_reader_init(&trace_stream, tracefile_io)) != FIF_ERROR_SUCCESS)
        return result;

    for (;;)
    {
        unsigned char command;
        if ((result = trace_stream_read_byte(trace_stream, &command)) != FIF_ERROR_SUCCESS)
        {
            // allow eof
            if (result == FIF_ERROR_END_OF_FILE)
                break;

            // everything else fails
            trace_stream_reader_finish(trace_stream);
            return result;
        }

        if ((result = trace_replay_command(mount, trace_stream, (enum FIF_TRACE_COMMAND)command)) != FIF_ERROR_SUCCESS)
            return result;
    }

    trace_stream_reader_finish(trace_stream);
    return FIF_ERROR_SUCCESS;
}

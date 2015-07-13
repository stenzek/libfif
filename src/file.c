#include "fif_internal.h"
#include "trace.h"

int fif_create_file(fif_mount_handle mount, const char *filename, fif_inode_index_t directory_inode, fif_inode_index_t *out_inode_index)
{
    int result;
    fif_inode_index_t file_inode_index;

    // allocate a new inode, near the directory
    FIF_VOLUME_FORMAT_INODE inode;
    memset(&inode, 0, sizeof(inode));
    inode.attributes = FIF_FILE_ATTRIBUTE_FILE;
    inode.creation_timestamp = fif_current_timestamp();
    inode.reference_count = 1;
    inode.compression_algorithm = mount->new_file_compression_algorithm;
    inode.compression_level = mount->new_file_compression_level;
    if (inode.compression_algorithm != FIF_COMPRESSION_ALGORITHM_NONE)
        inode.attributes |= FIF_FILE_ATTRIBUTE_COMPRESSED;

    // allocate and write
    if ((result = fif_alloc_inode(mount, directory_inode, &file_inode_index)) != FIF_ERROR_SUCCESS ||
        (result = fif_write_inode(mount, file_inode_index, &inode)) != FIF_ERROR_SUCCESS)
    {
        return result;
    }

    // add the file to the directory
    if ((result = fif_add_file_to_directory(mount, directory_inode, filename, file_inode_index)) != FIF_ERROR_SUCCESS)
    {
        // release the inode
        fif_free_inode(mount, file_inode_index);
        return result;
    }

    // done
    *out_inode_index = file_inode_index;
    return result;
}

int fif_resize_file(fif_mount_handle mount, fif_inode_index_t inode_index, FIF_VOLUME_FORMAT_INODE *inode, unsigned int new_size)
{
    int result;
    //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "fif_resize_file(%p, %u, %u -> %u)", mount, inode_index, inode->data_size, new_size);

    // calculate required blocks for file
    unsigned int required_blocks = new_size / mount->block_size;
    if ((new_size % mount->block_size) != 0)
        required_blocks++;

    // is this more than the current number of blocks?
    if (required_blocks != inode->block_count)
    {
        // does the file currently have any blocks allocated? if not, we need to allocate, not resize
        if (inode->data_size == 0)
        {
            // allocate blocks
            assert(inode->first_block_index == 0);
            if ((result = fif_volume_alloc_blocks(mount, 0, required_blocks, &inode->first_block_index)) != FIF_ERROR_SUCCESS)
                return result;
        }
        else
        {
            // have to extend the block range of the file
            assert(inode->first_block_index != 0 && inode->block_count > 0);
            if ((result = fif_volume_resize_block_range(mount, inode->first_block_index, inode->block_count, required_blocks, &inode->first_block_index)) != FIF_ERROR_SUCCESS)
                return result;
        }

        // update block count
        inode->block_count = required_blocks;

        // rewrite the inode, since the range may have changed
        if ((result = fif_write_inode(mount, inode_index, inode)) != FIF_ERROR_SUCCESS)
            return result;
    }

    // update file size
    inode->data_size = new_size;
    return FIF_ERROR_SUCCESS;
}

int fif_read_file_data(fif_mount_handle mount, fif_inode_index_t inode_index, const FIF_VOLUME_FORMAT_INODE *inode, unsigned int offset, void *buffer, unsigned int bytes)
{
    (void)inode_index;

    // todo: fragmentation, etc
    int result;
    //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "fif_read_file_data(%p, %u, %u, %u)", mount, inode_index, offset, bytes);

    // is the offset/bytes invalid?
    if ((offset + bytes) > inode->data_size)
        return FIF_ERROR_BAD_OFFSET;

    // loop while reading blocks
    unsigned char *buffer_ptr = (unsigned char*)buffer;
    fif_block_index_t current_block = offset / mount->block_size;
    unsigned int block_offset = offset % mount->block_size;
    unsigned int remaining_bytes = bytes;
    while (remaining_bytes > 0)
    {
        // find out remaining blocks in this block
        unsigned int bytes_from_this_block = mount->block_size - block_offset;
        if (bytes_from_this_block > remaining_bytes)
            bytes_from_this_block = remaining_bytes;

        // range check
        if (current_block >= inode->block_count)
            return bytes - remaining_bytes;

        // read the block
        if ((result = fif_volume_read_block(mount, inode->first_block_index + current_block, block_offset, buffer_ptr, bytes_from_this_block)) != FIF_ERROR_SUCCESS)
            return bytes - remaining_bytes;

        // move to next block
        remaining_bytes -= bytes_from_this_block;
        buffer_ptr += bytes_from_this_block;
        current_block++;
        block_offset = 0;
    }

    // done
    return bytes;
}

int fif_write_file_data(fif_mount_handle mount, fif_inode_index_t inode_index, FIF_VOLUME_FORMAT_INODE *inode, unsigned int offset, const void *buffer, unsigned int bytes)
{
    int result;
    //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "fif_write_file_data(%u, %u, %u)", inode_index, offset, bytes);

    // file has to be resized?
    if ((offset + bytes) > inode->data_size)
    {
        // resize the file
        unsigned int new_size = offset + bytes;
        if ((result = fif_resize_file(mount, inode_index, inode, new_size)) != FIF_ERROR_SUCCESS)
            return result;

        // update the inode
        inode->data_size = new_size;
        if ((result = fif_write_inode(mount, inode_index, inode)) != FIF_ERROR_SUCCESS)
            return result;
    }

    // loop while writing blocks
    unsigned char *buffer_ptr = (unsigned char*)buffer;
    fif_block_index_t current_block = (offset / mount->block_size);
    unsigned int block_offset = offset % mount->block_size;
    unsigned int remaining_bytes = bytes;
    while (remaining_bytes > 0)
    {
        // find out remaining blocks in this block
        unsigned int bytes_from_this_block = mount->block_size - block_offset;
        if (bytes_from_this_block > remaining_bytes)
            bytes_from_this_block = remaining_bytes;

        // check block range
        if (current_block >= inode->block_count)
            return bytes - remaining_bytes;

        // read the block
        if ((result = fif_volume_write_block(mount, inode->first_block_index + current_block, block_offset, buffer_ptr, bytes_from_this_block)) != FIF_ERROR_SUCCESS)
            return bytes - remaining_bytes;

        // move to next block
        remaining_bytes -= bytes_from_this_block;
        buffer_ptr += bytes_from_this_block;
        current_block++;
        block_offset = 0;
    }

    // wrote everything
    return bytes;
}

int fif_free_file_blocks(fif_mount_handle mount, fif_inode_index_t inode_index, FIF_VOLUME_FORMAT_INODE *inode)
{
    int result;
    if (inode->block_count > 0)
    {
        if ((result = fif_volume_free_blocks(mount, inode->first_block_index, inode->block_count)) != FIF_ERROR_SUCCESS)
            return result;
    }

    inode->uncompressed_size = 0;
    inode->data_size = 0;
    inode->checksum = 0;
    inode->first_block_index = 0;
    inode->block_count = 0;
    return fif_write_inode(mount, inode_index, inode);
}

int fif_resolve_file_name(fif_mount_handle mount, const char *path, fif_inode_index_t *out_inode_index, fif_inode_index_t *out_directory_inode_index)
{
    int result;

    // copy the path, canonicalize it
    int path_length = (int)strlen(path);
    char *path_copy = (char *)alloca(path_length + 1);
    if (!fif_canonicalize_path(path_copy, path_length + 1, path))
        return FIF_ERROR_BAD_PATH;

    // split to dirname and basename
    char *dirname, *basename;
    fif_split_path_dirbase(path_copy, &dirname, &basename);

    // resolve the file's containing directory
    fif_inode_index_t directory_inode;
    if (dirname == NULL)
        directory_inode = mount->root_inode;
    else if ((result = fif_resolve_directory_name(mount, dirname, &directory_inode)) != FIF_ERROR_SUCCESS)
        return result;

    // does the file exist in the directory?
    fif_inode_index_t file_inode_index;
    if ((result = fif_find_file_in_directory(mount, directory_inode, basename, &file_inode_index, NULL)) != FIF_ERROR_SUCCESS)
        return result;

    // return the inode
    if (out_inode_index != NULL)
        *out_inode_index = file_inode_index;
    if (out_directory_inode_index != NULL)
        *out_directory_inode_index = directory_inode;

    return FIF_ERROR_SUCCESS;
}

static int can_open_file(fif_mount_handle mount, fif_inode_index_t inode, unsigned int mode)
{
    if (mode & (FIF_OPEN_MODE_WRITE | FIF_OPEN_MODE_TRUNCATE))
    {
        if (mount->read_only)
            return FIF_ERROR_READ_ONLY;

        // check there isn't any open files for reading or writing with this inode
        for (unsigned int i = 0; i < mount->open_file_count; i++)
        {
            fif_file_handle file = mount->open_files[i];
            if (file != NULL && file->inode_index == inode && (file->open_mode & (FIF_OPEN_MODE_READ |FIF_OPEN_MODE_WRITE)))
                return FIF_ERROR_SHARING_VIOLATION;
        }

        return FIF_ERROR_SUCCESS;
    }
    else
    {
        // check there isn't any open files for writing with this inode
        for (unsigned int i = 0; i < mount->open_file_count; i++)
        {
            fif_file_handle file = mount->open_files[i];
            if (file != NULL && file->inode_index == inode && (file->open_mode & FIF_OPEN_MODE_WRITE))
                return FIF_ERROR_SHARING_VIOLATION;
        }

        return FIF_ERROR_SUCCESS;
    }
}

static int resize_open_file_buffer(fif_file_handle handle, unsigned int new_size)
{
    unsigned char *new_buffer = (unsigned char *)realloc(handle->buffer_data, new_size);
    if (new_buffer == NULL)
        return FIF_ERROR_OUT_OF_MEMORY;

    handle->buffer_data = new_buffer;
    handle->buffer_size = new_size;
    return FIF_ERROR_SUCCESS;
}

static int update_file_buffer(fif_mount_handle mount, fif_file_handle handle, unsigned int new_start)
{
    int result;
    assert(!(handle->open_mode & FIF_OPEN_MODE_FULLY_BUFFERED));
    assert(handle->buffer_range_size <= handle->buffer_size);
    //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "update_file_buffer(%u, %u) :: %u,%u %u", handle->inode_index, new_start, handle->buffer_range_start, handle->buffer_range_size, handle->current_offset);

    // if the buffer was changed
    if ((handle->open_mode & FIF_OPEN_MODE_WRITE) && handle->buffer_dirty)
    {
        // write it
        if (handle->compressor != NULL)
            result = handle->compressor->compressor_write(mount, handle->inode_index, &handle->inode, handle->compressor_data, handle->buffer_range_start, handle->buffer_data, handle->buffer_range_size);
        else
            result = fif_write_file_data(mount, handle->inode_index, &handle->inode, handle->buffer_range_start, handle->buffer_data, handle->buffer_range_size);

        if ((unsigned int)result != handle->buffer_range_size)
            return (result >= 0) ? FIF_ERROR_IO_ERROR : result;
    }

    // update the buffer starting position
    unsigned int old_buffer_range_start = handle->buffer_range_start;
    unsigned int old_buffer_range_size = handle->buffer_range_size;
    handle->buffer_range_start = new_start;
    handle->buffer_range_size = 0;

    // update the buffer contents
    if ((handle->open_mode & FIF_OPEN_MODE_READ) && new_start < handle->file_size)
    {
        unsigned int remaining_bytes = (handle->file_size - new_start);
        if (remaining_bytes > handle->buffer_size)
            handle->buffer_range_size = handle->buffer_size;
        else
            handle->buffer_range_size = remaining_bytes;

        // read the new buffer
        if (handle->decompressor != NULL)
        {
            // decompressor usually can't handle seeks, so we 'skip' the difference in bytes
            if (new_start > (old_buffer_range_start + old_buffer_range_size))
            {
                unsigned int skip_count = new_start - (old_buffer_range_start + old_buffer_range_size);
                //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "  old_buffer_range_start = %u, old_buffer_range_size = %u", old_buffer_range_start, old_buffer_range_size);
                //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "  new buffer_range_start = %u, new buffer_range_size = %u", handle->buffer_range_start, handle->buffer_range_size);
                //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "  skipping %u decompressed bytes", skip_count);
                if ((result = handle->decompressor->decompressor_skip(mount, handle->inode_index, &handle->inode, handle->decompressor_data, skip_count)) != (int)skip_count)
                    return (result >= 0) ? FIF_ERROR_IO_ERROR : result;
            }

            result = handle->decompressor->decompressor_read(mount, handle->inode_index, &handle->inode, handle->decompressor_data, handle->buffer_range_start, handle->buffer_data, handle->buffer_range_size);
        }
        else
        {
            result = fif_read_file_data(mount, handle->inode_index, &handle->inode, handle->buffer_range_start, handle->buffer_data, handle->buffer_range_size);
        }

        if ((unsigned int)result != handle->buffer_range_size)
            return (result >= 0) ? FIF_ERROR_IO_ERROR : result;
    }

    // done
    return FIF_ERROR_SUCCESS;
}

static void cleanup_open_file(fif_mount_handle mount, fif_file_handle handle)
{
    if (handle->compressor != NULL)
        handle->compressor->compressor_cleanup(mount, handle->compressor_data);

    if (handle->decompressor != NULL)
        handle->decompressor->decompressor_cleanup(mount, handle->decompressor_data);

    assert(handle->handle_index < mount->open_file_count && mount->open_files[handle->handle_index] == handle);
    mount->open_files[handle->handle_index] = NULL;

    free(handle->buffer_data);
    free(handle);
}

int fif_open_file_by_inode(fif_mount_handle mount, fif_inode_index_t inode_index, unsigned int mode, fif_file_handle *handle)
{
    int result;

    // read the inode
    FIF_VOLUME_FORMAT_INODE inode;
    if ((result = fif_read_inode(mount, inode_index, &inode)) != FIF_ERROR_SUCCESS)
        return result;

    // if we're asking for a directory and we didn't get a directory, or vice versa bail out
    if (inode.attributes & FIF_FILE_ATTRIBUTE_DIRECTORY)
    {
        // bail out unless we're asking for a directory
        if (!(mode & FIF_OPEN_MODE_DIRECTORY))
            return FIF_ERROR_FILE_NOT_FOUND;
    }
    else if (inode.attributes & FIF_FILE_ATTRIBUTE_FILE)
    {
        // bail out unless we're asking for a file
        if (mode & FIF_OPEN_MODE_DIRECTORY)
            return FIF_ERROR_FILE_NOT_FOUND;
    }
    else
    {
        // bail out for all non-files/directories
        return FIF_ERROR_FILE_NOT_FOUND;
    }

    // check the file can be opened
    if ((result = can_open_file(mount, inode_index, mode)) != FIF_ERROR_SUCCESS)
        return FIF_ERROR_SHARING_VIOLATION;

    // if we're opening a compressed file and it has a file size, or we're opening read/write, or we're not streaming, ensure it's opened fully buffered
    if (inode.compression_algorithm != FIF_COMPRESSION_ALGORITHM_NONE)
    {
        if (((mode & FIF_OPEN_MODE_WRITE) && !(mode & FIF_OPEN_MODE_TRUNCATE) && inode.uncompressed_size > 0) ||
            (mode & (FIF_OPEN_MODE_READ | FIF_OPEN_MODE_WRITE)) == (FIF_OPEN_MODE_READ | FIF_OPEN_MODE_WRITE) ||
            !(mode & FIF_OPEN_MODE_STREAMED))
        {
            mode |= FIF_OPEN_MODE_FULLY_BUFFERED;
        }
    }

    // find an index
    unsigned int handle_index;
    for (handle_index = 0; handle_index < mount->open_file_count; handle_index++)
    {
        if (mount->open_files[handle_index] == NULL)
            break;
    }
    if (handle_index == mount->open_file_count)
    {
        unsigned int new_open_file_count = mount->open_file_count + 1;
        fif_file_handle *new_open_files = (fif_file_handle *)realloc(mount->open_files, sizeof(fif_file_handle) * new_open_file_count);
        if (new_open_files == NULL)
            return FIF_ERROR_OUT_OF_MEMORY;

        memset(new_open_files + mount->open_file_count, 0, sizeof(fif_file_handle *) * (new_open_file_count - mount->open_file_count));
        mount->open_files = new_open_files;
        mount->open_file_count = new_open_file_count;
    }

    // allocate new handle
    fif_file_handle new_handle = (fif_file_handle)malloc(sizeof(struct fif_open_file_s));
    if (new_handle == NULL)
        return FIF_ERROR_OUT_OF_MEMORY;

    // fill handle info
    new_handle->inode_index = inode_index;
    memcpy(&new_handle->inode, &inode, sizeof(new_handle->inode));
    new_handle->handle_index = handle_index;
    new_handle->open_mode = mode;
    new_handle->current_offset = 0;
    new_handle->file_size = inode.uncompressed_size;
    new_handle->buffer_data = NULL;
    new_handle->buffer_size = 0;
    new_handle->buffer_range_start = UINT_MAX;
    new_handle->buffer_range_size = 0;
    new_handle->buffer_dirty = false;
    new_handle->compressor = NULL;
    new_handle->compressor_data = NULL;
    new_handle->decompressor = NULL;
    new_handle->decompressor_data = NULL;

    // store handle
    assert(mount->open_files[new_handle->handle_index] == NULL);
    mount->open_files[new_handle->handle_index] = new_handle;

    // initialize compressor
    if (new_handle->inode.compression_algorithm != FIF_COMPRESSION_ALGORITHM_NONE)
    {
        if (mode & FIF_OPEN_MODE_WRITE)
        {
            if ((new_handle->compressor = fif_get_compressor_functions(new_handle->inode.compression_algorithm)) == NULL)
            {
                cleanup_open_file(mount, new_handle);
                return FIF_ERROR_COMPRESSOR_NOT_FOUND;
            }

            if ((result = new_handle->compressor->compressor_init(mount, new_handle->inode_index, &new_handle->inode, &new_handle->compressor_data, new_handle->inode.compression_level)) != FIF_ERROR_SUCCESS)
            {
                cleanup_open_file(mount, new_handle);
                return result;
            }
        }

        if (mode & FIF_OPEN_MODE_READ)
        {
            if ((new_handle->decompressor = fif_get_decompressor_functions(new_handle->inode.compression_algorithm)) == NULL)
            {
                cleanup_open_file(mount, new_handle);
                return FIF_ERROR_COMPRESSOR_NOT_FOUND;
            }

            if ((result = new_handle->decompressor->decompressor_init(mount, new_handle->inode_index, &new_handle->inode, &new_handle->decompressor_data)) != FIF_ERROR_SUCCESS)
            {
                cleanup_open_file(mount, new_handle);
                return result;
            }
        }
    }

    // get buffer size
    unsigned int buffer_size;
    if (mode & FIF_OPEN_MODE_FULLY_BUFFERED)
        buffer_size = (inode.uncompressed_size > 0) ? inode.uncompressed_size : mount->block_size;
    else if (mode & FIF_OPEN_MODE_DIRECT)
        buffer_size = 0;
    else
        buffer_size = mount->block_size;

    // allocate buffer
    if (buffer_size > 0 && (result = resize_open_file_buffer(new_handle, buffer_size)) != FIF_ERROR_SUCCESS)
    {
        cleanup_open_file(mount, new_handle);
        return result;
    }

    // if we're opening the file with truncate, free all the blocks associated with the file and update the inode
    if (mode & FIF_OPEN_MODE_TRUNCATE && new_handle->file_size > 0)
    {
        // free the blocks
        if ((result = fif_free_file_blocks(mount, new_handle->inode_index, &new_handle->inode)) != FIF_ERROR_SUCCESS)
        {
            cleanup_open_file(mount, new_handle);
            return result;
        }
    }

    // if we're opening in read mode, and fully buffered, we have to read the whole file in
    if ((mode & (FIF_OPEN_MODE_READ | FIF_OPEN_MODE_FULLY_BUFFERED)) == (FIF_OPEN_MODE_READ | FIF_OPEN_MODE_FULLY_BUFFERED))
    {
        assert(new_handle->buffer_size >= inode.uncompressed_size);
        if (new_handle->decompressor != NULL)
            result = new_handle->decompressor->decompressor_read(mount, new_handle->inode_index, &new_handle->inode, new_handle->decompressor_data, 0, new_handle->buffer_data, inode.uncompressed_size);
        else
            result = fif_read_file_data(mount, inode_index, &inode, 0, new_handle->buffer_data, inode.uncompressed_size);

        // check result
        if ((unsigned int)result != inode.uncompressed_size)
        {
            cleanup_open_file(mount, new_handle);
            return (result >= 0) ? FIF_ERROR_IO_ERROR : result;
        }

        // update buffer range
        new_handle->buffer_range_start = 0;
        new_handle->buffer_range_size = inode.uncompressed_size;
    }

    // done
    *handle = new_handle;
    return FIF_ERROR_SUCCESS;
}

int fif_file_read(fif_mount_handle mount, fif_file_handle file, void *out_buffer, unsigned int count)
{
    int result;

    // can't read past the end of the file
    if ((file->current_offset + count) > file->file_size)
        count = file->file_size - file->current_offset;

    // buffered reads
    unsigned char *out_buffer_ptr = (unsigned char *)out_buffer;
    unsigned int remaining_bytes = count;
    while (remaining_bytes > 0)
    {
        // grab any bytes from the buffer that we can
        if (file->buffer_range_start <= file->current_offset)
        {
            unsigned int buffer_offset = (file->current_offset - file->buffer_range_start);
            if (buffer_offset < file->buffer_range_size)
            {
                unsigned int bytes_from_buffer = file->buffer_range_size - buffer_offset;
                if (bytes_from_buffer > remaining_bytes)
                    bytes_from_buffer = remaining_bytes;

                // copy from buffer
                memcpy(out_buffer_ptr, file->buffer_data + buffer_offset, bytes_from_buffer);
                out_buffer_ptr += bytes_from_buffer;
                remaining_bytes -= bytes_from_buffer;

                // increment file pointer
                file->current_offset += bytes_from_buffer;

                // if there's no bytes left, don't bother reading the next chunk
                if (remaining_bytes == 0)
                    break;
            }
        }

        // fill the buffer with new data, if the buffer is now empty, it means there's no more data
        if ((result = update_file_buffer(mount, file, file->current_offset)) != FIF_ERROR_SUCCESS || file->buffer_range_size == 0)
            return count - remaining_bytes;
    }

    // got everything
    assert(remaining_bytes == 0);
    return count;
}

int fif_file_write(fif_mount_handle mount, fif_file_handle file, const void *in_buffer, unsigned int count)
{
    int result;
    //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "fif_file_write(%u, %u)", file->inode_index, count);

    // if there's zero bytes, bail out early
    if (count == 0)
        return 0;

    // special case for fully-buffered files
    if (file->open_mode & FIF_OPEN_MODE_FULLY_BUFFERED)
    {
        // extending the file?
        if ((file->current_offset + count) > file->file_size)
        {
            // resize buffer
            if ((result = resize_open_file_buffer(file, file->current_offset + count)) != FIF_ERROR_SUCCESS)
                return result;

            // update size
            file->file_size = file->current_offset + count;
        }

        // simply copy to the buffer
        assert((file->buffer_range_size + count) <= file->buffer_size);
        memcpy(file->buffer_data + file->buffer_range_size, in_buffer, count);
        file->buffer_range_size += count;
        file->buffer_dirty = true;
        file->current_offset += count;
        return count;
    }

    // buffered writes
    unsigned char *in_buffer_ptr = (unsigned char *)in_buffer;
    unsigned int remaining_bytes = count;
    while (remaining_bytes > 0)
    {
        // write to the current buffer if possible
        if (file->buffer_range_start <= file->current_offset)
        {
            unsigned int buffer_offset = (file->current_offset - file->buffer_range_start);
            if (buffer_offset < file->buffer_size)
            {
                unsigned int buffer_space_remaining = (file->buffer_size - buffer_offset);
                unsigned int bytes_to_buffer = buffer_space_remaining;
                if (bytes_to_buffer > remaining_bytes)
                    bytes_to_buffer = remaining_bytes;

                // calculate new buffer size
                unsigned int new_buffer_size = buffer_offset + bytes_to_buffer;

                // copy from buffer
                memcpy(file->buffer_data + buffer_offset, in_buffer_ptr, bytes_to_buffer);
                in_buffer_ptr += bytes_to_buffer;
                remaining_bytes -= bytes_to_buffer;
                file->current_offset += bytes_to_buffer;
                file->buffer_range_size = (new_buffer_size > file->buffer_range_size) ? new_buffer_size : file->buffer_range_size;
                file->buffer_dirty = true;
                assert(file->buffer_range_size <= file->buffer_size);

                // update file size
                if (file->current_offset > file->file_size)
                    file->file_size = file->current_offset;

                // if this is everything, bail out early
                if (remaining_bytes == 0)
                    break;
            }
        }

        // flush the buffer and move it forwards
        if ((result = update_file_buffer(mount, file, file->current_offset)) != FIF_ERROR_SUCCESS)
            return count - remaining_bytes;
    }

    // done
    assert(remaining_bytes == 0);
    return count;
}

fif_offset_t fif_file_seek(fif_mount_handle mount, fif_file_handle file, fif_offset_t offset, enum FIF_SEEK_MODE mode)
{
    (void)mount;

    // find new offset
    unsigned int new_offset = UINT_MAX;
    if (mode == FIF_SEEK_MODE_SET)
        new_offset = (unsigned int)offset;
    else if (mode == FIF_SEEK_MODE_CUR)
        new_offset = (unsigned int)((fif_offset_t)file->current_offset + offset);
    else if (mode == FIF_SEEK_MODE_END)
        new_offset = file->file_size;

    // check offset
    if (new_offset > file->file_size)
        return FIF_ERROR_BAD_OFFSET;

    // streamed files have severe limitations on seeking
    if (file->open_mode & FIF_OPEN_MODE_STREAMED)
    {
        // if it's opened write, we can't seek at all. if it's opened read, we can only seek forward
        if ((file->open_mode & FIF_OPEN_MODE_WRITE) || (new_offset < file->current_offset))
            return FIF_ERROR_BAD_OFFSET;
    }

    // update it
    file->current_offset = new_offset;
    return file->current_offset;
}

int fif_file_truncate(fif_mount_handle mount, fif_file_handle file, fif_offset_t size)
{
    int result;
    if ((result = fif_resize_file(mount, file->inode_index, &file->inode, (unsigned int)size)) != FIF_ERROR_SUCCESS)
        return result;

    file->file_size = (unsigned int)size;
    return FIF_ERROR_SUCCESS;
}

int fif_file_close(fif_mount_handle mount, fif_file_handle file)
{
    if (file->open_mode & FIF_OPEN_MODE_WRITE)
    {
        int result;

        // flush the buffer
        if (file->buffer_range_size > 0 && file->buffer_dirty)
        {
            if (file->compressor != NULL)
                result = file->compressor->compressor_write(mount, file->inode_index, &file->inode, file->compressor_data, file->buffer_range_start, file->buffer_data, file->buffer_range_size);
            else
                result = fif_write_file_data(mount, file->inode_index, &file->inode, file->buffer_range_start, file->buffer_data, file->buffer_range_size);

            // write the data
            if ((unsigned int)result != file->buffer_range_size)
            {
                cleanup_open_file(mount, file);
                return (result >= 0) ? FIF_ERROR_IO_ERROR : result;
            }
        }

        // for filtered files, end the filter
        if (file->compressor != NULL)
        {
            if ((result = file->compressor->compressor_end(mount, file->inode_index, &file->inode, file->compressor_data)) != FIF_ERROR_SUCCESS)
            {
                cleanup_open_file(mount, file);
                return result;
            }
        }

        // update information in the inode
        file->inode.uncompressed_size = file->file_size;
        file->inode.modification_timestamp = fif_current_timestamp();
        if ((result = fif_write_inode(mount, file->inode_index, &file->inode)) != FIF_ERROR_SUCCESS)
            return result;
    }

    // cleanup and done
    cleanup_open_file(mount, file);
    return FIF_ERROR_SUCCESS;
}

int fif_stat(fif_mount_handle mount, const char *path, fif_fileinfo *fileinfo)
{
    int result;
    if (mount->trace_stream != NULL && (result = fif_trace_write_stat(mount, path)) != FIF_ERROR_SUCCESS)
        return result;

    // find the file
    fif_inode_index_t file_inode;
    if ((result = fif_resolve_file_name(mount, path, &file_inode, NULL)) != FIF_ERROR_SUCCESS)
        return result;

    // read inode
    FIF_VOLUME_FORMAT_INODE inode;
    if ((result = fif_read_inode(mount, file_inode, &inode)) != FIF_ERROR_SUCCESS)
        return result;

    // store info
    fileinfo->attributes = inode.attributes;
    fileinfo->block_count = inode.block_count;
    fileinfo->compression_algorithm = inode.compression_algorithm;
    fileinfo->compression_level = inode.compression_level;
    fileinfo->data_size = inode.data_size;
    fileinfo->size = inode.uncompressed_size;
    fileinfo->checksum = inode.checksum;
    fileinfo->creation_timestamp = inode.creation_timestamp;
    fileinfo->modify_timestamp = inode.modification_timestamp;
    return FIF_ERROR_SUCCESS;
}

int fif_fstat(fif_mount_handle mount, fif_file_handle file, fif_fileinfo *fileinfo)
{
    int result;
    if (mount->trace_stream != NULL && (result = fif_trace_write_fstat(mount, file)) != FIF_ERROR_SUCCESS)
        return result;

    // store info from open file
    fileinfo->attributes = file->inode.attributes;
    fileinfo->block_count = file->inode.block_count;
    fileinfo->compression_algorithm = file->inode.compression_algorithm;
    fileinfo->compression_level = file->inode.compression_level;
    fileinfo->data_size = file->inode.data_size;
    fileinfo->size = file->inode.uncompressed_size;
    fileinfo->checksum = file->inode.checksum;
    fileinfo->creation_timestamp = file->inode.creation_timestamp;
    fileinfo->modify_timestamp = file->inode.modification_timestamp;
    return FIF_ERROR_SUCCESS;
}

int fif_open(fif_mount_handle mount, const char *path, unsigned int mode, fif_file_handle *file)
{
    int result;
    if (mount->trace_stream != NULL && (result = fif_trace_write_open(mount, path, mode)) != FIF_ERROR_SUCCESS)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_trace_write_open failed: %i", result);
        return result;
    }

    // copy the path, canonicalize it
    int path_length = (int)strlen(path);
    char *path_copy = (char *)alloca(path_length + 1);
    if (!fif_canonicalize_path(path_copy, path_length + 1, path))
        return FIF_ERROR_BAD_PATH;

    // split to dirname and basename
    char *dirname, *basename;
    fif_split_path_dirbase(path_copy, &dirname, &basename);

    // resolve the file's containing directory
    fif_inode_index_t directory_inode;
    if (dirname == NULL)
        directory_inode = mount->root_inode;
    else if ((result = fif_resolve_directory_name(mount, dirname, &directory_inode)) != FIF_ERROR_SUCCESS)
        return result;

    // does the file exist in the directory?
    fif_inode_index_t file_inode_index;
    if ((result = fif_find_file_in_directory(mount, directory_inode, basename, &file_inode_index, NULL)) != FIF_ERROR_SUCCESS)
    {
        // if the error was NOT_FOUND, and we have a create open mode, create the file
        if (result != FIF_ERROR_FILE_NOT_FOUND || !(mode & FIF_OPEN_MODE_CREATE))
            return result;

        // create the file
        if ((result = fif_create_file(mount, basename, directory_inode, &file_inode_index)) != FIF_ERROR_SUCCESS)
            return result;
    }

    // don't allow opening a directory with the external api, since they could corrupt our internal structure
    mode &= ~(FIF_OPEN_MODE_DIRECTORY);

    // forward through to open by inode
    return fif_open_file_by_inode(mount, file_inode_index, mode, file);
}

int fif_read(fif_mount_handle mount, fif_file_handle file, void *out_buffer, unsigned int count)
{
    int result;
    if (mount->trace_stream != NULL && (result = fif_trace_write_read(mount, file, count)) != FIF_ERROR_SUCCESS)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_trace_write_read failed: %i", result);
        return result;
    }

    return fif_file_read(mount, file, out_buffer, count);
}

int fif_write(fif_mount_handle mount, fif_file_handle file, const void *in_buffer, unsigned int count)
{
    int result;
    if (mount->trace_stream != NULL && (result = fif_trace_write_write(mount, file, in_buffer, count)) != FIF_ERROR_SUCCESS)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_trace_write_write failed: %i", result);
        return result;
    }

    return fif_file_write(mount, file, in_buffer, count);
}

fif_offset_t fif_seek(fif_mount_handle mount, fif_file_handle file, fif_offset_t offset, enum FIF_SEEK_MODE mode)
{
    int result;
    if (mount->trace_stream != NULL && (result = fif_trace_write_seek(mount, file, offset, mode)) != FIF_ERROR_SUCCESS)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_trace_write_seek failed: %i", result);
        return result;
    }

    return fif_file_seek(mount, file, offset, mode);
}

int fif_tell(fif_mount_handle mount, fif_file_handle file)
{
    int result;
    if (mount->trace_stream != NULL && (result = fif_trace_write_tell(mount, file)) != FIF_ERROR_SUCCESS)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_trace_write_tell failed: %i", result);
        return result;
    }

    return file->current_offset;
}

int fif_ftruncate(fif_mount_handle mount, fif_file_handle file, fif_offset_t size)
{
    int result;
    if (mount->trace_stream != NULL && (result = fif_trace_write_ftruncate(mount, file, size)) != FIF_ERROR_SUCCESS)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_trace_write_ftruncate failed: %i", result);
        return result;
    }

    return fif_file_truncate(mount, file, size);
}

int fif_close(fif_mount_handle mount, fif_file_handle handle)
{
    int result;
    if (mount->trace_stream != NULL && (result = fif_trace_write_close(mount, handle)) != FIF_ERROR_SUCCESS)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_trace_write_close failed: %i", result);
        return result;
    }

    return fif_file_close(mount, handle);
}

int fif_unlink(fif_mount_handle mount, const char *filename)
{
    int result;
    if (mount->trace_stream != NULL && (result = fif_trace_write_unlink(mount, filename)) != FIF_ERROR_SUCCESS)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_trace_write_unlink failed: %i", result);
        return result;
    }

    // fix up path
    int filename_length = (int)strlen(filename);
    char *filename_copy = (char *)alloca(filename_length + 1);
    fif_canonicalize_path(filename_copy, filename_length + 1, filename);

    // split to dirname + basename
    char *real_dirname, *real_basename;
    fif_split_path_dirbase(filename_copy, &real_dirname, &real_basename);

    // find the containing directory
    fif_inode_index_t containing_inode;
    if (real_dirname == NULL)
        containing_inode = mount->root_inode;
    else if ((result = fif_resolve_directory_name(mount, real_dirname, &containing_inode)) != FIF_ERROR_SUCCESS)
        return result;

    // find the file in the directory
    fif_inode_index_t file_inode_index;
    if ((result = fif_find_file_in_directory(mount, containing_inode, real_basename, &file_inode_index, NULL)) != FIF_ERROR_SUCCESS)
        return result;

    // verify we can write to this file
    if ((result = can_open_file(mount, file_inode_index, FIF_OPEN_MODE_WRITE | FIF_OPEN_MODE_TRUNCATE)) != FIF_ERROR_SUCCESS)
        return FIF_ERROR_SHARING_VIOLATION;

    // read the inode
    FIF_VOLUME_FORMAT_INODE file_inode;
    if ((result = fif_read_inode(mount, file_inode_index, &file_inode)) != FIF_ERROR_SUCCESS)
        return result;

    // bail out for anything that's not a file
    if (!(file_inode.attributes & FIF_FILE_ATTRIBUTE_FILE))
        return FIF_ERROR_FILE_NOT_FOUND;

    // remove the file from the directory
    if ((result = fif_remove_file_from_directory(mount, containing_inode, real_basename)) != FIF_ERROR_SUCCESS)
        return result;

    // if that's the last reference, free the inode
    if ((--file_inode.reference_count) == 0)
    {
        fif_free_file_blocks(mount, file_inode_index, &file_inode);
        return fif_free_inode(mount, file_inode_index);
    }
    else
    {
        // just rewrite the inode
        return fif_write_inode(mount, file_inode_index, &file_inode);
    }
}

int fif_get_file_contents(fif_mount_handle mount, const char *filename, void *buffer, unsigned int maxcount)
{
    int result;
    if (mount->trace_stream != NULL && (result = fif_trace_write_get_file_contents(mount, filename, maxcount)) != FIF_ERROR_SUCCESS)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_trace_write_get_file_contents failed: %i", result);
        return result;
    }

    // find the file
    fif_inode_index_t file_inode_index;
    if ((result = fif_resolve_file_name(mount, filename, &file_inode_index, NULL)) != FIF_ERROR_SUCCESS)
        return result;

    // read the inode
    FIF_VOLUME_FORMAT_INODE inode;
    if ((result = fif_read_inode(mount, file_inode_index, &inode)) != FIF_ERROR_SUCCESS)
        return result;

    // get how many bytes to read
    unsigned int bytes_to_read = (maxcount > inode.uncompressed_size) ? inode.uncompressed_size : maxcount;

    // get decompressor if there is one
    const struct fif_decompressor_functions *decompressor = NULL;
    if (inode.compression_algorithm != FIF_COMPRESSION_ALGORITHM_NONE && (decompressor = fif_get_decompressor_functions(inode.compression_algorithm)) == NULL)
        return FIF_ERROR_COMPRESSOR_NOT_FOUND;

    // read the file
    if (bytes_to_read > 0)
    {
        if (decompressor != NULL)
        {
            void *decompressor_data;

            // initialize decompressor
            if ((result = decompressor->decompressor_init(mount, file_inode_index, &inode, &decompressor_data)) != FIF_ERROR_SUCCESS)
                return result;

            // decompress the file
            result = decompressor->decompressor_read(mount, file_inode_index, &inode, decompressor_data, 0, buffer, bytes_to_read);

            // cleanup decompressor
            decompressor->decompressor_cleanup(mount, decompressor_data);
        }
        else
        {
            // pass through
            result = fif_read_file_data(mount, file_inode_index, &inode, 0, buffer, bytes_to_read);
        }
    }
    else
    {
        // no bytes
        result = 0;
    }

    // done
    return result;
}

int fif_put_file_contents(fif_mount_handle mount, const char *filename, const void *buffer, unsigned int count)
{
    int result;
    if (mount->trace_stream != NULL && (result = fif_trace_write_put_file_contents(mount, filename, buffer, count)) != FIF_ERROR_SUCCESS)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_trace_write_put_file_contents failed: %i", result);
        return result;
    }

    // copy the path, canonicalize it
    int path_length = (int)strlen(filename);
    char *path_copy = (char *)alloca(path_length + 1);
    if (!fif_canonicalize_path(path_copy, path_length + 1, filename))
        return FIF_ERROR_BAD_PATH;

    // split to dirname and basename
    char *dirname, *basename;
    fif_split_path_dirbase(path_copy, &dirname, &basename);

    // resolve the file's containing directory
    fif_inode_index_t directory_inode;
    if (dirname == NULL)
        directory_inode = mount->root_inode;
    else if ((result = fif_resolve_directory_name(mount, dirname, &directory_inode)) != FIF_ERROR_SUCCESS)
        return result;

    // does the file exist in the directory?
    fif_inode_index_t file_inode_index;
    if ((result = fif_find_file_in_directory(mount, directory_inode, basename, &file_inode_index, NULL)) != FIF_ERROR_SUCCESS)
    {
        // if the error was NOT_FOUND, and we have a create open mode, create the file
        if (result != FIF_ERROR_FILE_NOT_FOUND)
            return result;

        // create the file
        if ((result = fif_create_file(mount, basename, directory_inode, &file_inode_index)) != FIF_ERROR_SUCCESS)
            return result;
    }

    // verify we can write to this file
    if ((result = can_open_file(mount, file_inode_index, FIF_OPEN_MODE_WRITE)) != FIF_ERROR_SUCCESS)
        return FIF_ERROR_SHARING_VIOLATION;

    // read the inode
    FIF_VOLUME_FORMAT_INODE inode;
    if ((result = fif_read_inode(mount, file_inode_index, &inode)) != FIF_ERROR_SUCCESS)
        return result;

    // initialize compressor if we have one
    const struct fif_compressor_functions *compressor = NULL;
    if (inode.compression_algorithm != FIF_COMPRESSION_ALGORITHM_NONE && (compressor = fif_get_compressor_functions(inode.compression_algorithm)) == NULL)
        return FIF_ERROR_COMPRESSOR_NOT_FOUND;

    // nuke any contents of the file
    if (inode.data_size > 0)
    {
        if ((result = fif_free_file_blocks(mount, file_inode_index, &inode)) != FIF_ERROR_SUCCESS)
            return result;
    }

    // write any data
    if (count > 0)
    {
        if (compressor != NULL)
        {
            void *compressor_data;

            // initialize compressor
            if ((result = compressor->compressor_init(mount, file_inode_index, &inode, &compressor_data, inode.compression_level)) != FIF_ERROR_SUCCESS)
                return result;

            if (compressor->compressor_write(mount, file_inode_index, &inode, compressor_data, 0, buffer, count) == (int)count)
                result = compressor->compressor_end(mount, file_inode_index, &inode, compressor_data);
            else
                result = FIF_ERROR_IO_ERROR;

            // cleanup compressor
            compressor->compressor_cleanup(mount, compressor_data);

        }
        else
        {
            if (fif_write_file_data(mount, file_inode_index, &inode, 0, buffer, count) == (int)count)
                result = FIF_ERROR_SUCCESS;
            else
                result = FIF_ERROR_IO_ERROR;
        }
    }

    // handle errors
    if (result != FIF_ERROR_SUCCESS)
        return result;

    // update the inode
    inode.modification_timestamp = fif_current_timestamp();
    inode.uncompressed_size = count;
    if ((result = fif_write_inode(mount, file_inode_index, &inode)) != FIF_ERROR_SUCCESS)
        return result;

    // done
    return FIF_ERROR_SUCCESS;
}

int fif_compress_file(fif_mount_handle mount, const char *filename, enum FIF_COMPRESSION_ALGORITHM new_compression_algorithm, unsigned int new_compression_level)
{
    int result;
    if (mount->trace_stream != NULL && (result = fif_trace_write_compress_file(mount, filename, new_compression_algorithm, new_compression_level)) != FIF_ERROR_SUCCESS)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_trace_write_compress_file failed: %i", result);
        return result;
    }

    if (mount->read_only)
        return FIF_ERROR_READ_ONLY;

    return -1;
}


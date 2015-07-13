#include "fif_internal.h"
#include "trace.h"

int fif_create_directory(fif_mount_handle mount, fif_inode_index_t inode_hint, fif_inode_index_t *out_directory_inode_index)
{
    int result;

    // allocate inode
    fif_inode_index_t directory_inode_index;
    if ((result = fif_alloc_inode(mount, inode_hint, &directory_inode_index)) != FIF_ERROR_SUCCESS)
        return result;

    // store directory inode
    FIF_VOLUME_FORMAT_INODE directory_inode;
    memset(&directory_inode, 0, sizeof(directory_inode));
    directory_inode.creation_timestamp = fif_current_timestamp();
    directory_inode.modification_timestamp = fif_current_timestamp();
    directory_inode.attributes = FIF_FILE_ATTRIBUTE_DIRECTORY;
    directory_inode.reference_count = 0;
    directory_inode.next_entry = 0;
    directory_inode.compression_algorithm = FIF_COMPRESSION_ALGORITHM_NONE;
    directory_inode.compression_level = 0;
    directory_inode.uncompressed_size = 0;
    directory_inode.data_size = 0;
    directory_inode.checksum = 0;
    directory_inode.first_block_index = 0;
    directory_inode.block_count = 0;
    if ((result = fif_write_inode(mount, directory_inode_index, &directory_inode)) != FIF_ERROR_SUCCESS)
        return result;

    // open the root directory as a file
    fif_file_handle directory_file;
    if ((result = fif_open_file_by_inode(mount, directory_inode_index, FIF_OPEN_MODE_WRITE | FIF_OPEN_MODE_TRUNCATE | FIF_OPEN_MODE_STREAMED | FIF_OPEN_MODE_DIRECTORY, &directory_file)) != FIF_ERROR_SUCCESS)
        return result;

    // write root directory header
    FIF_VOLUME_FORMAT_DIRECTORY_HEADER root_header;
    root_header.magic = FIF_VOLUME_FORMAT_DIRECTORY_HEADER_MAGIC;
    root_header.file_count = 0;
    root_header.max_filename_length = 0;
    root_header.first_file_inode = root_header.last_file_inode = 0;
    if ((result = fif_file_write(mount, directory_file, &root_header, sizeof(root_header))) != sizeof(root_header))
    {
        fif_file_close(mount, directory_file);
        if (result > 0)
            result = FIF_ERROR_IO_ERROR;
    }

    // close root directory file
    if ((result = fif_file_close(mount, directory_file)) != FIF_ERROR_SUCCESS)
        return result;

    // done
    *out_directory_inode_index = directory_inode_index;
    return FIF_ERROR_SUCCESS;
}

int fif_resolve_directory_name(fif_mount_handle mount, const char *dirname, fif_inode_index_t *directory_inode_index)
{
    // assumes an already-canonicalized directory name
    int dirname_length = (int)strlen(dirname);
    if (dirname_length == 1 && dirname[0] == '/')
    {
        // read the root inode
        *directory_inode_index = mount->root_inode;
        return FIF_ERROR_SUCCESS;
    }

    // allocate a copy of the directory name
    char *dirname_copy = (char *)alloca(dirname_length + 1);
    memcpy(dirname_copy, dirname, dirname_length + 1);

    // split the path
    int path_components = fif_split_path(dirname_copy);
    if (path_components == 0)
        return FIF_ERROR_BAD_PATH;

    // process each part
    int result;
    char *path_part = dirname_copy;
    fif_inode_index_t current_inode_index = mount->root_inode;
    for (int i = 0; i < path_components; i++)
    {
        // get next path part
        path_part = fif_path_next_part(path_part);

        // search the current directory for a file with this name
        if ((result = fif_find_file_in_directory(mount, current_inode_index, path_part, &current_inode_index, NULL)) != FIF_ERROR_SUCCESS)
            return result;
    }

    // if we're here we found the file
    *directory_inode_index = current_inode_index;
    return FIF_ERROR_SUCCESS;
}

int fif_find_file_in_directory(fif_mount_handle mount, fif_inode_index_t directory_inode_index, const char *filename, fif_inode_index_t *file_inode_index, unsigned int *file_index_in_directory)
{
    int result;
    fif_offset_t seek_result;
    unsigned int filename_length = (unsigned int)strlen(filename);

    // open the directory as a file
    fif_file_handle directory_file_handle;
    if ((result = fif_open_file_by_inode(mount, directory_inode_index, FIF_OPEN_MODE_READ | FIF_OPEN_MODE_STREAMED | FIF_OPEN_MODE_DIRECTORY, &directory_file_handle)) != FIF_ERROR_SUCCESS)
        return result;

    // read the directory header
    FIF_VOLUME_FORMAT_DIRECTORY_HEADER directory_header;
    if ((result = fif_file_read(mount, directory_file_handle, &directory_header, sizeof(directory_header))) != sizeof(directory_header))
    {
        fif_file_close(mount, directory_file_handle);
        return (result >= 0) ? FIF_ERROR_CORRUPT_VOLUME : result;
    }

    // check it
    if (directory_header.magic != FIF_VOLUME_FORMAT_DIRECTORY_HEADER_MAGIC)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_find_file_in_directory: bad directory magic in inode %u", directory_inode_index);
        fif_file_close(mount, directory_file_handle);
        return FIF_ERROR_CORRUPT_VOLUME;
    }

    // iterate through each file
    char *entry_filename = (char *)alloca(filename_length + 1);
    unsigned int file_count = directory_header.file_count;
    for (unsigned int i = 0; i < file_count; i++)
    {
        FIF_VOLUME_FORMAT_DIRECTORY_ENTRY directory_entry;
        if ((result = fif_file_read(mount, directory_file_handle, &directory_entry, sizeof(directory_entry))) != sizeof(directory_entry))
        {
            fif_file_close(mount, directory_file_handle);
            return (result >= 0) ? FIF_ERROR_CORRUPT_VOLUME : result;
        }

        // does the name length match? if not, we can skip reading it
        if (directory_entry.name_length == filename_length)
        {
            // read the filename
            if ((result = fif_file_read(mount, directory_file_handle, entry_filename, filename_length)) != filename_length)
            {
                fif_file_close(mount, directory_file_handle);
                return (result >= 0) ? FIF_ERROR_CORRUPT_VOLUME : result;
            }

            // compare it
            entry_filename[filename_length] = '\0';
#ifdef _MSC_VER
            if (_stricmp(filename, entry_filename) == 0)
#else
            if (strcasecmp(filename, entry_filename) == 0)
#endif
            {
                // found the file, store the info
                if (file_inode_index != NULL)
                    *file_inode_index = directory_entry.inode_index;
                if (file_index_in_directory != NULL)
                    *file_index_in_directory = i;

                // close handle and exit
                fif_file_close(mount, directory_file_handle);
                return FIF_ERROR_SUCCESS;
            }
        }
        else
        {
            if ((seek_result = fif_file_seek(mount, directory_file_handle, directory_entry.name_length, FIF_SEEK_MODE_CUR)) < 0)
            {
                fif_file_close(mount, directory_file_handle);
                return FIF_ERROR_CORRUPT_VOLUME;
            }
        }
    }

    // not found :(
    fif_file_close(mount, directory_file_handle);
    return FIF_ERROR_FILE_NOT_FOUND;
}

int fif_add_file_to_directory(fif_mount_handle mount, fif_inode_index_t directory_inode_index, const char *filename, fif_inode_index_t fileinode)
{
    int result;
    fif_offset_t seek_result;

    // open the directory inode as a file
    fif_file_handle directory_file;
    if ((result = fif_open_file_by_inode(mount, directory_inode_index, FIF_OPEN_MODE_READ | FIF_OPEN_MODE_WRITE | FIF_OPEN_MODE_DIRECTORY, &directory_file)) != FIF_ERROR_SUCCESS)
        return result;

    // read the directory header
    FIF_VOLUME_FORMAT_DIRECTORY_HEADER directory_header;
    if (fif_file_read(mount, directory_file, &directory_header, sizeof(directory_header)) != sizeof(directory_header))
    {
        fif_file_close(mount, directory_file);
        return (result >= 0) ? FIF_ERROR_CORRUPT_VOLUME : result;
    }

    // validate header
    if (directory_header.magic != FIF_VOLUME_FORMAT_DIRECTORY_HEADER_MAGIC)
    {
        fif_file_close(mount, directory_file);
        return FIF_ERROR_CORRUPT_VOLUME;
    }

    // update header
    int filename_length = (int)strlen(filename);
    if ((directory_header.file_count++) == 0)
    {
        // first file
        directory_header.max_filename_length = filename_length;
        directory_header.first_file_inode = directory_header.last_file_inode = fileinode;
    }
    else
    {
        // new file
        if (filename_length > (int)directory_header.max_filename_length)
            directory_header.max_filename_length = filename_length;
        if (fileinode < directory_header.first_file_inode)
            directory_header.first_file_inode = fileinode;
        if (fileinode > directory_header.last_file_inode)
            directory_header.last_file_inode = fileinode;
    }

    // rewrite header
    if ((seek_result = fif_file_seek(mount, directory_file, 0, FIF_SEEK_MODE_SET)) < 0 || (result = fif_file_write(mount, directory_file, &directory_header, sizeof(directory_header))) != sizeof(directory_header))
    {
        fif_file_close(mount, directory_file);
        return (result >= 0) ? FIF_ERROR_CORRUPT_VOLUME : result;
    }

    // construct the entry, todo vectored write to write entry and filename in one swoop
    FIF_VOLUME_FORMAT_DIRECTORY_ENTRY directory_entry;
    directory_entry.inode_index = fileinode;
    directory_entry.name_length = filename_length;
    if ((seek_result = fif_file_seek(mount, directory_file, 0, FIF_SEEK_MODE_END)) < 0 ||
        (result = fif_file_write(mount, directory_file, &directory_entry, sizeof(directory_entry))) != sizeof(directory_entry) ||
        (result = fif_file_write(mount, directory_file, filename, filename_length)) != filename_length)
    {
        fif_file_close(mount, directory_file);
        return (result >= 0) ? FIF_ERROR_CORRUPT_VOLUME : result;
    }

    // done with the directory now
    fif_file_close(mount, directory_file);
    return FIF_ERROR_SUCCESS;
}

int fif_remove_file_from_directory(fif_mount_handle mount, fif_inode_index_t directory_inode_index, const char *filename)
{
    // optimize this...
    int result;
    fif_offset_t seek_result;
    unsigned int filename_length = (unsigned int)strlen(filename);

    // open the directory as a file
    fif_file_handle directory_file_handle;
    if ((result = fif_open_file_by_inode(mount, directory_inode_index, FIF_OPEN_MODE_READ | FIF_OPEN_MODE_WRITE | FIF_OPEN_MODE_STREAMED | FIF_OPEN_MODE_DIRECTORY, &directory_file_handle)) != FIF_ERROR_SUCCESS)
        return result;

    // read the directory header
    FIF_VOLUME_FORMAT_DIRECTORY_HEADER directory_header;
    if ((result = fif_file_read(mount, directory_file_handle, &directory_header, sizeof(directory_header))) != sizeof(directory_header))
    {
        fif_file_close(mount, directory_file_handle);
        return (result >= 0) ? FIF_ERROR_CORRUPT_VOLUME : result;
    }

    // check it
    if (directory_header.magic != FIF_VOLUME_FORMAT_DIRECTORY_HEADER_MAGIC)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_find_file_in_directory: bad directory magic in inode %u", directory_inode_index);
        fif_file_close(mount, directory_file_handle);
        return FIF_ERROR_CORRUPT_VOLUME;
    }

    // iterate through each file
    char *entry_filename = (char *)alloca(filename_length + 1);
    unsigned int file_count = directory_header.file_count;
    for (unsigned int i = 0; i < file_count; i++)
    {
        // store current entry's offset
        unsigned int current_entry_offset = directory_file_handle->current_offset;

        // read entry
        FIF_VOLUME_FORMAT_DIRECTORY_ENTRY directory_entry;
        if ((result = fif_file_read(mount, directory_file_handle, &directory_entry, sizeof(directory_entry))) != sizeof(directory_entry))
        {
            fif_file_close(mount, directory_file_handle);
            return (result >= 0) ? FIF_ERROR_CORRUPT_VOLUME : result;
        }

        // does the name length match? if not, we can skip reading it
        if (directory_entry.name_length == filename_length)
        {
            // read the filename
            if ((result = fif_file_read(mount, directory_file_handle, entry_filename, filename_length)) != filename_length)
            {
                fif_file_close(mount, directory_file_handle);
                return (result >= 0) ? FIF_ERROR_CORRUPT_VOLUME : result;
            }

            // compare it
            entry_filename[filename_length] = '\0';
#ifdef _MSC_VER
            if (_stricmp(filename, entry_filename) == 0)
#else
            if (strcasecmp(filename, entry_filename) == 0)
#endif
            {
                // get the remaining file size after this directory entry, and read in this data
                unsigned int remaining_bytes = directory_file_handle->file_size - directory_file_handle->current_offset;
                unsigned char *remaining_data = NULL;
                if (remaining_bytes > 0)
                {
                    remaining_data = (unsigned char *)malloc(remaining_bytes);
                    if (remaining_data == NULL)
                    {
                        free(remaining_data);
                        return FIF_ERROR_OUT_OF_MEMORY;
                    }
                    if ((result = fif_file_read(mount, directory_file_handle, remaining_data, remaining_bytes)) != remaining_bytes)
                    {
                        free(remaining_data);
                        fif_file_close(mount, directory_file_handle);
                        return (result >= 0) ? FIF_ERROR_IO_ERROR : result;
                    }

                    // seek back to this entry's offset, and write the remaining data, thus overwriting it
                    if ((seek_result = fif_file_seek(mount, directory_file_handle, current_entry_offset, FIF_SEEK_MODE_SET)) < 0 ||
                        (result = fif_file_write(mount, directory_file_handle, remaining_data, remaining_bytes)) != remaining_bytes)
                    {
                        free(remaining_data);
                        fif_file_close(mount, directory_file_handle);
                        return (result >= 0) ? FIF_ERROR_IO_ERROR : result;
                    }

                    // buffer can go now
                    free(remaining_data);
                }

                // truncate to the correct size
                if ((result = fif_file_truncate(mount, directory_file_handle, current_entry_offset + remaining_bytes)) != FIF_ERROR_SUCCESS)
                {
                    fif_file_close(mount, directory_file_handle);
                    return result;
                }

                // close handle and exit
                fif_file_close(mount, directory_file_handle);
                return FIF_ERROR_SUCCESS;
            }
        }
        else
        {
            if ((seek_result = fif_file_seek(mount, directory_file_handle, directory_entry.name_length, FIF_SEEK_MODE_CUR)) < 0)
            {
                fif_file_close(mount, directory_file_handle);
                return FIF_ERROR_CORRUPT_VOLUME;
            }
        }
    }

    // not found :(
    fif_file_close(mount, directory_file_handle);
    return FIF_ERROR_FILE_NOT_FOUND;
}

static int read_entry_and_invoke_callback(fif_mount_handle mount, fif_file_handle directory_file, fif_enumdir_callback callback, void *userdata)
{
    int result;

    FIF_VOLUME_FORMAT_DIRECTORY_ENTRY directory_entry;
    if ((result = fif_file_read(mount, directory_file, &directory_entry, sizeof(directory_entry))) != sizeof(directory_entry))
        return (result >= 0) ? FIF_ERROR_CORRUPT_VOLUME : result;

    char *filename_buffer = (char *)alloca(directory_entry.name_length + 1);
    if ((result = fif_file_read(mount, directory_file, filename_buffer, directory_entry.name_length)) != directory_entry.name_length)
        return (result >= 0) ? FIF_ERROR_CORRUPT_VOLUME : result;

    filename_buffer[directory_entry.name_length] = '\0';
    return callback(userdata, filename_buffer);
}

int fif_enumdir(fif_mount_handle mount, const char *dirname, fif_enumdir_callback callback, void *userdata)
{
    int result;
    if (mount->trace_stream != NULL && (result = fif_trace_write_enumdir(mount, dirname)) != FIF_ERROR_SUCCESS)
        return result;

    // resolve the directory name as a file
    fif_inode_index_t directory_inode_index;
    if ((result = fif_resolve_file_name(mount, dirname, &directory_inode_index, NULL)) != FIF_ERROR_SUCCESS)
        return result;

    // open the directory inode as a file
    fif_file_handle directory_file;
    if ((result = fif_open_file_by_inode(mount, directory_inode_index, FIF_OPEN_MODE_READ | FIF_OPEN_MODE_DIRECTORY, &directory_file)) != FIF_ERROR_SUCCESS)
        return result;

    // read the directory header
    FIF_VOLUME_FORMAT_DIRECTORY_HEADER directory_header;
    if (fif_file_read(mount, directory_file, &directory_header, sizeof(directory_header)) != sizeof(directory_header))
    {
        fif_file_close(mount, directory_file);
        return (result >= 0) ? FIF_ERROR_CORRUPT_VOLUME : result;
    }

    // validate header
    if (directory_header.magic != FIF_VOLUME_FORMAT_DIRECTORY_HEADER_MAGIC)
    {
        fif_file_close(mount, directory_file);
        return FIF_ERROR_CORRUPT_VOLUME;
    }

    // if the directory is empty, bail out early
    if (directory_header.file_count == 0)
    {
        fif_file_close(mount, directory_file);
        return FIF_ERROR_SUCCESS;
    }

    // loop through each entry
    unsigned int file_count = directory_header.file_count;
    for (unsigned int i = 0; i < file_count; i++)
    {
        if ((result = read_entry_and_invoke_callback(mount, directory_file, callback, userdata)) != 0)
        {
            fif_file_close(mount, directory_file);
            return result;
        }
    }

    // close file
    fif_file_close(mount, directory_file);
    return FIF_ERROR_SUCCESS;
}

int fif_mkdir(fif_mount_handle mount, const char *dirname)
{
    int result;
    if (mount->trace_stream != NULL && (result = fif_trace_write_mkdir(mount, dirname)) != FIF_ERROR_SUCCESS)
        return result;

    // fix up path
    int dirname_length = (int)strlen(dirname);
    char *dirname_copy = (char *)alloca(dirname_length + 1);
    fif_canonicalize_path(dirname_copy, dirname_length + 1, dirname);

    // split to dirname + basename
    char *real_dirname, *real_basename;
    fif_split_path_dirbase(dirname_copy, &real_dirname, &real_basename);

    // find the containing directory
    fif_inode_index_t containing_inode;
    if (real_dirname == NULL)
        containing_inode = mount->root_inode;
    else if ((result = fif_resolve_directory_name(mount, real_dirname, &containing_inode)) != FIF_ERROR_SUCCESS)
        return result;

    // check something doesn't exist already
    if ((result = fif_find_file_in_directory(mount, containing_inode, real_basename, NULL, NULL)) != FIF_ERROR_FILE_NOT_FOUND)
        return (result == FIF_ERROR_SUCCESS) ? FIF_ERROR_ALREADY_EXISTS : result;

    // create the directory inode near the containing directory
    fif_inode_index_t new_inode;
    if ((result = fif_create_directory(mount, containing_inode, &new_inode)) != FIF_ERROR_SUCCESS)
        return result;

    // add it to the directory
    if ((result = fif_add_file_to_directory(mount, containing_inode, real_basename, new_inode)) != FIF_ERROR_SUCCESS)
    {
        fif_free_inode(mount, new_inode);
        return result;
    }

    // done
    return FIF_ERROR_SUCCESS;
}

int fif_rmdir(fif_mount_handle mount, const char *dirname)
{
    int result;
    if (mount->trace_stream != NULL && (result = fif_trace_write_rmdir(mount, dirname)) != FIF_ERROR_SUCCESS)
        return result;

    // fix up path
    int dirname_length = (int)strlen(dirname);
    char *dirname_copy = (char *)alloca(dirname_length + 1);
    fif_canonicalize_path(dirname_copy, dirname_length + 1, dirname);

    // split to dirname + basename
    char *real_dirname, *real_basename;
    fif_split_path_dirbase(dirname_copy, &real_dirname, &real_basename);

    // find the containing directory
    fif_inode_index_t containing_inode;
    if (real_dirname == NULL)
        containing_inode = mount->root_inode;
    else if ((result = fif_resolve_directory_name(mount, real_dirname, &containing_inode)) != FIF_ERROR_SUCCESS)
        return result;

    // find the directory
    fif_inode_index_t directory_inode_index;
    if ((result = fif_find_file_in_directory(mount, containing_inode, real_basename, &directory_inode_index, NULL)) != FIF_ERROR_SUCCESS)
        return result;

    // open the directory inode as a file
    fif_file_handle directory_file;
    if ((result = fif_open_file_by_inode(mount, directory_inode_index, FIF_OPEN_MODE_READ | FIF_OPEN_MODE_WRITE | FIF_OPEN_MODE_STREAMED | FIF_OPEN_MODE_DIRECTORY, &directory_file)) != FIF_ERROR_SUCCESS)
        return result;

    // read the directory header
    FIF_VOLUME_FORMAT_DIRECTORY_HEADER directory_header;
    if (fif_file_read(mount, directory_file, &directory_header, sizeof(directory_header)) != sizeof(directory_header))
    {
        fif_file_close(mount, directory_file);
        return (result >= 0) ? FIF_ERROR_CORRUPT_VOLUME : result;
    }

    // validate header
    if (directory_header.magic != FIF_VOLUME_FORMAT_DIRECTORY_HEADER_MAGIC)
    {
        fif_file_close(mount, directory_file);
        return FIF_ERROR_CORRUPT_VOLUME;
    }

    // check there are no files
    if (directory_header.file_count > 0)
    {
        fif_file_close(mount, directory_file);
        return FIF_ERROR_DIRECTORY_NOT_EMPTY;
    }

    // remove it from the containing directory
    if ((result = fif_remove_file_from_directory(mount, containing_inode, real_basename)) != FIF_ERROR_SUCCESS)
    {
        fif_file_close(mount, directory_file);
        return result;
    }

    // if this is the last reference, free the directory inode itself
    if (directory_file->inode.reference_count == 1)
    {
        fif_free_file_blocks(mount, directory_inode_index, &directory_file->inode);
        fif_file_close(mount, directory_file);
        return fif_free_inode(mount, directory_inode_index);
    }
    else
    {
        directory_file->inode.reference_count--;
        result = fif_write_inode(mount, directory_inode_index, &directory_file->inode);
        fif_file_close(mount, directory_file);
        return result;
    }
}


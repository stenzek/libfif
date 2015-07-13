#include "fif_internal.h"
#include "trace_stream.h"

static void finalize_mount_structure(fif_mount_handle mount)
{
    mount->inodes_per_table = mount->block_size / sizeof(FIF_VOLUME_FORMAT_INODE);
}

static void free_mount_structure(fif_mount_handle mount)
{
    free(mount);
}

int fif_volume_write_descriptor(fif_mount_handle mount)
{
    // build the volume header
    FIF_VOLUME_FORMAT_HEADER volume_header;
    volume_header.magic = FIF_VOLUME_FORMAT_HEADER_MAGIC;
    volume_header.version = 1;
    volume_header.block_size = mount->block_size;
    volume_header.block_count = mount->block_count;
    volume_header.smallfile_size = mount->smallfile_size;
    volume_header.hash_table_size = mount->hash_table_size;
    volume_header.inode_table_count = mount->inode_table_count;
    volume_header.free_block_count = mount->free_block_count;
    volume_header.free_inode_count = mount->free_inode_count;
    volume_header.first_inode_table_block = mount->first_inode_table_block;
    volume_header.last_inode_table_block = mount->last_inode_table_block;
    volume_header.first_free_inode = mount->first_free_inode;
    volume_header.last_free_inode = mount->last_free_inode;
    volume_header.first_free_block = mount->first_free_block;
    volume_header.last_free_block = mount->last_free_block;
    volume_header.root_inode = mount->root_inode;

    // seek and write it
    if (mount->io.io_seek(mount->io.userdata, 0, FIF_SEEK_MODE_SET) != 0 ||
        mount->io.io_write(mount->io.userdata, &volume_header, sizeof(volume_header)) != sizeof(volume_header))
    {
        // failed to write or seek
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_volume_write_descriptor: failed to write volume descriptor");
        return FIF_ERROR_IO_ERROR;
    }

    // done
    return FIF_ERROR_SUCCESS;
}

void fif_set_default_volume_options(fif_volume_options *options)
{
    options->block_size = 1024;
    options->smallfile_size = 64;
    options->hash_table_size = 512;
    options->inode_table_count = 4;
}

void fif_set_default_mount_options(fif_mount_options *options)
{
    options->block_cache_size = 0;
    options->mount_read_only = false;
    options->new_file_compression_algorithm = FIF_COMPRESSION_ALGORITHM_NONE;
    options->new_file_compression_level = 0;
    options->fragmentation_threshold = 128;
}

int fif_create_volume(fif_mount_handle *out_mount_handle, const fif_io *io, fif_log_callback log_callback, const fif_volume_options *archive_options, const fif_mount_options *mount_options)
{
    // todo validate options
    int result;

    // create the mount structure
    struct fif_mount_s *mount = (struct fif_mount_s *)malloc(sizeof(struct fif_mount_s));
    if (mount == NULL)
    {
        // failed to allocate memory :/ what sort of potato would fail at this?
        return FIF_ERROR_OUT_OF_MEMORY;
    }

    // fill the mount structure
    memcpy(&mount->io, io, sizeof(mount->io));
    mount->log_callback = log_callback;
    mount->read_only = mount_options->mount_read_only;
    mount->error_state = 0;
    mount->block_cache_size = mount_options->block_cache_size;
    mount->new_file_compression_algorithm = mount_options->new_file_compression_algorithm;
    mount->new_file_compression_level = mount_options->new_file_compression_level;
    mount->fragmentation_threshold = mount_options->fragmentation_threshold;
    mount->block_size = archive_options->block_size;
    mount->smallfile_size = archive_options->smallfile_size;
    mount->hash_table_size = archive_options->hash_table_size;
    mount->block_count = 1;
    mount->inode_table_count = 0;
    mount->free_block_count = 0;
    mount->free_inode_count = 0;
    mount->first_inode_table_block = 0;
    mount->last_inode_table_block = 0;
    mount->first_free_inode = 0;
    mount->last_free_inode = 0;
    mount->first_free_block = 0;
    mount->last_free_block = 0;
    mount->root_inode = 0;
    mount->open_file_count = 0;
    mount->open_files = NULL;
    mount->trace_stream = NULL;

    // fill in calculated fields
    finalize_mount_structure(mount);

    // truncate the file to the block size (the first block is always the header)
    if (mount->io.io_ftruncate(mount->io.userdata, mount->block_size) != 0)
    {
        result = FIF_ERROR_INSUFFICIENT_SPACE;
        goto ERROR_LABEL;
    }

    // allocate initial inode tables
    for (unsigned int i = 0; i < archive_options->inode_table_count; i++)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "allocate inode table %u/%u...", i + 1, archive_options->inode_table_count);
        if ((result = fif_alloc_inode_table(mount, NULL)) != FIF_ERROR_SUCCESS)
            goto ERROR_LABEL;
    }

    // logging
    fif_log_msg(mount, FIF_LOG_LEVEL_DEBUG, "creating root directory...");

    // allocate the root directory
    if ((result = fif_create_directory(mount, 0, &mount->root_inode)) != FIF_ERROR_SUCCESS)
        goto ERROR_LABEL;

    // use helper methods to rewrite the header
    if ((result = fif_volume_write_descriptor(mount)) != FIF_ERROR_SUCCESS)
        goto ERROR_LABEL;

    // all done
    *out_mount_handle = mount;
    return FIF_ERROR_SUCCESS;

ERROR_LABEL:
    // cleanup the mount
    free_mount_structure(mount);
    return result;
}

int fif_mount_volume(fif_mount_handle *out_mount_handle, const fif_io *io, fif_log_callback log_callback, const fif_mount_options *mount_options)
{
    // read superblock
    FIF_VOLUME_FORMAT_HEADER header;
    if (io->io_seek(io->userdata, 0, FIF_SEEK_MODE_SET) != 0 ||
        io->io_read(io->userdata, &header, sizeof(header)) != sizeof(header))
    {
        return FIF_ERROR_IO_ERROR;
    }

    // check the header magic
    if (header.magic != FIF_VOLUME_FORMAT_HEADER_MAGIC)
        return FIF_ERROR_CORRUPT_VOLUME;

    // create the mount structure
    struct fif_mount_s *mount = (struct fif_mount_s *)malloc(sizeof(struct fif_mount_s));
    if (mount == NULL)
        return FIF_ERROR_OUT_OF_MEMORY;

    // fill the mount structure
    memcpy(&mount->io, io, sizeof(mount->io));
    mount->log_callback = log_callback;
    mount->read_only = mount_options->mount_read_only;
    mount->error_state = 0;
    mount->block_cache_size = mount_options->block_cache_size;
    mount->new_file_compression_algorithm = mount_options->new_file_compression_algorithm;
    mount->new_file_compression_level = mount_options->new_file_compression_level;
    mount->fragmentation_threshold = mount_options->fragmentation_threshold;
    mount->block_size = header.block_size;
    mount->smallfile_size = header.smallfile_size;
    mount->hash_table_size = header.hash_table_size;
    mount->block_count = header.block_count;
    mount->inode_table_count = header.inode_table_count;
    mount->free_block_count = header.free_block_count;
    mount->free_inode_count = header.free_inode_count;
    mount->first_inode_table_block = header.first_inode_table_block;
    mount->last_inode_table_block = header.last_inode_table_block;
    mount->first_free_inode = header.first_free_inode;
    mount->last_free_inode = header.last_free_inode;
    mount->first_free_block = header.first_free_block;
    mount->last_free_block = header.last_free_block;
    mount->root_inode = header.root_inode;
    mount->open_file_count = 0;
    mount->open_files = NULL;
    mount->trace_stream = NULL;

    // fill in calculated fields
    finalize_mount_structure(mount);

    // done
    *out_mount_handle = mount;
    return FIF_ERROR_SUCCESS;
}

void fif_get_mount_options(fif_mount_handle mount, fif_mount_options *options)
{
    (void)mount;
    (void)options;
}

void fif_get_volume_options(fif_mount_handle mount, fif_volume_options *options)
{
    (void)mount;
    (void)options;
}

int fif_unmount_volume(fif_mount_handle mount)
{
    // cleanup the mount
    fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "fif_unmount_volume:");
    fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "  block_count = %u", mount->block_count);
    fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "  free_block_count = %u", mount->free_block_count);
    fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "  free_inode_count = %u", mount->free_inode_count);

    // close the trace stream
    if (mount->trace_stream != NULL)
    {
        trace_stream_writer_finish(mount->trace_stream);
        mount->trace_stream = NULL;
    }

    free_mount_structure(mount);
    return FIF_ERROR_SUCCESS;
}

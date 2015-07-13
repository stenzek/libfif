/**
 * fif_internal.h
 */

#ifndef __FIF_INTERNAL_H
#define __FIF_INTERNAL_H

// from c library
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <malloc.h>
#include <limits.h>

// from our library
#include "libfif/fif.h"
#include "libfif/fif_types.h"
#include "fif_format.h"

struct fif_mount_s
{
    fif_io io;
    fif_log_callback log_callback;
    unsigned int read_only;
    unsigned int error_state;

    // mount options
    unsigned int block_cache_size;
    unsigned int new_file_compression_algorithm;
    unsigned int new_file_compression_level;
    unsigned int fragmentation_threshold;

    // info from superblock
    unsigned int block_size;
    unsigned int smallfile_size;
    unsigned int hash_table_size;
    unsigned int block_count;
    unsigned int inode_table_count;
    unsigned int free_block_count;
    unsigned int free_inode_count;
    fif_inode_index_t first_inode_table_block;
    fif_inode_index_t last_inode_table_block;
    fif_inode_index_t first_free_inode;
    fif_inode_index_t last_free_inode;
    fif_block_index_t first_free_block;
    fif_block_index_t last_free_block;
    fif_inode_index_t root_inode;

    // calculated helper fields
    fif_inode_index_t inodes_per_table;

    // currently open files
    fif_file_handle *open_files;
    unsigned int open_file_count;

    // trace stream (if enabled)
    struct fif_trace_stream *trace_stream;
};

struct fif_open_file_s
{
    fif_inode_index_t inode_index;
    FIF_VOLUME_FORMAT_INODE inode;

    unsigned int handle_index;
    unsigned int open_mode;
    unsigned int current_offset;
    unsigned int file_size;

    // when using buffered input
    unsigned char *buffer_data;
    unsigned int buffer_size;
    unsigned int buffer_range_start;
    unsigned int buffer_range_size;
    bool buffer_dirty;

    // compressor/decompressor
    const struct fif_compressor_functions *compressor;
    void *compressor_data;
    const struct fif_decompressor_functions *decompressor;
    void *decompressor_data;
};

// compressor/decompressor function prototypes
typedef int(*fif_compressor_init)(fif_mount_handle mount, fif_inode_index_t inode_index, FIF_VOLUME_FORMAT_INODE *inode, void **out_compressor_data, int compression_level);
typedef int(*fif_compressor_write)(fif_mount_handle mount, fif_inode_index_t inode_index, FIF_VOLUME_FORMAT_INODE *inode, void *compressor_data, unsigned int offset, const void *buffer, unsigned int bytes);
typedef int(*fif_compressor_end)(fif_mount_handle mount, fif_inode_index_t inode_index, FIF_VOLUME_FORMAT_INODE *inode, void *compressor_data);
typedef int(*fif_compressor_cleanup)(fif_mount_handle mount, void *compressor_data);
typedef int(*fif_decompressor_init)(fif_mount_handle mount, fif_inode_index_t inode_index, FIF_VOLUME_FORMAT_INODE *inode, void **out_decompressor_data);
typedef int(*fif_decompressor_read)(fif_mount_handle mount, fif_inode_index_t inode_index, FIF_VOLUME_FORMAT_INODE *inode, void *decompressor_data, unsigned int offset, void *buffer, unsigned int bytes);
typedef int(*fif_decompressor_skip)(fif_mount_handle mount, fif_inode_index_t inode_index, FIF_VOLUME_FORMAT_INODE *inode, void *decompressor_data, unsigned int count);
typedef int(*fif_decompressor_cleanup)(fif_mount_handle mount, void *decompressor_data);

// compressor/decompressor libraries
struct fif_compressor_functions
{
    fif_compressor_init compressor_init;
    fif_compressor_write compressor_write;
    fif_compressor_end compressor_end;
    fif_compressor_cleanup compressor_cleanup;
};
struct fif_decompressor_functions
{
    fif_decompressor_init decompressor_init;
    fif_decompressor_read decompressor_read;
    fif_decompressor_skip decompressor_skip;
    fif_decompressor_cleanup decompressor_cleanup;
};

// builtin filters
const struct fif_compressor_functions *fif_get_compressor_functions(enum FIF_COMPRESSION_ALGORITHM compression_algorithm);
const struct fif_decompressor_functions *fif_get_decompressor_functions(enum FIF_COMPRESSION_ALGORITHM compression_algorithm);

// logging
void fif_log_msg(fif_mount_handle mount, enum FIF_LOG_LEVEL level, const char *msg);
void fif_log_fmt(fif_mount_handle mount, enum FIF_LOG_LEVEL level, const char *format, ...);

// utilities
uint64_t fif_current_timestamp();
int fif_split_path(char *path);
char *fif_path_next_part(char *path);
bool fif_path_next_part_ptr(char *start, int length, char **current);
bool fif_canonicalize_path(char *dest, int dest_size, const char *path);
void fif_split_path_dirbase(char *path, char **dirname, char **basename);

// header rewriting
int fif_volume_write_descriptor(fif_mount_handle mount);

// block i/o
int fif_volume_read_block(fif_mount_handle mount, fif_block_index_t block_index, unsigned int block_offset, void *buffer, unsigned int bytes);
int fif_volume_write_block(fif_mount_handle mount, fif_block_index_t block_index, unsigned int block_offset, const void *buffer, unsigned int bytes);
int fif_volume_copy_blocks(fif_mount_handle mount, fif_block_index_t src_block_index, fif_block_index_t dst_block_index, unsigned int block_count);
int fif_volume_zero_blocks(fif_mount_handle mount, fif_block_index_t first_block_index, unsigned int block_count);
int fif_volume_zero_block_partial(fif_mount_handle mount, fif_block_index_t block_index, unsigned int offset, unsigned int bytes);
int fif_volume_resize(fif_mount_handle mount, fif_block_index_t new_block_count);

// block allocator
int fif_volume_add_freeblock(fif_mount_handle mount, fif_block_index_t block_index, unsigned int block_count);
int fif_volume_remove_freeblock(fif_mount_handle mount, fif_block_index_t block_index, fif_block_index_t prevblock_index);
int fif_volume_shrink_freeblock(fif_mount_handle mount, fif_block_index_t block_index, fif_block_index_t new_block_count, fif_block_index_t prevblock_index);
int fif_volume_alloc_blocks(fif_mount_handle mount, fif_block_index_t block_hint, unsigned int block_count, fif_block_index_t *block_index);
int fif_volume_free_blocks(fif_mount_handle mount, fif_block_index_t first_block_index, unsigned int block_count);
int fif_volume_resize_block_range(fif_mount_handle mount, fif_block_index_t first_block_index, unsigned int current_block_count, unsigned int new_block_count, fif_block_index_t *new_block_index);

// inode table allocator
int fif_alloc_inode_table(fif_mount_handle mount, fif_block_index_t *inode_table_block_index);

// inode reader/writer
int fif_read_inode(fif_mount_handle mount, fif_inode_index_t inode_index, FIF_VOLUME_FORMAT_INODE *inode);
int fif_write_inode(fif_mount_handle mount, fif_inode_index_t inode_index, const FIF_VOLUME_FORMAT_INODE *inode);

// inode allocator
int fif_alloc_inode(fif_mount_handle mount, fif_inode_index_t inode_hint, fif_inode_index_t *inode_index);
int fif_free_inode(fif_mount_handle mount, fif_inode_index_t inode_index);

// file low-level block access
int fif_create_file(fif_mount_handle mount, const char *filename, fif_inode_index_t directory_inode, fif_inode_index_t *out_inode_index);
int fif_resize_file(fif_mount_handle mount, fif_inode_index_t inode_index, FIF_VOLUME_FORMAT_INODE *inode, unsigned int new_size);
int fif_free_file_blocks(fif_mount_handle mount, fif_inode_index_t inode_index, FIF_VOLUME_FORMAT_INODE *inode);

// file data reader/writer
int fif_read_file_data(fif_mount_handle mount, fif_inode_index_t inode_index, const FIF_VOLUME_FORMAT_INODE *inode, unsigned int offset, void *buffer, unsigned int bytes);
int fif_write_file_data(fif_mount_handle mount, fif_inode_index_t inode_index, FIF_VOLUME_FORMAT_INODE *inode, unsigned int offset, const void *buffer, unsigned int bytes);

// file reading/writing
fif_offset_t fif_file_seek(fif_mount_handle mount, fif_file_handle file, fif_offset_t offset, enum FIF_SEEK_MODE mode);
int fif_file_read(fif_mount_handle mount, fif_file_handle file, void *out_buffer, unsigned int count);
int fif_file_write(fif_mount_handle mount, fif_file_handle file, const void *in_buffer, unsigned int count);
int fif_file_truncate(fif_mount_handle mount, fif_file_handle file, fif_offset_t size);
int fif_file_close(fif_mount_handle mount, fif_file_handle file);

// file opener by inode
int fif_resolve_file_name(fif_mount_handle mount, const char *path, fif_inode_index_t *out_inode_index, fif_inode_index_t *out_directory_inode_index);
int fif_open_file_by_inode(fif_mount_handle mount, fif_inode_index_t inode_index, unsigned int mode, fif_file_handle *handle);

// directory low-level access
int fif_create_directory(fif_mount_handle mount, fif_inode_index_t inode_hint, fif_inode_index_t *out_directory_inode_index);
int fif_resolve_directory_name(fif_mount_handle mount, const char *dirname, fif_inode_index_t *directory_inode_index);
int fif_find_file_in_directory(fif_mount_handle mount, fif_inode_index_t directory_inode_index, const char *filename, fif_inode_index_t *file_inode_index, unsigned int *file_index_in_directory);
int fif_add_file_to_directory(fif_mount_handle mount, fif_inode_index_t directory_inode_index, const char *filename, fif_inode_index_t fileinode);
int fif_remove_file_from_directory(fif_mount_handle mount, fif_inode_index_t directory_inode_index, const char *filename);

#endif      // __FIF_INTERNAL_H

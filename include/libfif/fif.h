/**
 * fif.h
 */
#ifndef __FIF_H
#define __FIF_H

// required includes
#include "libfif/fif_types.h"

// needs the io library
#include "libfif/fif_io.h"

// Logging callbacks, optional
typedef void(*fif_log_callback)(enum FIF_LOG_LEVEL level, const char *message);

// Mount handle
typedef struct fif_mount_s *fif_mount_handle;
typedef struct fif_open_dir_s *fif_dir_handle;
typedef struct fif_open_file_s *fif_file_handle;

// Mount options
typedef struct 
{
    unsigned int block_cache_size;
    unsigned int mount_read_only;
    unsigned int new_file_compression_algorithm;
    unsigned int new_file_compression_level;
    unsigned int fragmentation_threshold;
} fif_mount_options;

// archive options
typedef struct 
{
    unsigned int block_size;
    unsigned int smallfile_size;
    unsigned int hash_table_size;
    unsigned int inode_table_count;
} fif_volume_options;

// Mount operations
LIBFIF_API void fif_set_default_volume_options(fif_volume_options *options);
LIBFIF_API void fif_set_default_mount_options(fif_mount_options *options);
LIBFIF_API int fif_create_volume(fif_mount_handle *out_mount_handle, const fif_io *io, fif_log_callback log_callback, const fif_volume_options *archive_options, const fif_mount_options *mount_options);
LIBFIF_API int fif_mount_volume(fif_mount_handle *out_mount_handle, const fif_io *io, fif_log_callback log_callback, const fif_mount_options *mount_options);
LIBFIF_API void fif_get_mount_options(fif_mount_handle mount, fif_mount_options *options);
LIBFIF_API void fif_get_volume_options(fif_mount_handle mount, fif_volume_options *options);
LIBFIF_API void fif_set_log_callback(fif_mount_handle mount, fif_log_callback log_callback);
LIBFIF_API int fif_unmount_volume(fif_mount_handle mount);

// trace exports
LIBFIF_API int fif_trace_create_volume(fif_mount_handle *out_mount_handle, const fif_io *io, fif_log_callback log_callback, const fif_volume_options *archive_options, const fif_mount_options *mount_options, const fif_io *trace_io);
LIBFIF_API int fif_trace_mount_volume(fif_mount_handle *out_mount_handle, const fif_io *io, fif_log_callback log_callback, const fif_mount_options *mount_options, const fif_io *trace_io);
LIBFIF_API int fif_trace_replay(fif_mount_handle mount, const fif_io *tracefile_io);

// File operations
LIBFIF_API int fif_stat(fif_mount_handle mount, const char *path, fif_fileinfo *fileinfo);
LIBFIF_API int fif_fstat(fif_mount_handle mount, fif_file_handle file, fif_fileinfo *fileinfo);
LIBFIF_API int fif_open(fif_mount_handle mount, const char *path, unsigned int mode, fif_file_handle *file);
LIBFIF_API int fif_read(fif_mount_handle mount, fif_file_handle file, void *out_buffer, unsigned int count);
LIBFIF_API int fif_write(fif_mount_handle mount, fif_file_handle file, const void *in_buffer, unsigned int count);
LIBFIF_API fif_offset_t fif_seek(fif_mount_handle mount, fif_file_handle file, fif_offset_t offset, enum FIF_SEEK_MODE mode);
LIBFIF_API int fif_tell(fif_mount_handle mount, fif_file_handle file);
LIBFIF_API int fif_ftruncate(fif_mount_handle mount, fif_file_handle file, fif_offset_t size);
LIBFIF_API int fif_close(fif_mount_handle mount, fif_file_handle file);
LIBFIF_API int fif_unlink(fif_mount_handle mount, const char *filename);

// Whole-file operations
LIBFIF_API int fif_get_file_contents(fif_mount_handle mount, const char *filename, void *buffer, unsigned int max_count);
LIBFIF_API int fif_put_file_contents(fif_mount_handle mount, const char *filename, const void *buffer, unsigned int count);
LIBFIF_API int fif_compress_file(fif_mount_handle mount, const char *filename, enum FIF_COMPRESSION_ALGORITHM new_compression_algorithm, unsigned int new_compression_level);

// Directory operations
typedef int(*fif_enumdir_callback)(void *userdata, const char *filename);
LIBFIF_API int fif_enumdir(fif_mount_handle mount, const char *dirname, fif_enumdir_callback callback, void *userdata);
LIBFIF_API int fif_mkdir(fif_mount_handle mount, const char *dirname);
LIBFIF_API int fif_rmdir(fif_mount_handle mount, const char *dirname);

#endif      // __FIF_H

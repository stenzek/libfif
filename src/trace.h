#ifndef __FIF_TRACE_H
#define __FIF_TRACE_H

#include "libfif/fif.h"

// trace packets
int fif_trace_write_stat(fif_mount_handle mount, const char *path);
int fif_trace_write_fstat(fif_mount_handle mount, fif_file_handle file);
int fif_trace_write_open(fif_mount_handle mount, const char *path, unsigned int mode);
int fif_trace_write_read(fif_mount_handle mount, fif_file_handle file, unsigned int count);
int fif_trace_write_write(fif_mount_handle mount, fif_file_handle file, const void *in_buffer, unsigned int count);
int fif_trace_write_seek(fif_mount_handle mount, fif_file_handle file, fif_offset_t offset, enum FIF_SEEK_MODE mode);
int fif_trace_write_tell(fif_mount_handle mount, fif_file_handle file);
int fif_trace_write_ftruncate(fif_mount_handle mount, fif_file_handle file, fif_offset_t size);
int fif_trace_write_close(fif_mount_handle mount, fif_file_handle file);
int fif_trace_write_unlink(fif_mount_handle mount, const char *filename);

// Whole-file operations
int fif_trace_write_get_file_contents(fif_mount_handle mount, const char *filename, unsigned int max_count);
int fif_trace_write_put_file_contents(fif_mount_handle mount, const char *filename, const void *buffer, unsigned int count);
int fif_trace_write_compress_file(fif_mount_handle mount, const char *filename, enum FIF_COMPRESSION_ALGORITHM new_compression_algorithm, unsigned int new_compression_level);

// Directory operations
int fif_trace_write_enumdir(fif_mount_handle mount, const char *dirname);
int fif_trace_write_mkdir(fif_mount_handle mount, const char *dirname);
int fif_trace_write_rmdir(fif_mount_handle mount, const char *dirname);

#endif      // __FIF_TRACE_H
/**
 * fif.h
 */
#ifndef __FIF_TYPES_H
#define __FIF_TYPES_H

// required includes
#include <stdbool.h>
#include <stdint.h>

// api declarations
#ifdef __cplusplus
    #define LIBFIF_LINKAGE extern "C"
#else
    #define LIBFIF_LINKAGE 
#endif
#ifdef _WIN32
    #ifdef LIBFIF_EXPORTS
        #define LIBFIF_API LIBFIF_LINKAGE __declspec(dllexport)
    #else
        #define LIBFIF_API LIBFIF_LINKAGE __declspec(dllimport)
    #endif
#else
    #define LIBFIF_API LIBFIF_LINKAGE
#endif

// basic types
typedef unsigned int fif_block_index_t;
typedef unsigned int fif_inode_index_t;

// offset type
typedef long long fif_offset_t;

// Logging callbacks, optional
enum FIF_LOG_LEVEL
{
    FIF_LOG_LEVEL_ERROR,
    FIF_LOG_LEVEL_WARNING,
    FIF_LOG_LEVEL_INFO,
    FIF_LOG_LEVEL_DEBUG
};

// Generic errors
enum FIF_ERROR
{
    FIF_ERROR_SUCCESS                   = 0,
    FIF_ERROR_GENERIC_ERROR             = -1,
    FIF_ERROR_BAD_PATH                  = -2,
    FIF_ERROR_FILE_NOT_FOUND            = -3,
    FIF_ERROR_END_OF_FILE               = -4,
    FIF_ERROR_NO_MORE_FILES             = -5,
    FIF_ERROR_BAD_OFFSET                = -6,
    FIF_ERROR_DIRECTORY_NOT_EMPTY       = -7,
    FIF_ERROR_ALREADY_EXISTS            = -8,
    FIF_ERROR_IO_ERROR                  = -9,
    FIF_ERROR_OUT_OF_MEMORY             = -10,
    FIF_ERROR_READ_ONLY                 = -11,
    FIF_ERROR_CORRUPT_VOLUME            = -12,
    FIF_ERROR_INSUFFICIENT_SPACE        = -13,
    FIF_ERROR_SHARING_VIOLATION         = -14,
    FIF_ERROR_COMPRESSOR_NOT_FOUND      = -15,
    FIF_ERROR_COMPRESSOR_ERROR          = -16,
};

// file open mode
enum FIF_OPEN_MODE
{
    FIF_OPEN_MODE_CREATE                = (1 << 0),
    FIF_OPEN_MODE_READ                  = (1 << 1),
    FIF_OPEN_MODE_WRITE                 = (1 << 2),
    FIF_OPEN_MODE_TRUNCATE              = (1 << 3),
    FIF_OPEN_MODE_APPEND                = (1 << 4),
    FIF_OPEN_MODE_DIRECTORY             = (1 << 5),
    FIF_OPEN_MODE_STREAMED              = (1 << 6),
    FIF_OPEN_MODE_DIRECT                = (1 << 7),
    FIF_OPEN_MODE_FULLY_BUFFERED        = (1 << 8),
    FIF_OPEN_MODE_ATOMIC_REWRITE        = (1 << 9)
};

// file seek mode
enum FIF_SEEK_MODE
{
    FIF_SEEK_MODE_SET                   = 0,
    FIF_SEEK_MODE_CUR                   = 1,
    FIF_SEEK_MODE_END                   = 2,
};

// file attributes
enum FIF_FILE_ATTRIBUTE
{
    FIF_FILE_ATTRIBUTE_FREE_INODE       = (1 << 0),
    FIF_FILE_ATTRIBUTE_FILE             = (1 << 1),
    FIF_FILE_ATTRIBUTE_DIRECTORY        = (1 << 2),
    FIF_FILE_ATTRIBUTE_SMALL_FILE       = (1 << 3),
    FIF_FILE_ATTRIBUTE_COMPRESSED       = (1 << 4),
    FIF_FILE_ATTRIBUTE_FRAGMENTED       = (1 << 5),
};

// file compression algorithm
enum FIF_COMPRESSION_ALGORITHM
{
    FIF_COMPRESSION_ALGORITHM_NONE      = 0,
    FIF_COMPRESSION_ALGORITHM_ZLIB      = 1,
    FIF_COMPRESSION_ALGORITHM_LZMA      = 2,
};

// file compression level
enum FIF_COMPRESSION_LEVEL
{
    FIF_COMPRESSION_LEVEL_LOWEST        = 1,
    FIF_COMPRESSION_LEVEL_HIGHEST       = 9,
};

// stat result
typedef struct
{
    unsigned int attributes;
    unsigned int block_count;
    unsigned int compression_algorithm;
    unsigned int compression_level;
    unsigned int data_size;
    unsigned int size;
    unsigned int checksum;
    uint64_t creation_timestamp;
    uint64_t modify_timestamp;
} fif_fileinfo;

#endif      // __FIF_TYPES_H

/**
 * fif_format.h
 */

#ifndef __FIF_FORMAT_H
#define __FIF_FORMAT_H

#include <stdint.h>

// magic values
#define FIF_VOLUME_FORMAT_HEADER_MAGIC (0x11223344U)
#define FIF_VOLUME_FORMAT_INODE_TABLE_HEADER_MAGIC (0x44556677U)
#define FIF_VOLUME_FORMAT_DIRECTORY_HEADER_MAGIC (0x77889900U)
#define FIF_VOLUME_FORMAT_FRAGMENTATION_HEADER_MAGIC (0x00AABBCCU)
#define FIF_VOLUME_FORMAT_FREEBLOCK_HEADER_MAGIC (0xCCDDEEFFU)

#pragma pack(push, 1)

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t block_size;
    uint32_t block_count;
    uint32_t smallfile_size;
    uint32_t hash_table_size;
    uint32_t inode_table_count;
    uint32_t free_block_count;
    uint32_t free_inode_count;
    uint32_t first_inode_table_block;
    uint32_t last_inode_table_block;
    uint32_t first_free_inode;
    uint32_t last_free_inode;
    uint32_t first_free_block;
    uint32_t last_free_block;
    uint32_t root_inode;
} FIF_VOLUME_FORMAT_HEADER;

typedef struct
{
    uint64_t creation_timestamp;
    uint64_t modification_timestamp;
    uint32_t attributes;
    uint32_t reference_count;
    uint32_t next_entry;
    uint32_t compression_algorithm;
    uint32_t compression_level;
    uint32_t uncompressed_size;
    uint32_t data_size;
    uint32_t checksum;
    uint32_t first_block_index;
    uint32_t block_count;
    unsigned char __padding[8];
} FIF_VOLUME_FORMAT_INODE;

typedef struct
{
    uint32_t magic;
    uint32_t file_count;
    uint32_t max_filename_length;
    uint32_t first_file_inode;
    uint32_t last_file_inode;
} FIF_VOLUME_FORMAT_DIRECTORY_HEADER;

typedef struct
{
    uint32_t name_length;
    uint32_t inode_index;
} FIF_VOLUME_FORMAT_DIRECTORY_ENTRY;

typedef struct
{
    uint32_t magic;
    uint32_t fragment_count;
} FIF_VOLUME_FORMAT_FRAGMENTATION_HEADER;

typedef struct
{
    uint32_t magic;
    uint32_t block_count;
    uint32_t next_free_block;
} FIF_VOLUME_FORMAT_FREEBLOCK_HEADER;

typedef struct
{
    uint32_t magic;
    uint32_t smallfile_size;
} FIF_VOLUME_FORMAT_SMALLFILE_HEADER;

#pragma pack(pop)

#endif          // __FIF_FORMAT_H

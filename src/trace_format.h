#ifndef __FIF_TRACE_FORMAT_H
#define __FIF_TRACE_FORMAT_H

#include "libfif/fif.h"

enum FIF_TRACE_COMMAND
{
    FIF_TRACE_COMMAND_STAT,
    FIF_TRACE_COMMAND_FSTAT,
    FIF_TRACE_COMMAND_OPEN,
    FIF_TRACE_COMMAND_READ,
    FIF_TRACE_COMMAND_WRITE,
    FIF_TRACE_COMMAND_SEEK,
    FIF_TRACE_COMMAND_TELL,
    FIF_TRACE_COMMAND_FTRUNCATE,
    FIF_TRACE_COMMAND_CLOSE,
    FIF_TRACE_COMMAND_UNLINK,
    FIF_TRACE_COMMAND_GET_FILE_CONTENTS,
    FIF_TRACE_COMMAND_PUT_FILE_CONTENTS,
    FIF_TRACE_COMMAND_COMPRESS_FILE,
    FIF_TRACE_COMMAND_ENUMDIR,
    FIF_TRACE_COMMAND_MKDIR,
    FIF_TRACE_COMMAND_RMDIR
};
/*
#pragma pack(push, 1)

struct fif_trace_command_header
{
    unsigned int command;
    unsigned int size;
};

struct fif_trace_command_stat
{
    fif_trace_command_header header;
    unsigned int path_length;
    const char path[];
};

struct fif_trace_command_fstat
{
    fif_trace_command_header header;
    unsigned int file_handle_index;
};

struct fif_trace_command_open
{
    fif_trace_command_header header;
    unsigned int result_file_handle_index;
};

#pragma pack(pop)
*/
#endif      // __FIF_TRACE_FORMAT_H

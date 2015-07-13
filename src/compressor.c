#include "fif_internal.h"

extern const struct fif_compressor_functions *fif_zlib_compressor_functions();
extern const struct fif_decompressor_functions *fif_zlib_decompressor_functions();

const struct fif_compressor_functions *fif_get_compressor_functions(enum FIF_COMPRESSION_ALGORITHM compression_algorithm)
{
    switch (compression_algorithm)
    {
    case FIF_COMPRESSION_ALGORITHM_ZLIB:
        return fif_zlib_compressor_functions();

    case FIF_COMPRESSION_ALGORITHM_NONE:
        return NULL;

    case FIF_COMPRESSION_ALGORITHM_LZMA:
        return NULL;
    }

    return NULL;
}

const struct fif_decompressor_functions *fif_get_decompressor_functions(enum FIF_COMPRESSION_ALGORITHM compression_algorithm)
{
    switch (compression_algorithm)
    {
    case FIF_COMPRESSION_ALGORITHM_ZLIB:
        return fif_zlib_decompressor_functions();

    case FIF_COMPRESSION_ALGORITHM_NONE:
        return NULL;

    case FIF_COMPRESSION_ALGORITHM_LZMA:
        return NULL;
    }

    return NULL;
}


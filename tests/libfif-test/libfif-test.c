#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libfif/fif.h"

int main(int argc, char *argv[])
{
    int result;

    // archive and mount options
    fif_volume_options volume_options;
    fif_mount_options mount_options;
    fif_set_default_volume_options(&volume_options);
    fif_set_default_mount_options(&mount_options);
    mount_options.new_file_compression_algorithm = FIF_COMPRESSION_ALGORITHM_ZLIB;
    mount_options.new_file_compression_level = 6;

    // create io
    fif_io test_file_io;
    if ((result = fif_io_open_local_file("test.fif", FIF_OPEN_MODE_CREATE | FIF_OPEN_MODE_READ | FIF_OPEN_MODE_WRITE | FIF_OPEN_MODE_TRUNCATE, &test_file_io)) != FIF_ERROR_SUCCESS)
    {
        printf("fif_io_open_local_file() failed: %i\n", result);
        return -1;
    }

    // create archive
    fif_mount_handle mount;
    if ((result = fif_create_volume(&mount, &test_file_io, NULL, &volume_options, &mount_options)) != FIF_ERROR_SUCCESS)
    {
        printf("fif_create() failed: %i\n", result);
        return -1;
    }

    // create a file and write "hello" to it
    fif_file_handle file;
    if ((result = fif_open(mount, "test.txt", FIF_OPEN_MODE_CREATE | FIF_OPEN_MODE_TRUNCATE | FIF_OPEN_MODE_WRITE | FIF_OPEN_MODE_STREAMED, &file)) != FIF_ERROR_SUCCESS)
    {
        printf("fif_open() for write failed: %i", result);
        return -1;
    }
    if ((result = fif_write(mount, file, "hello", 6)) != 6)
    {
        printf("fif_write() failed: %i", result);
        return -1;
    }
    if ((result = fif_close(mount, file)) != FIF_ERROR_SUCCESS)
    {
        printf("fif_close() failed: %i", result);
        return -1;
    }

    char temp[6];
    if ((result = fif_open(mount, "test.txt", FIF_OPEN_MODE_READ | FIF_OPEN_MODE_STREAMED, &file)) != FIF_ERROR_SUCCESS)
    {
        printf("fif_open() for read failed: %i", result);
        return -1;
    }
    if ((result = fif_read(mount, file, temp, 6)) != 6)
    {
        printf("fif_read() failed: %i", result);
        return -1;
    }
    if ((result = fif_close(mount, file)) != FIF_ERROR_SUCCESS)
    {
        printf("fif_close() failed: %i", result);
        return -1;
    }

    // close archive
    fif_unmount_volume(mount);

    // close file
    fif_io_close_local_file(&test_file_io);
    return 0;
}

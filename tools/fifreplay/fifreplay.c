#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libfif/fif.h>

#define CHECK_ARG(str) (!strcmp(argv[i], str))
#define CHECK_ARG_PARAM(str) (!strcmp(argv[i], str) && ((i + 1) < argc))

static void usage(const char *progname)
{
    fprintf(stderr, "usage: %s <-v volume> <-t tracefile> [-c] [-b blocksize]\n", progname);
    fprintf(stderr, "       [-calg compression algorithm] [-clevel compression level]\n");
    exit(-1);
}

int main(int argc, char *argv[])
{
    const char *volume_filename = NULL;
    const char *trace_filename = NULL;
    int create_volume = 0;
    int create_volume_blocksize = 1024;
    int create_volume_compression_algorithm = FIF_COMPRESSION_ALGORITHM_NONE;
    int create_volume_compression_level = 0;

    // print usage if no args
    if (argc == 1)
        usage(argv[0]);

    // parse args
    for (int i = 1; i < argc; i++)
    {
        if (CHECK_ARG_PARAM("-v"))
            volume_filename = argv[++i];
        else if (CHECK_ARG_PARAM("-t"))
            trace_filename = argv[++i];
        else if (CHECK_ARG("-c"))
            create_volume = 1;
        else if (CHECK_ARG_PARAM("-b"))
            create_volume_blocksize = atoi(argv[++i]);
        else if (CHECK_ARG_PARAM("-calg"))
            create_volume_compression_algorithm = atoi(argv[++i]);
        else if (CHECK_ARG_PARAM("-clevel"))
            create_volume_compression_level = atoi(argv[++i]);
        else
            usage(argv[0]);
    }

    // check filenames
    int result;
    if (volume_filename == NULL || trace_filename == NULL)
        usage(argv[0]);

    // open the tracefile
    fif_io tracefile_io;
    if ((result = fif_io_open_local_file(trace_filename, FIF_OPEN_MODE_READ | FIF_OPEN_MODE_STREAMED, &tracefile_io)) != FIF_ERROR_SUCCESS)
    {
        fprintf(stderr, "failed to open tracefile '%s': %i\n", trace_filename, result);
        return -1;
    }

    // set mount options
    fif_mount_options mount_options;
    fif_set_default_mount_options(&mount_options);
    mount_options.new_file_compression_algorithm = create_volume_compression_algorithm;
    mount_options.new_file_compression_level = create_volume_compression_level;

    // mount or create the volume
    fif_io volumefile_io;
    fif_mount_handle mount_handle;
    if (create_volume)
    {
        if ((result = fif_io_open_local_file(volume_filename, FIF_OPEN_MODE_CREATE | FIF_OPEN_MODE_READ | FIF_OPEN_MODE_WRITE | FIF_OPEN_MODE_TRUNCATE, &volumefile_io)) != FIF_ERROR_SUCCESS)
        {
            fprintf(stderr, "failed to open volumefile '%s': %i\n", volume_filename, result);
            fif_io_close_local_file(&tracefile_io);
            return -1;
        }

        // set volume options
        fif_volume_options volume_options;
        fif_set_default_volume_options(&volume_options);
        volume_options.block_size = create_volume_blocksize;
        fprintf(stdout, "creating volume '%s'...\n", volume_filename);

        // create volume
        if ((result = fif_create_volume(&mount_handle, &volumefile_io, NULL, &volume_options, &mount_options)) != FIF_ERROR_SUCCESS)
        {
            fif_io_close_local_file(&volumefile_io);
            fif_io_close_local_file(&tracefile_io);
            fprintf(stderr, "failed to create volume '%s': %i\n", volume_filename, result);
            return -1;
        }
    }
    else
    {
        // mount volume
        if ((result = fif_io_open_local_file(volume_filename, FIF_OPEN_MODE_READ | FIF_OPEN_MODE_WRITE, &volumefile_io)) != FIF_ERROR_SUCCESS)
        {
            fprintf(stderr, "failed to open volumefile '%s': %i\n", volume_filename, result);
            fif_io_close_local_file(&tracefile_io);
            return -1;
        }

        // mount volume
        fprintf(stdout, "mounting volume '%s'...\n", volume_filename);

        // mount volume
        if ((result = fif_mount_volume(&mount_handle, &volumefile_io, NULL, &mount_options)) != FIF_ERROR_SUCCESS)
        {
            fif_io_close_local_file(&volumefile_io);
            fif_io_close_local_file(&tracefile_io);
            fprintf(stderr, "failed to mount volume '%s': %i\n", volume_filename, result);
            return -1;
        }
    }

    // replace the trace
    fprintf(stdout, "replaying trace...\n");
    if ((result = fif_trace_replay(mount_handle, &tracefile_io)) != FIF_ERROR_SUCCESS)
    {
        fprintf(stderr, "failed to replay tracefile '%s': %i\n", trace_filename, result);
        fif_unmount_volume(mount_handle);
        fif_io_close_local_file(&volumefile_io);
        fif_io_close_local_file(&tracefile_io);
        return -1;
    }

    // unmount the volume
    fprintf(stdout, "unmounting volume...\n");
    fif_unmount_volume(mount_handle);
    fif_io_close_local_file(&volumefile_io);
    fif_io_close_local_file(&tracefile_io);
    return 0;
}

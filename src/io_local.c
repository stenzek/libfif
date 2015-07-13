#include "libfif/fif_io.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _MSC_VER
    #define WIN32_LEAN_AND_MEAN 1
    #include <Windows.h>
    #include <io.h>
    #include <share.h>
#else
    #include <unistd.h>
#endif

static int io_local_read(fif_io_userdata userdata, void *buffer, unsigned int count)
{
    int fd = (int)userdata;
#ifdef _MSC_VER
    int bytes = (int)_read(fd, buffer, count);
#else
    int bytes = (int)read(fd, buffer, count);
#endif
    return bytes;
}

static int io_local_write(fif_io_userdata userdata, const void *buffer, unsigned int count)
{
    int fd = (int)userdata;
#ifdef _MSC_VER
    int bytes = (int)_write(fd, buffer, count);
#else
    int bytes = (int)write(fd, buffer, count);
#endif

    return bytes;
}

static int64_t io_local_seek(fif_io_userdata userdata, fif_offset_t offset, enum FIF_SEEK_MODE mode)
{
    int fd = (int)userdata;

    // translate seek mode
    int os_mode = FIF_SEEK_MODE_SET;
    if (mode == FIF_SEEK_MODE_CUR)
        os_mode = FIF_SEEK_MODE_CUR;
    else if (mode == FIF_SEEK_MODE_END)
        os_mode = FIF_SEEK_MODE_END;

#ifdef _MSC_VER
    return (uint64_t)_lseeki64(fd, offset, os_mode);
#else    
    return (uint64_t)lseek(fd, offset, os_mode);
#endif
}

static int io_local_zero(fif_io_userdata userdata, fif_offset_t offset, unsigned int count)
{
    static char zero_bytes[128] = { 0 };
    uint64_t current_offset = io_local_seek(userdata, 0, FIF_SEEK_MODE_CUR);

    if (io_local_seek(userdata, offset, FIF_SEEK_MODE_SET) != offset)
    {
        io_local_seek(userdata, current_offset, FIF_SEEK_MODE_SET);
        return -1;
    }

    unsigned int remaining = count;
    while (remaining > 0)
    {
        unsigned int count_to_write = (remaining > sizeof(zero_bytes)) ? sizeof(zero_bytes) : remaining;
        unsigned int written_pass = io_local_write(userdata, zero_bytes, count_to_write);
        if (written_pass != count_to_write)
            return count - remaining - ((written_pass > 0) ? (int)written_pass : 0);

        remaining -= written_pass;
    }

    return (int)(count - remaining);
}

static int io_local_ftruncate(fif_io_userdata userdata, fif_offset_t newsize)
{
#ifdef _MSC_VER
    int fd = (int)userdata;
    HANDLE hFile = (HANDLE)_get_osfhandle(fd);
    LARGE_INTEGER move_position;
    LARGE_INTEGER new_position;
    move_position.QuadPart = newsize;
    if (!SetFilePointerEx(hFile, move_position, &new_position, FILE_BEGIN))
        return -1;
    if (!SetEndOfFile(hFile))
        return -1;

    return 0;
#else
    int fd = (int)userdata;
    return ftruncate(fd, newsize);
#endif
}

static int64_t io_local_filesize(fif_io_userdata userdata)
{
    int fd = (int)userdata;

#ifdef _MSC_VER
    /*struct __stat64 info;
    if (_fstat64(fd, &info) != 0)
        return 0;

    return (uint64_t)info.st_size;*/
    HANDLE hFile = (HANDLE)_get_osfhandle(fd);
    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(hFile, &file_size))
        return 0;

    return (uint64_t)file_size.QuadPart;
#else
    struct stat info;
    if (fstat(fd, &info) != 0)
        return 0;

    return (uint64_t)info.st_size;
#endif
}

int fif_io_open_local_file(const char *path, unsigned int mode, fif_io *out_io)
{
    // convert mode
    int os_mode = 0;
    if (mode & FIF_OPEN_MODE_CREATE)
        os_mode |= O_CREAT;

    if ((mode & (FIF_OPEN_MODE_READ | FIF_OPEN_MODE_WRITE)) == (FIF_OPEN_MODE_READ | FIF_OPEN_MODE_WRITE))
        os_mode |= O_RDWR;
    else if (mode & FIF_OPEN_MODE_READ)
        os_mode |= O_RDONLY;
    else if (mode & FIF_OPEN_MODE_WRITE)
        os_mode |= O_WRONLY;

    if (mode & FIF_OPEN_MODE_TRUNCATE)
        os_mode |= O_TRUNC;

#ifdef _MSC_VER
    int fd;
    errno_t err = _sopen_s(&fd, path, os_mode | _O_BINARY, _SH_DENYRW, _S_IREAD | _S_IWRITE);
    if (err != 0)
        return -1;
#else
    int fd = open(path, os_mode);
    if (fd < 0)
        return -1;
#endif

    out_io->io_read = io_local_read;
    out_io->io_write = io_local_write;
    out_io->io_seek = io_local_seek;
    out_io->io_zero = io_local_zero;
    out_io->io_ftruncate = io_local_ftruncate;
    out_io->io_filesize = io_local_filesize;
    out_io->userdata = (fif_io_userdata)fd;
    return 0;
}

int fif_io_close_local_file(const fif_io *io)
{
    int fd = (int)io->userdata;
#ifdef _MSC_VER
    if (_close(fd) < 0)
        return -1;
#else
    if (close(fd) < 0)
        return -1;
#endif

    return 0;
}

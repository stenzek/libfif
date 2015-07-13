#include "fif_internal.h"
#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>

void fif_log_msg(fif_mount_handle mount, enum FIF_LOG_LEVEL level, const char *msg)
{
    // if there's no log callback, just print it to stderr
    if (mount->log_callback == NULL)
    {
        fputs(msg, stderr);
        fputc('\n', stderr);
    }
    else
    {
        mount->log_callback(level, msg);
    }
}

void fif_log_fmt(fif_mount_handle mount, enum FIF_LOG_LEVEL level, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    // if there's no log callback, just print it to stderr
    if (mount->log_callback == NULL)
    {
        vfprintf(stderr, format, ap);
        fputc('\n', stderr);
    }
    else
    {
#ifdef _MSC_VER
        int length = _vscprintf(format, ap);
        if (length > 0)
        {
            // allocate a buffer, print it, and pass through
            char *buffer = (char *)alloca(length + 1);
            vsnprintf_s(buffer, length + 1, _TRUNCATE, format, ap);
            buffer[length] = '\0';
            mount->log_callback(level, buffer);
        }
#else
        // calculate the length first
        int length = vsnprintf(NULL, 0, format, ap);
        if (length > 0)
        {
            // allocate a buffer, print it, and pass through
            char *buffer = (char *)alloca(length + 1);
            vsnprintf(buffer, length + 1, format, ap);
            buffer[length] = '\0';
            mount->log_callback(level, buffer);
        }
#endif
    }

    va_end(ap);
}

void fif_set_log_callback(fif_mount_handle mount, fif_log_callback log_callback)
{
    mount->log_callback = log_callback;
}

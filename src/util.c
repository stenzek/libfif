#include "fif_internal.h"

uint64_t fif_current_timestamp()
{
    return (uint64_t)time(NULL);
}

int fif_split_path(char *path)
{
    char *ptr = path;
    int count = 0;
    for (; *ptr != '\0'; ptr++)
    {
        if (*ptr == '/')
        {
            *ptr = 0;
            count++;
        }
    }

    return count;
}

char *fif_path_next_part(char *path)
{
    char *ptr = path;
    while (*ptr != '\0')
        ptr++;

    return ptr + 1;
}

bool fif_path_next_part_ptr(char *start, int length, char **current)
{
    char *search = *current;
    while (search < (start + length) && search != '\0')
        search++;

    if ((search + 1) < (start + length))
    {
        *current = search + 1;
        return true;
    }
    else
    {
        return false;
    }
}

void fif_split_path_dirbase(char *path, char **dirname, char **basename)
{
    // remove leading slash
    if (path[0] == '/')
        path++;

    // find the rightmost slash
    char *rightmost_slash = strrchr(path, '/');
    if (rightmost_slash == NULL)
    {
        *dirname = NULL;
        *basename = path;
    }
    else
    {
        // remove the rightmost slash, and set pointers
        *rightmost_slash = '\0';
        *dirname = path;
        *basename = rightmost_slash;
    }
}

bool fif_canonicalize_path(char *dest, int dest_size, const char *path)
{
    int path_length = (int)strlen(path);
    if (path_length == 0)
        return false;

    // handle cases where the path is just '/'
    if (path_length == 1 && path[0] == '/')
    {
#ifdef _MSC_VER
        strncpy_s(dest, dest_size, "/", _TRUNCATE);
#else
        strncpy(dest, "/", dest_size);
#endif
        return false;
    }

    // remove leading /
    if (path[0] == '/')
    {
        path++;
        path_length--;
    }

    // iterate path
    int dest_length = 0;
    for (int i = 0; i < path_length;)
    {
        char prev_ch = (i > 0) ? path[i - 1] : '\0';
        char current_ch = path[i];
        char next_ch = (i < path_length) ? path[i + 1] : '\0';

        if (current_ch == '.')
        {
            if (prev_ch == '\\' || prev_ch == '/' || prev_ch == '\0')
            {
                // handle '.'
                if (next_ch == '\\' || next_ch == '/' || next_ch == '\0')
                {
                    // skip '.\'
                    i++;

                    // remove the previous \, if we have one trailing the dot it'll append it anyway
                    if (dest_length > 0)
                        dest[--dest_length] = '\0';

                    continue;
                }
                // handle '..'
                else if (next_ch == '.')
                {
                    char afterNext = ((i + 1) < path_length) ? path[i + 2] : '\0';
                    if (afterNext == '\\' || afterNext == '/' || afterNext == '\0')
                    {
                        // remove one directory of the path, including the /.
                        if (dest_length > 1)
                        {
                            int j;
                            for (j = dest_length - 2; j > 0; j--)
                            {
                                if (dest[j] == '\\' || dest[j] == '/')
                                    break;
                            }

                            dest_length = j;
                            dest[dest_length] = '\0';
                        }

                        // skip the dot segment
                        i += 2;
                        continue;
                    }
                }
            }
        }

        // copy character
        if (dest_length < dest_size)
        {
            dest[dest_length++] = current_ch;
            dest[dest_length] = '\0';
        }
        else
            break;

        // increment position by one
        i++;
    }

    // ensure null termination
    if (dest_length < dest_size)
        dest[dest_length] = '\0';
    else
        dest[dest_length - 1] = '\0';

    return true;
}


# libfif #

Small virtual filesystem-in-a-file library. Also can be in memory.
Fairly unoptimized at this point, but performance is still decent enough for not-so-large archives.

### Features ###

* dynamic archive size - start small, expand as data is added
* files/directories in archive
* enumeration of files in directories
* zlib compression of file contents
* buffered reads/writes of files

### What's not done or ideas ###

* find files based on mask
* indexing, lookups are linear and slow
* inode caching
* fine-grained threading - all reads/writes/opens have to hold the same lock
* sharing violations - opening the same file twice will work, but undefined as to what it does
* "small files" - multiple files packed into one block
* hash table of filenames for fast lookups

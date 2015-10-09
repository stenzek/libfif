#include "fif_internal.h"

/**
  * block i/o
  */

int fif_volume_read_block(fif_mount_handle mount, fif_block_index_t block_index, unsigned int block_offset, void *buffer, unsigned int bytes)
{
    // some sanity checks, we can't write past a block, or over a block boundary
    if ((block_offset + bytes) > mount->block_size)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_volume_read_block: attempt to read past a block boundary (block:%u,offset:%u,bytes:%u)", block_index, block_offset, bytes);
        return FIF_ERROR_BAD_OFFSET;
    }

    // check the block count
    if (block_index >= mount->block_count)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_volume_read_block: attempt to read out-of-range block %u", block_index);
        return FIF_ERROR_BAD_OFFSET;
    }

    fif_offset_t file_offset = (fif_offset_t)mount->block_size * (fif_offset_t)block_index + (fif_offset_t)block_offset;
    if (mount->io.io_seek(mount->io.userdata, file_offset, FIF_SEEK_MODE_SET) != file_offset)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_volume_read_block: failed to seek to file offset %u for block %u offset %u", (unsigned int)file_offset, block_index, block_offset);
        return FIF_ERROR_BAD_OFFSET;
    }

    if ((unsigned int)mount->io.io_read(mount->io.userdata, buffer, bytes) != bytes)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_volume_read_block: failed to read %u bytes from block %u offset %u", bytes, block_index, block_offset);
        return FIF_ERROR_IO_ERROR;
    }

    return FIF_ERROR_SUCCESS;
}

int fif_volume_write_block(fif_mount_handle mount, fif_block_index_t block_index, unsigned int block_offset, const void *buffer, unsigned int bytes)
{
    // some sanity checks, we can't write past a block, or over a block boundary
    if ((block_offset + bytes) > mount->block_size)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_volume_write_block: attempt to write past a block boundary (block:%u,offset:%u,bytes:%u)", block_index, block_offset, bytes);
        return FIF_ERROR_BAD_OFFSET;
    }

    // check the block count
    if (block_index >= mount->block_count)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_volume_write_block: attempt to write out-of-range block %u", block_index);
        return FIF_ERROR_BAD_OFFSET;
    }

    int64_t file_offset = (int64_t)mount->block_size * (int64_t)block_index + (int64_t)block_offset;
    if (mount->io.io_seek(mount->io.userdata, file_offset, FIF_SEEK_MODE_SET) != file_offset)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_volume_write_block: failed to seek to file offset %u for block %u offset %u", (unsigned int)file_offset, block_index, block_offset);
        return FIF_ERROR_BAD_OFFSET;
    }

    if ((unsigned int)mount->io.io_write(mount->io.userdata, buffer, bytes) != bytes)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_volume_write_block: failed to write %u bytes to block %u offset %u", bytes, block_index, block_offset);
        return FIF_ERROR_IO_ERROR;
    }

    return FIF_ERROR_SUCCESS;
}

int fif_volume_copy_blocks(fif_mount_handle mount, fif_block_index_t src_block_index, fif_block_index_t dst_block_index, unsigned int block_count)
{
    // this can be optimized in the future...
    void *buffer = malloc(mount->block_size);
    if (buffer == NULL)
        return FIF_ERROR_OUT_OF_MEMORY;

    for (unsigned int i = 0; i < block_count; i++)
    {
        int result;

        if ((result = fif_volume_read_block(mount, src_block_index, 0, buffer, mount->block_size)) != FIF_ERROR_SUCCESS)
            return result;

        if ((result = fif_volume_write_block(mount, dst_block_index, 0, buffer, mount->block_size)) != FIF_ERROR_SUCCESS)
            return result;

        src_block_index++;
        dst_block_index++;
    }

    free(buffer);
    return FIF_ERROR_SUCCESS;
}

int fif_volume_zero_blocks(fif_mount_handle mount, fif_block_index_t first_block_index, unsigned int block_count)
{
    uint64_t file_offset = (uint64_t)mount->block_size * (uint64_t)first_block_index;
    unsigned int zero_count = mount->block_size * block_count;

    if (mount->io.io_zero(mount->io.userdata, file_offset, zero_count) != (int)zero_count)
        return FIF_ERROR_IO_ERROR;

    return FIF_ERROR_SUCCESS;
}

int fif_volume_zero_block_partial(fif_mount_handle mount, fif_block_index_t block_index, unsigned int offset, unsigned int bytes)
{
    // some sanity checks, we can't write past a block, or over a block boundary
    if ((offset + bytes) > mount->block_size)
        return FIF_ERROR_BAD_OFFSET;

    int64_t file_offset = (int64_t)mount->block_size * (int64_t)block_index + (int64_t)offset;

    if (mount->io.io_zero(mount->io.userdata, file_offset, bytes) != (int)bytes)
        return FIF_ERROR_IO_ERROR;

    return FIF_ERROR_SUCCESS;
}

int fif_volume_resize(fif_mount_handle mount, fif_block_index_t new_block_count)
{
    int result;
    if (mount->error_state)
        return FIF_ERROR_CORRUPT_VOLUME;

    // check here
    assert(new_block_count > mount->block_count);
    
    int64_t new_archive_size = (int64_t)mount->block_size * (int64_t)new_block_count;
    if ((result = mount->io.io_ftruncate(mount->io.userdata, new_archive_size)) != FIF_ERROR_SUCCESS)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_volume_resize: failed to extend archive from %u blocks to %u blocks", mount->block_count, new_block_count);
        return result;
    }

    // update the mount info
    mount->block_count = new_block_count;
    if ((result = fif_volume_write_descriptor(mount)) != FIF_ERROR_SUCCESS)
        return result;

    // done
    return FIF_ERROR_SUCCESS;
}

/**
 * block allocator
 */

int fif_volume_add_freeblock(fif_mount_handle mount, fif_block_index_t block_index, unsigned int block_count)
{
    int result;
    if (mount->error_state)
        return FIF_ERROR_CORRUPT_VOLUME;

    //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "fif_volume_add_freeblock(%u, %u -> %u)", block_index, block_count, block_index + block_count - 1);
    //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "  first_free_block = %u, last_free_block = %u", mount->first_free_block, mount->last_free_block);

    // can we merge these (now-free) blocks with another freeblock?
    fif_block_index_t prev_free_block = 0;
    fif_block_index_t this_free_block = mount->first_free_block;
    FIF_VOLUME_FORMAT_FREEBLOCK_HEADER this_free_block_header;
    FIF_VOLUME_FORMAT_FREEBLOCK_HEADER new_free_block_header;
    while (this_free_block != 0)
    {
        if ((result = fif_volume_read_block(mount, this_free_block, 0, &this_free_block_header, sizeof(this_free_block_header))) != FIF_ERROR_SUCCESS)
            return result;

        //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "    read freeblock %u :: %u, %u", this_free_block, this_free_block_header.block_count, this_free_block_header.next_free_block);

        // sanity check
        if (this_free_block_header.magic != FIF_VOLUME_FORMAT_FREEBLOCK_HEADER_MAGIC)
        {
            fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_volume_add_freeblock: freeblock %u is corrupt", this_free_block);
            return FIF_ERROR_CORRUPT_VOLUME;
        }

        // does this freeblock end on our starting point?
        if ((this_free_block + this_free_block_header.block_count) == block_index)
        {
            //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "  extend freeblock %u from %u to %u (next %u)", this_free_block, this_free_block_header.block_count, this_free_block_header.block_count + block_count, this_free_block_header.next_free_block);

            // does this freeblock now reach the next block?
            if ((this_free_block + this_free_block_header.block_count + block_count) == this_free_block_header.next_free_block)
            {
                // read the next block
                fif_block_index_t next_free_block = this_free_block_header.next_free_block;
                FIF_VOLUME_FORMAT_FREEBLOCK_HEADER next_free_block_header;
                if ((result = fif_volume_read_block(mount, next_free_block, 0, &next_free_block_header, sizeof(next_free_block_header))) != FIF_ERROR_SUCCESS)
                    return result;

                // sanity check it
                if (next_free_block_header.magic != FIF_VOLUME_FORMAT_FREEBLOCK_HEADER_MAGIC)
                {
                    fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_volume_add_freeblock: freeblock %u is corrupt", next_free_block);
                    return FIF_ERROR_CORRUPT_VOLUME;
                }

                // we can merge the blocks
                //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "    merging freeblock %u -> %u (new block %u -> %u) and %u -> %u", this_free_block, this_free_block + this_free_block_header.block_count - 1, block_index, block_index + block_count - 1, next_free_block, next_free_block + next_free_block_header.block_count - 1);
                
                // zero the next freeblock
                if ((result = fif_volume_zero_block_partial(mount, next_free_block, 0, sizeof(FIF_VOLUME_FORMAT_FREEBLOCK_HEADER))) != FIF_ERROR_SUCCESS)
                {
                    mount->error_state = 1;
                    return result;
                }

                // update this block to point to the initial block, the stuff we're freeing now, and the next block
                this_free_block_header.block_count += block_count + next_free_block_header.block_count;
                this_free_block_header.next_free_block = next_free_block_header.next_free_block;
                if (fif_volume_write_block(mount, this_free_block, 0, &this_free_block_header, sizeof(this_free_block_header)) != FIF_ERROR_SUCCESS)
                {
                    mount->error_state = 1;
                    return result;
                }

                // is the next block the last one? have to update the last pointer if that's the case
                if (this_free_block_header.next_free_block == 0)
                {
                    if (mount->last_free_block != next_free_block)
                    {
                        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_volume_add_freeblock: superblock has incorrect last block pointer (%u, should be %u)", mount->last_free_block, next_free_block);
                        mount->error_state = 1;
                        return FIF_ERROR_CORRUPT_VOLUME;
                    }
                    
                    mount->last_free_block = this_free_block;
                }
            }
            else
            {
                // simple, extend this freeblock
                this_free_block_header.block_count += block_count;
                if (fif_volume_write_block(mount, this_free_block, 0, &this_free_block_header, sizeof(this_free_block_header)) != FIF_ERROR_SUCCESS)
                {
                    mount->error_state = 1;
                    return result;
                }
            }

            // extended, so no more work to do
            break;
        }

        // does this free block fall on our ending point?
        if ((block_index + block_count) == this_free_block)
        {
            //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "  retract freeblock %u to %u", this_free_block, block_index);

            // update the block count, and write the retracted block in the new location
            this_free_block_header.block_count += block_count;
            if ((result = fif_volume_write_block(mount, block_index, 0, &this_free_block_header, sizeof(this_free_block_header))) != FIF_ERROR_SUCCESS)
            {
                mount->error_state = 1;
                return result;
            }

            // update the previous to point to the new starting location (seek backwards at this point but meh it's safer in terms of corruption possibilities)
            if (prev_free_block != 0)
            {
                // read previous block
                FIF_VOLUME_FORMAT_FREEBLOCK_HEADER prev_free_block_header;
                if ((result = fif_volume_read_block(mount, prev_free_block, 0, &prev_free_block_header, sizeof(prev_free_block_header))) != FIF_ERROR_SUCCESS)
                {
                    mount->error_state = 1;
                    return result;
                }

                // update next pointer (this is actually prev here)
                if (prev_free_block_header.next_free_block != this_free_block)
                {
                    fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_volume_add_freeblock: freeblock %u has bad next_free_block (%u, expected %u)", prev_free_block, prev_free_block_header.next_free_block, this_free_block);
                    return FIF_ERROR_CORRUPT_VOLUME;
                }

                // write it
                prev_free_block_header.next_free_block = block_index;
                if ((result = fif_volume_write_block(mount, prev_free_block, 0, &prev_free_block_header, sizeof(prev_free_block_header))) != FIF_ERROR_SUCCESS)
                {
                    mount->error_state = 1;
                    return result;
                }

                // if the last block in the superblock was pointing to the (now-retracted) block, this has to be changed to the new position
                if (this_free_block_header.next_free_block == 0)
                {
                    if (mount->last_free_block != this_free_block)
                    {
                        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_volume_add_freeblock: superblock has incorrect last block pointer (%u, should be %u)", mount->last_free_block, this_free_block);
                        mount->error_state = 1;
                        return FIF_ERROR_CORRUPT_VOLUME;
                    }

                    // update the pointer
                    mount->last_free_block = block_index;
                }
            }
            else
            {
                // we were the first block in the chain, so update the superblock
                assert(mount->first_free_block == this_free_block);
                mount->first_free_block = block_index;

                // check for the last case too
                if (this_free_block_header.next_free_block == 0)
                {
                    assert(mount->last_free_block == this_free_block);
                    mount->last_free_block = block_index;
                }
            }
            
            // zero the freeblock header in the retracted location
            if ((result = fif_volume_zero_block_partial(mount, this_free_block, 0, sizeof(this_free_block_header))) != FIF_ERROR_SUCCESS)
            {
                mount->error_state = 1;
                return result;
            }

            // retracted, no more work to do
            break;
        }

        // if this free block greater than our block index, we need to insert between before this free block
        if (this_free_block > block_index)
        {
            // shouldn't be any overlap
            //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "    new freeblock %u -> %u", block_index, block_index + block_count - 1); 
            assert((block_index + block_count) < this_free_block);

            // initialize the new freeblock, and write it
            new_free_block_header.magic = FIF_VOLUME_FORMAT_FREEBLOCK_HEADER_MAGIC;
            new_free_block_header.block_count = block_count;
            new_free_block_header.next_free_block = this_free_block;
            if ((result = fif_volume_write_block(mount, block_index, 0, &new_free_block_header, sizeof(new_free_block_header))) != FIF_ERROR_SUCCESS)
            {
                mount->error_state = 1;
                return result;
            }

            // update the previous to point to the us
            if (prev_free_block != 0)
            {
                // read previous block
                if ((result = fif_volume_read_block(mount, prev_free_block, 0, &this_free_block_header, sizeof(this_free_block_header))) != FIF_ERROR_SUCCESS)
                {
                    mount->error_state = 1;
                    return result;
                }

                // update next pointer (this is actually prev here)
                if (this_free_block_header.next_free_block != this_free_block)
                {
                    fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_volume_add_freeblock: freeblock %u has bad next_free_block (%u, expected %u)", prev_free_block, this_free_block_header.next_free_block, this_free_block);
                    return FIF_ERROR_CORRUPT_VOLUME;
                }

                // write it
                this_free_block_header.next_free_block = block_index;
                if ((result = fif_volume_write_block(mount, prev_free_block, 0, &this_free_block_header, sizeof(this_free_block_header))) != FIF_ERROR_SUCCESS)
                {
                    mount->error_state = 1;
                    return result;
                }
            }
            else
            {
                // we were the first block in the chain, so update the superblock, the last can be left as the old value though
                assert(mount->first_free_block == this_free_block);
                mount->first_free_block = block_index;
            }

            // we're done now
            break;
        }

        // search the next free block
        prev_free_block = this_free_block;
        this_free_block = this_free_block_header.next_free_block;
    }

    // new ending block?
    if (this_free_block == 0)
    {
        assert(block_index > mount->last_free_block);

        // initialize the new freeblock at the end
        //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "    new freeblock %u -> %u", block_index, block_index + block_count - 1);
        new_free_block_header.magic = FIF_VOLUME_FORMAT_FREEBLOCK_HEADER_MAGIC;
        new_free_block_header.block_count = block_count;
        new_free_block_header.next_free_block = 0;
        if ((result = fif_volume_write_block(mount, block_index, 0, &new_free_block_header, sizeof(new_free_block_header))) != FIF_ERROR_SUCCESS)
        {
            mount->error_state = 1;
            return result;
        }

        // update the current last free block to point to us
        if (mount->last_free_block != 0)
        {
            if ((result = fif_volume_read_block(mount, mount->last_free_block, 0, &this_free_block_header, sizeof(this_free_block_header))) != FIF_ERROR_SUCCESS)
                return result;

            // sanity check
            if (this_free_block_header.magic != FIF_VOLUME_FORMAT_FREEBLOCK_HEADER_MAGIC)
            {
                fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_volume_add_freeblock: freeblock %u is corrupt", mount->last_free_block);
                return FIF_ERROR_CORRUPT_VOLUME;
            }

            // check that it's the last block in the chain
            if (this_free_block_header.next_free_block != 0)
            {
                fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_volume_add_freeblock: freeblock %u has bad next_free_block (%u, expected zero)", mount->last_free_block, this_free_block_header.next_free_block);
                return FIF_ERROR_CORRUPT_VOLUME;
            }

            // update it to point to us
            this_free_block_header.next_free_block = block_index;
            if ((result = fif_volume_write_block(mount, mount->last_free_block, 0, &this_free_block_header, sizeof(this_free_block_header))) != FIF_ERROR_SUCCESS)
            {
                mount->error_state = 1;
                return result;
            }

            // and update the last block pointer
            mount->last_free_block = block_index;
        }
        else
        {
            // there was no last free block, therefore there shouldn't be any first either
            assert(mount->first_free_block == 0);
            mount->first_free_block = block_index;
            mount->last_free_block = block_index;
        }
    }

    // update the header with the new free blocks
    mount->free_block_count += block_count;
    if ((result = fif_volume_write_descriptor(mount)) != FIF_ERROR_SUCCESS)
    {
        mount->error_state = 1;
        return result;
    }

    // done
    //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "  first_free_block now = %u, last_free_block now = %u", mount->first_free_block, mount->last_free_block);
    return FIF_ERROR_SUCCESS;
}

int fif_volume_remove_freeblock(fif_mount_handle mount, fif_block_index_t block_index, fif_block_index_t prevblock_index)
{
    int result;
    if (mount->error_state)
        return FIF_ERROR_CORRUPT_VOLUME;

    // read in the current free block
    FIF_VOLUME_FORMAT_FREEBLOCK_HEADER freeblock_header;
    if ((result = fif_volume_read_block(mount, block_index, 0, &freeblock_header, sizeof(freeblock_header))) != FIF_ERROR_SUCCESS)
        return result;

    // sanity check
    //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "fif_volume_remove_freeblock(%u) :: %u,%u", block_index, freeblock_header.block_count, freeblock_header.next_free_block);
    if (freeblock_header.magic != FIF_VOLUME_FORMAT_FREEBLOCK_HEADER_MAGIC)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_volume_remove_freeblock: freeblock %u is corrupt", block_index);
        return FIF_ERROR_CORRUPT_VOLUME;
    }

    // since we're removing the free block from the chain entirely, we need to update the previous block pointing to it
    if (prevblock_index != 0)
    {
        // read the previous block
        FIF_VOLUME_FORMAT_FREEBLOCK_HEADER prevblock_header;
        if ((result = fif_volume_read_block(mount, prevblock_index, 0, &prevblock_header, sizeof(prevblock_header))) != FIF_ERROR_SUCCESS)
            return result;

        // check magic
        if (prevblock_header.magic != FIF_VOLUME_FORMAT_FREEBLOCK_HEADER_MAGIC)
        {
            fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_volume_remove_freeblock: freeblock %u has incorrect magic", prevblock_index);
            return FIF_ERROR_CORRUPT_VOLUME;
        }

        // corruption check here
        if (prevblock_header.next_free_block != block_index)
        {
            fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_volume_remove_freeblock: freeblock %u has incorrect next block pointer (%u, should be %u)", prevblock_index, prevblock_header.next_free_block, block_index);
            return FIF_ERROR_CORRUPT_VOLUME;
        }

        // handle case where we're the last block in the chain, so set the last block to the previous free block
        if (freeblock_header.next_free_block == 0)
        {
            if (mount->last_free_block != block_index)
            {
                fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_volume_remove_freeblock: superblock has incorrect last block pointer (%u, should be %u)", mount->last_free_block, block_index);
                mount->error_state = 1;
                return FIF_ERROR_CORRUPT_VOLUME;
            }

            // update last block
            mount->last_free_block = prevblock_index;
        }

        // update next block
        prevblock_header.next_free_block = freeblock_header.next_free_block;
        if ((result = fif_volume_write_block(mount, prevblock_index, 0, &prevblock_header, sizeof(prevblock_header))) != FIF_ERROR_SUCCESS)
        {
            mount->error_state = 1;
            return result;
        }
    }
    else
    {
        // we're the first block in the chain, so update the superblock
        assert(mount->first_free_block == block_index);
        mount->first_free_block = freeblock_header.next_free_block;
        if (freeblock_header.next_free_block == 0)
        {
            // we're also the last block in the chain
            assert(mount->last_free_block == block_index);
            mount->last_free_block = 0;
        }
    }

    // 

    // update superblock
    assert(mount->free_block_count >= freeblock_header.block_count);
    mount->free_block_count -= freeblock_header.block_count;
    if ((result = fif_volume_write_descriptor(mount)) != FIF_ERROR_SUCCESS)
    {
        mount->error_state = 1;
        return result;
    }

    // all done
    return FIF_ERROR_SUCCESS;
}

int fif_volume_shrink_freeblock(fif_mount_handle mount, fif_block_index_t block_index, fif_block_index_t new_block_count, fif_block_index_t prevblock_index)
{
    int result;
    if (mount->error_state)
        return FIF_ERROR_CORRUPT_VOLUME;

    // read in the current free block
    FIF_VOLUME_FORMAT_FREEBLOCK_HEADER freeblock_header;
    if ((result = fif_volume_read_block(mount, block_index, 0, &freeblock_header, sizeof(freeblock_header))) != FIF_ERROR_SUCCESS)
        return result;

    // sanity check
    //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "fif_volume_shrink_freeblock(%u, %u -> %u) :: %u,%u", block_index, freeblock_header.block_count, new_block_count, freeblock_header.block_count, freeblock_header.next_free_block);
    if (freeblock_header.magic != FIF_VOLUME_FORMAT_FREEBLOCK_HEADER_MAGIC)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_volume_shrink_freeblock: freeblock %u is corrupt", block_index);
        return FIF_ERROR_CORRUPT_VOLUME;
    }

    // should always be > new_block_count
    unsigned int old_freeblock_size = freeblock_header.block_count;
    assert(old_freeblock_size > new_block_count);

    // calculate the new free block index
    unsigned int shrink_block_count = old_freeblock_size - new_block_count;
    fif_block_index_t new_free_block_index = block_index + shrink_block_count;
    //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "  shrink_block_count = %u", shrink_block_count);
    //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "  new_free_block_index = %u", new_free_block_index);

    // do we have to update the previous free block?
    if (prevblock_index != 0)
    {
        // read the previous block
        FIF_VOLUME_FORMAT_FREEBLOCK_HEADER prevblock_header;
        if ((result = fif_volume_read_block(mount, prevblock_index, 0, &prevblock_header, sizeof(prevblock_header))) != FIF_ERROR_SUCCESS)
            return result;

        // corruption check here
        if (prevblock_header.next_free_block != block_index)
        {
            fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_shrink_block: freeblock %u has incorrect next block pointer (%u, should be %u)", prevblock_index, prevblock_header.next_free_block, block_index);
            return FIF_ERROR_CORRUPT_VOLUME;
        }

        // update next block
        prevblock_header.next_free_block = new_free_block_index;
        if ((result = fif_volume_write_block(mount, prevblock_index, 0, &prevblock_header, sizeof(prevblock_header))) != FIF_ERROR_SUCCESS)
        {
            mount->error_state = 1;
            return result;
        }
    }
    else
    {
        // if it's zero, it means it's the first free block
        if (mount->first_free_block != block_index)
        {
            fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_shrink_block: superblock has incorrect first block pointer (%u, should be %u)", mount->first_free_block, block_index);
            return FIF_ERROR_CORRUPT_VOLUME;
        }

        // update the header/superblock
        mount->first_free_block = new_free_block_index;
    }

    // check if we're the ending free block in the volume
    if (freeblock_header.next_free_block == 0)
    {
        // if it's zero, it means it's the first free block
        if (mount->last_free_block != block_index)
        {
            fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_shrink_block: superblock has incorrect last block pointer (%u, should be %u)", mount->last_free_block, block_index);
            return FIF_ERROR_CORRUPT_VOLUME;
        }

        // update the header/superblock
        mount->last_free_block = new_free_block_index;
    }

    // we can mostly reuse the found block header, just modifying the count
    freeblock_header.block_count = new_block_count;
    //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "  %u %u", freeblock_header.block_count, freeblock_header.next_free_block);
    if ((result = fif_volume_write_block(mount, new_free_block_index, 0, &freeblock_header, sizeof(freeblock_header))) != FIF_ERROR_SUCCESS)
    {
        mount->error_state = 1;
        return result;
    }

    // update header
    mount->free_block_count -= shrink_block_count;
    if ((result = fif_volume_write_descriptor(mount)) != FIF_ERROR_SUCCESS)
    {
        mount->error_state = 1;
        return result;
    }

    // all done
    return FIF_ERROR_SUCCESS;
}

int fif_volume_alloc_blocks(fif_mount_handle mount, fif_block_index_t block_hint, unsigned int block_count, fif_block_index_t *block_index)
{
    int result;
    if (mount->error_state)
        return FIF_ERROR_CORRUPT_VOLUME;

    //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "fif_volume_alloc_blocks(%u, %u)", block_hint, block_count);

    // do we have any free blocks?
    if (mount->first_free_block > 0)
    {
        fif_block_index_t prev_free_block = 0;
        fif_block_index_t next_free_block = mount->first_free_block;
        fif_block_index_t found_block_prev = 0;
        fif_block_index_t found_block_index = 0;
        fif_block_index_t found_block_distance = UINT32_MAX;
        FIF_VOLUME_FORMAT_FREEBLOCK_HEADER freeblock_header;
        FIF_VOLUME_FORMAT_FREEBLOCK_HEADER found_block_header;

        // try to find a free block > block_hint if possible
        while (next_free_block != 0)
        {
            //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "  next_free_block = %u", next_free_block);
            if ((result = fif_volume_read_block(mount, next_free_block, 0, &freeblock_header, sizeof(freeblock_header))) != FIF_ERROR_SUCCESS)
                return result;

            // sanity check here
            if (freeblock_header.magic != FIF_VOLUME_FORMAT_FREEBLOCK_HEADER_MAGIC)
            {
                fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_volume_shrink_freeblock: freeblock %u is corrupt", next_free_block);
                return FIF_ERROR_CORRUPT_VOLUME;
            }

            // is there sufficient space inside this block?
            if (freeblock_header.block_count >= block_count)
            {
                // calculate distance from hint to block
                fif_block_index_t distance = (next_free_block >= block_hint) ? (next_free_block - block_hint) : (block_hint - next_free_block);

                // is this block closer?
                if (distance < found_block_distance)
                {
                    found_block_prev = prev_free_block;
                    found_block_index = next_free_block;
                    found_block_distance = distance;
                    memcpy(&found_block_header, &freeblock_header, sizeof(found_block_header));

                    // if there's no hint, use the first-found block
                    if (block_hint == 0)
                        break;
                }
            }

            // go to the next free block
            prev_free_block = next_free_block;
            next_free_block = freeblock_header.next_free_block;
        }

        // did we find a free block?
        if (found_block_index != 0)
        {
            // does this free block require splitting?
            assert(found_block_header.block_count >= block_count);
            if (found_block_header.block_count > block_count)
            {
                // shrink the free block
                if ((result = fif_volume_shrink_freeblock(mount, found_block_index, found_block_header.block_count - block_count, found_block_prev)) != FIF_ERROR_SUCCESS)
                    return result;
            }
            else
            {
                // remove the free block from the chain
                if ((result = fif_volume_remove_freeblock(mount, found_block_index, found_block_prev)) != FIF_ERROR_SUCCESS)
                    return result;
            }

            // zero the found block header
            if ((result = fif_volume_zero_block_partial(mount, found_block_index, 0, sizeof(FIF_VOLUME_FORMAT_FREEBLOCK_HEADER))) != FIF_ERROR_SUCCESS)
                return result;

            // block is now allocated!
            //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "   -> %u", found_block_index);
            *block_index = found_block_index;
            return FIF_ERROR_SUCCESS;
        }
    }

    // the current block count is the block index we're handing back
    *block_index = mount->block_count;

    // we didn't find any free blocks, thus have to extend the archive, so calculate the new archive size and truncate it
    if ((result = fif_volume_resize(mount, mount->block_count + block_count)) != FIF_ERROR_SUCCESS)
        return result;
    
    // all done
    //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "   -> %u", *block_index);
    return FIF_ERROR_SUCCESS;
}

int fif_volume_free_blocks(fif_mount_handle mount, fif_block_index_t block_index, unsigned int block_count)
{
    int result;
    assert((block_index + block_count) <= mount->block_count);
    //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "fif_volume_free_blocks(%u, %u)", block_index, block_count);

    // zero the blocks
    if ((result = fif_volume_zero_blocks(mount, block_index, block_count)) != FIF_ERROR_SUCCESS)
        return result;

    // add freeblocks
    return fif_volume_add_freeblock(mount, block_index, block_count);
}

int fif_volume_resize_block_range(fif_mount_handle mount, fif_block_index_t block_index, unsigned int current_block_count, unsigned int new_block_count, fif_block_index_t *new_block_index)
{
    int result;
    //fif_log_fmt(mount, FIF_LOG_LEVEL_DEBUG, "fif_volume_resize_block_range(%u, %u, %u)", block_index, current_block_count, new_block_count);

    // process for extending a block range
    if (new_block_count > current_block_count)
    {
        fif_block_index_t required_index = block_index + current_block_count;
        fif_block_index_t required_blocks = new_block_count - current_block_count;

        // is this block at the end of the archive?
        if (required_index == mount->block_count)
        {
            // simply extend the archive out
            if ((result = fif_volume_resize(mount, mount->block_count + required_blocks)) != FIF_ERROR_SUCCESS)
                return result;

            // the block index is not changing
            *new_block_index = block_index;
            return FIF_ERROR_SUCCESS;
        }

        // search for a free block immediately after block_index of size <= new_block_count
        fif_block_index_t prev_free_block_index = 0;
        fif_block_index_t this_free_block_index = mount->first_free_block;
        FIF_VOLUME_FORMAT_FREEBLOCK_HEADER freeblock_header;
        while (this_free_block_index != 0)
        {
            // no point searching if the index is too high
            if (this_free_block_index > required_index)
                break;

            // read the block
            if ((result = fif_volume_read_block(mount, this_free_block_index, 0, &freeblock_header, sizeof(freeblock_header))) != FIF_ERROR_SUCCESS)
                return result;

            // does this one match?
            if (this_free_block_index == required_index)
            {
                // well we're halfway there, now we need to see if it'll fit
                if (freeblock_header.block_count > required_blocks)
                {
                    // simple enough now, just shrink the freeblock the required number of blocks
                    if ((result = fif_volume_shrink_freeblock(mount, this_free_block_index, freeblock_header.block_count - required_blocks, prev_free_block_index)) != FIF_ERROR_SUCCESS)
                        return result;

                    // and return the same block index
                    *new_block_index = block_index;
                    return FIF_ERROR_SUCCESS;
                }
                else if (freeblock_header.block_count == required_blocks)
                {
                    // taking up the entire free block, so remove it from the chain
                    if ((result = fif_volume_remove_freeblock(mount, this_free_block_index, prev_free_block_index)) != FIF_ERROR_SUCCESS)
                        return result;

                    // and return the same block index
                    *new_block_index = block_index;
                    return FIF_ERROR_SUCCESS;
                }

                // it won't fit, so no point looking through any more block since they won't match the index...
                break;
            }

            // move to next
            prev_free_block_index = this_free_block_index;
            this_free_block_index = freeblock_header.next_free_block;
        }

        // allocate new blocks of new_block_count
        fif_block_index_t allocated_index;
        if ((result = fif_volume_alloc_blocks(mount, block_index, new_block_count, &allocated_index)) != FIF_ERROR_SUCCESS)
            return result;

        // copy current_block_count worth of blocks in
        if ((result = fif_volume_copy_blocks(mount, block_index, allocated_index, current_block_count)) != FIF_ERROR_SUCCESS)
            return result;

        // remove the old blocks
        if ((result = fif_volume_free_blocks(mount, block_index, current_block_count)) != FIF_ERROR_SUCCESS)
            return result;

        // all done
        *new_block_index = allocated_index;
    }
    else if (new_block_count < current_block_count)
    {
        // shrinking the block range, so just add a freeblock after new_block_count
        unsigned int freeblock_count = new_block_count - current_block_count;
        if ((result = fif_volume_free_blocks(mount, current_block_count + new_block_count, freeblock_count)) != FIF_ERROR_SUCCESS)
            return result;

        // set to the same
        *new_block_index = block_index;
    }

    // done with success, or did nothing at all
    return FIF_ERROR_SUCCESS;
}

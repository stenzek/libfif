#include "fif_internal.h"

// todo: inode table caching

static int get_inode_table_for_inode(fif_mount_handle mount, fif_block_index_t *table_block, fif_inode_index_t *table_offset, fif_inode_index_t inode_index)
{
    int result;

    // find the inode table index
    fif_inode_index_t inode_table_index = inode_index / mount->inodes_per_table;

    // search through each table until we find our table
    fif_inode_index_t current_table_index = 0;
    fif_block_index_t current_table_block = mount->first_inode_table_block;
    while (current_table_index != inode_table_index)
    {
        // read this table's first inode
        FIF_VOLUME_FORMAT_INODE first_inode;
        if ((result = fif_volume_read_block(mount, current_table_block, 0, &first_inode, sizeof(first_inode))) != FIF_ERROR_SUCCESS)
            return result;

        // it should have the internal attribute set, otherwise the archive is corrupt
        if (first_inode.attributes != 0)
        {
            fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "get_inode_table_for_inode: descriptor inode in block %u is corrupt", current_table_block);
            return FIF_ERROR_CORRUPT_VOLUME;
        }

        // is there a next table?
        if (first_inode.next_entry == 0)
        {
            // not the best, should really be 'inode not found' but file not found will do
            return FIF_ERROR_FILE_NOT_FOUND;
        }

        // update with the next table
        current_table_index++;
        current_table_block = first_inode.next_entry;
    }

    // calculate offset into table
    fif_inode_index_t table_start_offset = current_table_index * mount->inodes_per_table;
    fif_inode_index_t current_table_offset = inode_index - table_start_offset;

    // sanity check
    if (current_table_block == 0 || current_table_block >= mount->block_count ||
        current_table_offset == 0 || current_table_offset >= mount->inodes_per_table)
    {
        fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "get_inode_table_for_inode: bad block (%u) or offset (%u)", current_table_block, current_table_offset);
        return FIF_ERROR_CORRUPT_VOLUME;
    }

    // found it, so store vars
    *table_block = current_table_block;
    *table_offset = current_table_offset;
    return FIF_ERROR_SUCCESS;
}

int fif_alloc_inode_table(fif_mount_handle mount, fif_block_index_t *inode_table_block_index)
{
    int result;

    // calculate the new inode indices
    fif_inode_index_t first_new_inode_index = mount->inode_table_count * mount->inodes_per_table;

    // allocating a new block
    fif_block_index_t allocated_block_index;
    if ((result = fif_volume_alloc_blocks(mount, 0, 1, &allocated_block_index)) != FIF_ERROR_SUCCESS)
        return result;

    // allocate some scratch memory, and fill the block contents with 
    FIF_VOLUME_FORMAT_INODE *inodes = (FIF_VOLUME_FORMAT_INODE *)calloc(mount->inodes_per_table, sizeof(FIF_VOLUME_FORMAT_INODE));

    // the first inode of a table always serves as a pointer to the next table, so initialize it as such (leave the next_entry as zero, since we're the new end)
    inodes[0].creation_timestamp = inodes[0].modification_timestamp = fif_current_timestamp();
    inodes[0].attributes = 0;
    inodes[0].next_entry = 0;

    // initialize remaining inodes as free inodes
    fif_inode_index_t current_next_inode_index = first_new_inode_index + 2;
    for (unsigned int i = 1; i < mount->inodes_per_table; i++)
    {
        inodes[i].attributes = FIF_FILE_ATTRIBUTE_FREE_INODE;
        inodes[i].next_entry = current_next_inode_index;
        current_next_inode_index++;
    }

    // last inode entry should have a next_entry of zero
    inodes[mount->inodes_per_table - 1].next_entry = 0;

    // write the block
    if ((result = fif_volume_write_block(mount, allocated_block_index, 0, inodes, sizeof(FIF_VOLUME_FORMAT_INODE) * mount->inodes_per_table)) != FIF_ERROR_SUCCESS)
    {
        free(inodes);
        return result;
    }

    // no longer need the scratch memory
    free(inodes);

    // update the 'last free inode' if there is one, otherwise the header
    if (mount->last_free_inode != 0)
    {
        FIF_VOLUME_FORMAT_INODE last_inode;
        if ((result = fif_read_inode(mount, mount->last_free_inode, &last_inode)) != FIF_ERROR_SUCCESS)
            return result;

        if (last_inode.next_entry != 0)
        {
            fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_alloc_inode_table: bad next_entry for last inode %u (should be zero)", mount->last_free_inode);
            return FIF_ERROR_CORRUPT_VOLUME;
        }

        last_inode.next_entry = first_new_inode_index + 1;
        if ((result = fif_write_inode(mount, mount->last_free_inode, &last_inode)) != FIF_ERROR_SUCCESS)
            return result;

        // update header
        mount->last_free_inode = first_new_inode_index + (mount->inodes_per_table - 1);
    }
    else
    {
        // update the header
        assert(mount->first_free_inode == 0);
        mount->first_free_inode = first_new_inode_index + 1;
        mount->last_free_inode = first_new_inode_index + (mount->inodes_per_table - 1);
    }

    // update the last inode table's next inode table to be us
    if (mount->last_inode_table_block != 0)
    {
        // read the descriptor inode in that table
        FIF_VOLUME_FORMAT_INODE last_inode_table_desc;
        if ((result = fif_volume_read_block(mount, mount->last_inode_table_block, 0, &last_inode_table_desc, sizeof(last_inode_table_desc))) != FIF_ERROR_SUCCESS)
            return result;

        // should have the correct flags
        if (last_inode_table_desc.attributes != 0 || last_inode_table_desc.next_entry != 0)
        {
            fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_alloc_inode_table: first inode of table at block %u is corrupted", mount->last_inode_table_block);
            return FIF_ERROR_CORRUPT_VOLUME;
        }

        // update the next entry, and write the block
        last_inode_table_desc.next_entry = allocated_block_index;
        if ((result = fif_volume_write_block(mount, mount->last_inode_table_block, 0, &last_inode_table_desc, sizeof(last_inode_table_desc))) != FIF_ERROR_SUCCESS)
            return result;

        // update the header
        mount->last_inode_table_block = allocated_block_index;
    }
    else
    {
        // this is the first inode table in the volume (we must be creating)
        mount->first_inode_table_block = allocated_block_index;
        mount->last_inode_table_block = allocated_block_index;
    }

    // update inode table count in header
    mount->inode_table_count++;
    if ((result = fif_volume_write_descriptor(mount)) != FIF_ERROR_SUCCESS)
        return result;

    // all done
    if (inode_table_block_index != NULL)
        *inode_table_block_index = allocated_block_index;

    return FIF_ERROR_SUCCESS;
}

int fif_read_inode(fif_mount_handle mount, fif_inode_index_t inode_index, FIF_VOLUME_FORMAT_INODE *inode)
{
    int result;

    // find the inode table 
    fif_block_index_t table_block;
    fif_inode_index_t table_offset;
    if ((result = get_inode_table_for_inode(mount, &table_block, &table_offset, inode_index)) != FIF_ERROR_SUCCESS)
        return result;

    // read the inode itself
    if ((result = fif_volume_read_block(mount, table_block, table_offset * sizeof(FIF_VOLUME_FORMAT_INODE), inode, sizeof(FIF_VOLUME_FORMAT_INODE))) != FIF_ERROR_SUCCESS)
        return result;

    // done
    return FIF_ERROR_SUCCESS;
}

int fif_write_inode(fif_mount_handle mount, fif_inode_index_t inode_index, const FIF_VOLUME_FORMAT_INODE *inode)
{
    int result;

    // find the inode table 
    fif_block_index_t table_block;
    fif_inode_index_t table_offset;
    if ((result = get_inode_table_for_inode(mount, &table_block, &table_offset, inode_index)) != FIF_ERROR_SUCCESS)
        return result;

    // write the inode itself
    if ((result = fif_volume_write_block(mount, table_block, table_offset * sizeof(FIF_VOLUME_FORMAT_INODE), inode, sizeof(FIF_VOLUME_FORMAT_INODE))) != FIF_ERROR_SUCCESS)
        return result;

    // done
    return FIF_ERROR_SUCCESS;
}

int fif_alloc_inode(fif_mount_handle mount, fif_inode_index_t inode_hint, fif_inode_index_t *inode_index)
{
    int result;

    // similar algorithm to blocks, we find an inode close to inode_hint if possible
    if (mount->first_free_inode > 0)
    {
        fif_inode_index_t next_free_inode = mount->first_free_inode;
        fif_inode_index_t prev_inode_index = 0;
        fif_inode_index_t found_inode_index = 0;
        fif_inode_index_t found_inode_prev = 0;
        fif_inode_index_t found_inode_next = 0;
        fif_inode_index_t found_inode_distance = UINT32_MAX;
        FIF_VOLUME_FORMAT_INODE current_inode;

        // try to find a free inode
        while (next_free_inode != 0)
        {
            // read the actual inode out to find the next free inode
            if ((result = fif_read_inode(mount, next_free_inode, &current_inode)) != FIF_ERROR_SUCCESS)
                return result;

            // sanity check on inode data
            if (!(current_inode.attributes & FIF_FILE_ATTRIBUTE_FREE_INODE))
            {
                fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_alloc_inode: inode %u in free chain is not a free inode", next_free_inode);
                return FIF_ERROR_CORRUPT_VOLUME;
            }

            // get distance to inode
            fif_inode_index_t distance = (next_free_inode >= inode_hint) ? (next_free_inode - inode_hint) : (inode_hint - next_free_inode);

            // is this block closer?
            if (distance < found_inode_distance)
            {
                found_inode_index = next_free_inode;
                found_inode_prev = prev_inode_index;
                found_inode_next = current_inode.next_entry;
                found_inode_distance = distance;

                // if there's no hint, use the first free one
                if (inode_hint == 0)
                    break;
            }

            // set next inode
            prev_inode_index = next_free_inode;
            next_free_inode = current_inode.next_entry;
        }

        // found one?
        if (found_inode_index != 0)
        {
            // remove from the free the inode chain
            if (found_inode_prev != 0)
            {
                if ((result = fif_read_inode(mount, found_inode_prev, &current_inode)) != FIF_ERROR_SUCCESS)
                    return result;

                assert(current_inode.next_entry == found_inode_index);
                current_inode.next_entry = found_inode_next;
                if ((result = fif_write_inode(mount, found_inode_prev, &current_inode)) != FIF_ERROR_SUCCESS)
                    return result;
            }
            else
            {
                // this is the first free inode
                mount->first_free_inode = found_inode_next;
                if ((result = fif_volume_write_descriptor(mount)) != FIF_ERROR_SUCCESS)
                    return result;
            }

            // is this the last free inode?
            if (found_inode_next == 0)
            {
                assert(mount->last_free_inode == found_inode_index);
                mount->last_free_inode = 0;
                if ((result = fif_volume_write_descriptor(mount)) != FIF_ERROR_SUCCESS)
                    return result;
            }

            // return this index
            *inode_index = found_inode_index;
            return FIF_ERROR_SUCCESS;
        }
    }

    // allocate a new inode table
    if ((result = fif_alloc_inode_table(mount, NULL)) != FIF_ERROR_SUCCESS)
        return result;

    // use the first free inode (there should be one now)
    fif_inode_index_t new_inode_index = mount->first_free_inode;
    assert(new_inode_index != 0);

    // set the next free inode to be the index + 1
    mount->first_free_inode = new_inode_index + 1;
    if ((result = fif_volume_write_descriptor(mount)) != FIF_ERROR_SUCCESS)
        return result;

    // and the index
    *inode_index = new_inode_index;
    return FIF_ERROR_SUCCESS;
}

int fif_free_inode(fif_mount_handle mount, fif_inode_index_t inode_index)
{
    int result;

    // build new inode contents
    FIF_VOLUME_FORMAT_INODE free_inode;
    free_inode.attributes = FIF_FILE_ATTRIBUTE_FREE_INODE;
    memset(&free_inode, 0, sizeof(free_inode));

    // are we the new head inode? if so, we can skip all this
    if (mount->first_free_inode == 0 || inode_index < mount->first_free_inode)
    {
        free_inode.next_entry = mount->first_free_inode;
        if ((result = fif_write_inode(mount, inode_index, &free_inode)) != FIF_ERROR_SUCCESS)
            return result;

        // update header
        mount->first_free_inode = inode_index;
        if (mount->last_free_inode == 0)
            mount->last_free_inode = inode_index;
    }
    else
    {
        // find a free inode with a next free inode > inode_index
        fif_inode_index_t current_free_inode_index = mount->first_free_inode;
        FIF_VOLUME_FORMAT_INODE current_free_inode;
        while (current_free_inode_index != 0)
        {
            if ((result = fif_read_inode(mount, current_free_inode_index, &current_free_inode)) != FIF_ERROR_SUCCESS)
                return result;

            if (current_free_inode.next_entry > inode_index)
                break;

            // try the next one
            current_free_inode_index = current_free_inode.next_entry;
        }

        // any match found?
        if (current_free_inode_index == 0)
        {
            // this means no inode is appropriate to insert after, so we insert in the last place
            free_inode.next_entry = 0;
            if ((result = fif_write_inode(mount, inode_index, &free_inode)) != FIF_ERROR_SUCCESS)
                return result;

            // are we not the only inode in the chain?
            if (mount->last_free_inode != 0)
            {
                // read last inode
                if ((result = fif_read_inode(mount, mount->last_free_inode, &current_free_inode)) != FIF_ERROR_SUCCESS)
                    return result;

                // update next entry
                if (current_free_inode.next_entry != 0)
                {
                    fif_log_fmt(mount, FIF_LOG_LEVEL_ERROR, "fif_free_inode: bad next_entry for inode %u (should be zero)", mount->last_free_inode);
                    return FIF_ERROR_CORRUPT_VOLUME;
                }

                // set to us
                current_free_inode.next_entry = inode_index;
                if ((result = fif_write_inode(mount, mount->last_free_inode, &current_free_inode)) != FIF_ERROR_SUCCESS)
                    return result;

                // update the last in the superblock
                mount->last_free_inode = inode_index;
            }
            else
            {
                // we're the only (now) free inode
                assert(mount->first_free_inode == 0);
                mount->first_free_inode = inode_index;
                mount->last_free_inode = inode_index;
            }
        }
        else
        {
            // we should be inserting after current_free_inode
            assert(current_free_inode_index < inode_index);

            // insert after current_free_inode
            free_inode.next_entry = current_free_inode.next_entry;
            if ((result = fif_write_inode(mount, inode_index, &free_inode)) != FIF_ERROR_SUCCESS)
                return result;

            // update current_free_inode
            current_free_inode.next_entry = inode_index;
            if ((result = fif_write_inode(mount, current_free_inode_index, &current_free_inode)) != FIF_ERROR_SUCCESS)
                return result;

            // shouldn't be the last inode, that case should've been handled in the other branch of the if
            assert(mount->last_free_inode != current_free_inode_index);
        }
    }

    // increment free inode count
    mount->last_free_inode++;

    // commit the header
    if ((result = fif_volume_write_descriptor(mount)) != FIF_ERROR_SUCCESS)
        return result;

    // done
    return FIF_ERROR_SUCCESS;
}


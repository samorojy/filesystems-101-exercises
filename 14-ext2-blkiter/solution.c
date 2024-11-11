#include <solution.h>
#include <fs_malloc.h>
#include <ext2fs/ext2fs.h>
#include <ext2fs/ext2_fs.h>
#include <errno.h>
#include <unistd.h>

struct ext2_fs {
    int fd;
    int blkSize;
    struct ext2_super_block sb;
};

struct blk {
    int bid;
    int *b_pointer;
};

struct ext2_blkiter {
    struct ext2_fs *fs;
    int inode_table;

    struct ext2_inode inode;
    int current;

    struct blk direct;
    struct blk indirect;
};

int ext2_fs_init(struct ext2_fs **fs, int fd) {
    struct ext2_fs *file_system = fs_xmalloc(sizeof(struct ext2_fs));
    file_system->fd = fd;

    int blk_read = pread(file_system->fd, &file_system->sb, SUPERBLOCK_SIZE, SUPERBLOCK_OFFSET);
    if (blk_read == -1) {
        fs_xfree(file_system);
        return -errno;
    }
    file_system->blkSize = EXT2_BLOCK_SIZE(&file_system->sb);
    *fs = file_system;
    return 0;
}

void ext2_fs_free(struct ext2_fs *fs) {
    fs_xfree(fs);
}

int ext2_blkiter_init(struct ext2_blkiter **i, struct ext2_fs *fs, int ino) {
    int inoId = (ino - 1) % fs->sb.s_inodes_per_group;
    int groupId = (ino - 1) / fs->sb.s_inodes_per_group;

    int numBlkDesc = fs->blkSize / sizeof(struct ext2_group_desc);

    int blkno = fs->sb.s_first_data_block + 1 + groupId / numBlkDesc;
    int blkoffset = (groupId % numBlkDesc) * sizeof(struct ext2_group_desc);

    struct ext2_group_desc group_desc;
    int r_read = pread(fs->fd, &group_desc, sizeof(group_desc), blkno * fs->blkSize + blkoffset);

    if (r_read == -1) {
        return -errno;
    }
    struct ext2_blkiter *next_iter = fs_xmalloc(sizeof(struct ext2_blkiter));
    next_iter->inode_table = group_desc.bg_inode_table;

    r_read = pread(fs->fd, &next_iter->inode, sizeof(struct ext2_inode),
                next_iter->inode_table * fs->blkSize + inoId * fs->sb.s_inode_size);

    if (r_read == -1) {
        return -errno;
    }

    *i = next_iter;
    next_iter->current = 0;

    next_iter->direct.bid = 0;
    next_iter->direct.b_pointer = NULL;
    next_iter->indirect.bid = 0;
    next_iter->indirect.b_pointer = NULL;

    next_iter->fs = fs;

    return 0;
}

int get_blk(struct blk *b, int new_id, struct ext2_fs *fs) {
    if (b->b_pointer && new_id == b->bid) {
        return 0;
    }

    if (!b->b_pointer) {
        b->b_pointer = fs_xmalloc(fs->blkSize);
    }

    int ret = pread(fs->fd, b->b_pointer, fs->blkSize, new_id * fs->blkSize);
    if (ret == -1) {
        return -1;
    }

    b->bid = new_id;
    return 1;
}

int ext2_blkiter_next(struct ext2_blkiter *i, int *blkno) {
    int numBlkPtrs = i->fs->blkSize / sizeof(int);

    int direct_end = EXT2_NDIR_BLOCKS;
    int indirect_start = direct_end;
    int indirect_end = direct_end + numBlkPtrs;
    int indirect_indirect_start = indirect_end;
    int indirect_indirect_end = indirect_end + numBlkPtrs * numBlkPtrs;

    if (i->current < direct_end) {
        int ptr = i->inode.i_block[i->current];
        if (ptr == 0) {
            return 0;
        }
        *blkno = ptr;
        i->current++;

        return 1;
    }

    if (i->current < indirect_end) {
        int updated = get_blk(&i->direct, i->inode.i_block[EXT2_IND_BLOCK], i->fs);
        if (updated) {
            if (updated < 0) {
                return -errno;
            }

            *blkno = i->inode.i_block[EXT2_IND_BLOCK];
            return 1;
        }

        int indirect_pos = i->current - indirect_start;
        int ptr = i->direct.b_pointer[indirect_pos];

        if (ptr == 0) {
            return 0;
        }

        *blkno = ptr;
        i->current++;
        return 1;
    }
    if (i->current < indirect_indirect_end) {
        int updated = get_blk(&i->indirect, i->inode.i_block[EXT2_DIND_BLOCK], i->fs);
        if (updated) {
            if (updated < 0) {
                return -errno;
            }
            *blkno = i->inode.i_block[EXT2_DIND_BLOCK];
            return 1;
        }

        int indirect_indirect_pos = (i->current - indirect_indirect_start) / numBlkPtrs;
        int indirect_pos = (i->current - indirect_indirect_start) % numBlkPtrs;

        updated = get_blk(&i->direct, i->indirect.b_pointer[indirect_indirect_pos], i->fs);
        if (updated) {
            if (updated < 0) {
                return -errno;
            }
            *blkno = i->indirect.b_pointer[indirect_indirect_pos];
            return 1;
        }

        int ptr = i->direct.b_pointer[indirect_pos];
        if (ptr == 0) {
            return 0;
        }

        *blkno = ptr;
        i->current++;
        return 1;
    }

    return 0;
}

void ext2_blkiter_free(struct ext2_blkiter *i) {
    if (i != NULL) {
        fs_xfree(i->direct.b_pointer);
        fs_xfree(i->indirect.b_pointer);
    }
    fs_xfree(i);
}

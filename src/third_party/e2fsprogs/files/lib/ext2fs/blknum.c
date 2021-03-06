/*
 * blknum.c --- Functions to handle blk64_t and high/low 64-bit block
 * number.
 *
 * Copyright IBM Corporation, 2007
 * Author Jose R. Santos <jrs@us.ibm.com>
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include "ext2fs.h"

/*
 * Return the group # of a block
 */
dgrp_t ext2fs_group_of_blk2(ext2_filsys fs, blk64_t blk)
{
	return (blk - fs->super->s_first_data_block) /
		fs->super->s_blocks_per_group;
}

/*
 * Return the first block (inclusive) in a group
 */
blk64_t ext2fs_group_first_block2(ext2_filsys fs, dgrp_t group)
{
	return fs->super->s_first_data_block +
		((blk64_t)group * fs->super->s_blocks_per_group);
}

/*
 * Return the last block (inclusive) in a group
 */
blk64_t ext2fs_group_last_block2(ext2_filsys fs, dgrp_t group)
{
	return (group == fs->group_desc_count - 1 ?
		ext2fs_blocks_count(fs->super) - 1 :
		ext2fs_group_first_block2(fs, group) +
			(fs->super->s_blocks_per_group - 1));
}

/*
 * Return the inode data block count
 */
blk64_t ext2fs_inode_data_blocks2(ext2_filsys fs,
					struct ext2_inode *inode)
{
	return (inode->i_blocks |
		(fs->super->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT ?
		(__u64)inode->osd2.linux2.l_i_blocks_hi << 32 : 0)) -
		(inode->i_file_acl ? fs->blocksize >> 9 : 0);
}

/*
 * Return the inode i_blocks count
 */
blk64_t ext2fs_inode_i_blocks(ext2_filsys fs,
					struct ext2_inode *inode)
{
	return (inode->i_blocks |
		(fs->super->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT ?
		 (__u64)inode->osd2.linux2.l_i_blocks_hi << 32 : 0));
}

/*
 * Return the fs block count
 */
blk64_t ext2fs_blocks_count(struct ext2_super_block *super)
{
	return super->s_blocks_count |
		(super->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT ?
		(__u64) super->s_blocks_count_hi << 32 : 0);
}

/*
 * Set the fs block count
 */
void ext2fs_blocks_count_set(struct ext2_super_block *super, blk64_t blk)
{
	super->s_blocks_count = blk;
	if (super->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT)
		super->s_blocks_count_hi = (__u64) blk >> 32;
}

/*
 * Add to the current fs block count
 */
void ext2fs_blocks_count_add(struct ext2_super_block *super, blk64_t blk)
{
	blk64_t tmp;
	tmp = ext2fs_blocks_count(super) + blk;
	ext2fs_blocks_count_set(super, tmp);
}

/*
 * Return the fs reserved block count
 */
blk64_t ext2fs_r_blocks_count(struct ext2_super_block *super)
{
	return super->s_r_blocks_count |
		(super->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT ?
		(__u64) super->s_r_blocks_count_hi << 32 : 0);
}

/*
 * Set the fs reserved block count
 */
void ext2fs_r_blocks_count_set(struct ext2_super_block *super, blk64_t blk)
{
	super->s_r_blocks_count = blk;
	if (super->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT)
		super->s_r_blocks_count_hi = (__u64) blk >> 32;
}

/*
 * Add to the current reserved fs block count
 */
void ext2fs_r_blocks_count_add(struct ext2_super_block *super, blk64_t blk)
{
	blk64_t tmp;
	tmp = ext2fs_r_blocks_count(super) + blk;
	ext2fs_r_blocks_count_set(super, tmp);
}

/*
 * Return the fs free block count
 */
blk64_t ext2fs_free_blocks_count(struct ext2_super_block *super)
{
	return super->s_free_blocks_count |
		(super->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT ?
		(__u64) super->s_free_blocks_hi << 32 : 0);
}

/*
 * Set the fs free block count
 */
void ext2fs_free_blocks_count_set(struct ext2_super_block *super, blk64_t blk)
{
	super->s_free_blocks_count = blk;
	if (super->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT)
		super->s_free_blocks_hi = (__u64) blk >> 32;
}

/*
 * Add to the current free fs block count
 */
void ext2fs_free_blocks_count_add(struct ext2_super_block *super, blk64_t blk)
{
	blk64_t tmp;
	tmp = ext2fs_free_blocks_count(super) + blk;
	ext2fs_free_blocks_count_set(super, tmp);
}

/*
 * Get a pointer to a block group descriptor.  We need the explicit
 * pointer to the group desc for code that swaps block group
 * descriptors before writing them out, as it wants to make a copy and
 * do the swap there.
 */
struct ext2_group_desc *ext2fs_group_desc(ext2_filsys fs,
					  struct ext2_group_desc *gdp,
					  dgrp_t group)
{
	if (fs->super->s_desc_size >= EXT2_MIN_DESC_SIZE_64BIT)
		return (struct ext2_group_desc *)
			((struct ext4_group_desc *) gdp + group);
	else
		return (struct ext2_group_desc *) gdp + group;
}

/*
 * Return the block bitmap block of a group
 */
blk64_t ext2fs_block_bitmap_loc(ext2_filsys fs, dgrp_t group)
{
	if (fs->super->s_desc_size >= EXT2_MIN_DESC_SIZE_64BIT) {
		struct ext4_group_desc *gdp;
		gdp = (struct ext4_group_desc *) (fs->group_desc) + group;

		return gdp->bg_block_bitmap |
			(fs->super->s_feature_incompat
			 & EXT4_FEATURE_INCOMPAT_64BIT ?
			 (__u64) gdp->bg_block_bitmap_hi << 32 : 0);
	}

	return fs->group_desc[group].bg_block_bitmap;
}

/*
 * Set the block bitmap block of a group
 */
void ext2fs_block_bitmap_loc_set(ext2_filsys fs, dgrp_t group, blk64_t blk)
{
	if (fs->super->s_desc_size >= EXT2_MIN_DESC_SIZE_64BIT) {
		struct ext4_group_desc *gdp;
		gdp = (struct ext4_group_desc *) (fs->group_desc) + group;
		gdp->bg_block_bitmap = blk;
		if (fs->super->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT)
			gdp->bg_block_bitmap_hi = (__u64) blk >> 32;
	} else
		fs->group_desc[group].bg_block_bitmap = blk;
}

/*
 * Return the inode bitmap block of a group
 */
blk64_t ext2fs_inode_bitmap_loc(ext2_filsys fs, dgrp_t group)
{
	if (fs->super->s_desc_size >= EXT2_MIN_DESC_SIZE_64BIT) {
		struct ext4_group_desc *gdp;
		gdp = (struct ext4_group_desc *) (fs->group_desc) + group;

		return gdp->bg_inode_bitmap |
			(fs->super->s_feature_incompat
			 & EXT4_FEATURE_INCOMPAT_64BIT ?
			 (__u64) gdp->bg_inode_bitmap_hi << 32 : 0);
	}

	return fs->group_desc[group].bg_inode_bitmap;
}

/*
 * Set the inode bitmap block of a group
 */
void ext2fs_inode_bitmap_loc_set(ext2_filsys fs, dgrp_t group, blk64_t blk)
{
	if (fs->super->s_desc_size >= EXT2_MIN_DESC_SIZE_64BIT) {
		struct ext4_group_desc *gdp;
		gdp = (struct ext4_group_desc *) (fs->group_desc) + group;
		gdp->bg_inode_bitmap = blk;
		if (fs->super->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT)
			gdp->bg_inode_bitmap_hi = (__u64) blk >> 32;
	} else
		fs->group_desc[group].bg_inode_bitmap = blk;
}

/*
 * Return the inode table block of a group
 */
blk64_t ext2fs_inode_table_loc(ext2_filsys fs, dgrp_t group)
{
	if (fs->super->s_desc_size >= EXT2_MIN_DESC_SIZE_64BIT) {
		struct ext4_group_desc *gdp;
		gdp = (struct ext4_group_desc *) (fs->group_desc) + group;

		return gdp->bg_inode_table |
			(fs->super->s_feature_incompat
			 & EXT4_FEATURE_INCOMPAT_64BIT ?
			 (__u64) gdp->bg_inode_table_hi << 32 : 0);
	}

	return fs->group_desc[group].bg_inode_table;
}

/*
 * Set the inode table block of a group
 */
void ext2fs_inode_table_loc_set(ext2_filsys fs, dgrp_t group, blk64_t blk)
{
	if (fs->super->s_desc_size >= EXT2_MIN_DESC_SIZE_64BIT) {
		struct ext4_group_desc *gdp;
		gdp = (struct ext4_group_desc *) (fs->group_desc) + group;
		gdp->bg_inode_table = blk;
		if (fs->super->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT)
			gdp->bg_inode_table_hi = (__u64) blk >> 32;
	} else
		fs->group_desc[group].bg_inode_table = blk;
}

/*
 * Return the free blocks count of a group
 */
blk64_t ext2fs_bg_free_blocks_count(ext2_filsys fs, dgrp_t group)
{
	if (fs->super->s_desc_size >= EXT2_MIN_DESC_SIZE_64BIT) {
		struct ext4_group_desc *gdp;
		gdp = (struct ext4_group_desc *) (fs->group_desc) + group;

		return gdp->bg_free_blocks_count |
			(fs->super->s_feature_incompat
			 & EXT4_FEATURE_INCOMPAT_64BIT ?
			 (__u64) gdp->bg_free_blocks_count_hi << 32 : 0);
	}

	return fs->group_desc[group].bg_free_blocks_count;
}

/*
 * Set the free blocks count of a group
 */
void ext2fs_bg_free_blocks_count_set(ext2_filsys fs, dgrp_t group, blk64_t blk)
{
	if (fs->super->s_desc_size >= EXT2_MIN_DESC_SIZE_64BIT) {
		struct ext4_group_desc *gdp;
		gdp = (struct ext4_group_desc *) (fs->group_desc) + group;
		gdp->bg_free_blocks_count = blk;
		if (fs->super->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT)
			gdp->bg_free_blocks_count_hi = (__u64) blk >> 32;
	} else
		fs->group_desc[group].bg_free_blocks_count = blk;
}

/*
 * Return the free inodes count of a group
 */
blk64_t ext2fs_bg_free_inodes_count(ext2_filsys fs, dgrp_t group)
{
	if (fs->super->s_desc_size >= EXT2_MIN_DESC_SIZE_64BIT) {
		struct ext4_group_desc *gdp;
		gdp = (struct ext4_group_desc *) (fs->group_desc) + group;

		return gdp->bg_free_inodes_count |
			(fs->super->s_feature_incompat
			 & EXT4_FEATURE_INCOMPAT_64BIT ?
			 (__u64) gdp->bg_free_inodes_count_hi << 32 : 0);
	}

	return fs->group_desc[group].bg_free_inodes_count;
}

/*
 * Set the free inodes count of a group
 */
void ext2fs_bg_free_inodes_count_set(ext2_filsys fs, dgrp_t group, blk64_t blk)
{
	if (fs->super->s_desc_size >= EXT2_MIN_DESC_SIZE_64BIT) {
		struct ext4_group_desc *gdp;
		gdp = (struct ext4_group_desc *) (fs->group_desc) + group;
		gdp->bg_free_inodes_count = blk;
		if (fs->super->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT)
			gdp->bg_free_inodes_count_hi = (__u64) blk >> 32;
	} else
		fs->group_desc[group].bg_free_inodes_count = blk;
}

/*
 * Return the used dirs count of a group
 */
blk64_t ext2fs_bg_used_dirs_count(ext2_filsys fs, dgrp_t group)
{
	if (fs->super->s_desc_size >= EXT2_MIN_DESC_SIZE_64BIT) {
		struct ext4_group_desc *gdp;
		gdp = (struct ext4_group_desc *) (fs->group_desc) + group;

		return gdp->bg_used_dirs_count |
			(fs->super->s_feature_incompat
			 & EXT4_FEATURE_INCOMPAT_64BIT ?
			 (__u64) gdp->bg_used_dirs_count_hi << 32 : 0);
	}

	return fs->group_desc[group].bg_used_dirs_count;
}

/*
 * Set the used dirs count of a group
 */
void ext2fs_bg_used_dirs_count_set(ext2_filsys fs, dgrp_t group, blk64_t blk)
{
	if (fs->super->s_desc_size >= EXT2_MIN_DESC_SIZE_64BIT) {
		struct ext4_group_desc *gdp;
		gdp = (struct ext4_group_desc *) (fs->group_desc) + group;
		gdp->bg_used_dirs_count = blk;
		if (fs->super->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT)
			gdp->bg_used_dirs_count_hi = (__u64) blk >> 32;
	} else
		fs->group_desc[group].bg_used_dirs_count = blk;
}

/*
 * Return the unused inodes count of a group
 */
blk64_t ext2fs_bg_itable_unused(ext2_filsys fs, dgrp_t group)
{
	if (fs->super->s_desc_size >= EXT2_MIN_DESC_SIZE_64BIT) {
		struct ext4_group_desc *gdp;
		gdp = (struct ext4_group_desc *) (fs->group_desc) + group;

		return gdp->bg_itable_unused |
			(fs->super->s_feature_incompat
			 & EXT4_FEATURE_INCOMPAT_64BIT ?
			 (__u64) gdp->bg_itable_unused_hi << 32 : 0);
	}

	return fs->group_desc[group].bg_itable_unused;
}

/*
 * Set the unused inodes count of a group
 */
void ext2fs_bg_itable_unused_set(ext2_filsys fs, dgrp_t group, blk64_t blk)
{
	if (fs->super->s_desc_size >= EXT2_MIN_DESC_SIZE_64BIT) {
		struct ext4_group_desc *gdp;
		gdp = (struct ext4_group_desc *) (fs->group_desc) + group;
		gdp->bg_itable_unused = blk;
		if (fs->super->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT)
			gdp->bg_itable_unused_hi = (__u64) blk >> 32;
	} else
		fs->group_desc[group].bg_itable_unused = blk;
}

/*
 * Get the flags for this block group
 */
__u16 ext2fs_bg_flags(ext2_filsys fs, dgrp_t group)
{
	if (fs->super->s_desc_size >= EXT2_MIN_DESC_SIZE_64BIT) {
		struct ext4_group_desc *gdp;
		gdp = (struct ext4_group_desc *) (fs->group_desc) + group;

		return gdp->bg_flags;
	}

	return fs->group_desc[group].bg_flags;
}

/*
 * Set the flags for this block group
 */
void ext2fs_bg_flags_set(ext2_filsys fs, dgrp_t group, __u16 bg_flags)
{
	if (fs->super->s_desc_size >= EXT2_MIN_DESC_SIZE_64BIT) {
		struct ext4_group_desc *gdp;
		gdp = (struct ext4_group_desc *) (fs->group_desc) + group;

		gdp->bg_flags = bg_flags;
		return;
	}

	fs->group_desc[group].bg_flags = bg_flags;
}

/*
 * Clear the flags for this block group
 */
void ext2fs_bg_flags_clear(ext2_filsys fs, dgrp_t group, __u16 bg_flags)
{
	if (fs->super->s_desc_size >= EXT2_MIN_DESC_SIZE_64BIT) {
		struct ext4_group_desc *gdp;
		gdp = (struct ext4_group_desc *) (fs->group_desc) + group;

		gdp->bg_flags = 0;
		return;
	}

	fs->group_desc[group].bg_flags = 0;
}

/*
 * Get the value of a particular flag for this block group
 */
int ext2fs_bg_flag_test(ext2_filsys fs, dgrp_t group, __u16 bg_flag)
{
	if (fs->super->s_desc_size >= EXT2_MIN_DESC_SIZE_64BIT) {
		struct ext4_group_desc *gdp;
		gdp = (struct ext4_group_desc *) (fs->group_desc) + group;

		return gdp->bg_flags & bg_flag;
	}

	return fs->group_desc[group].bg_flags & bg_flag;
}

/*
 * Set a particular flag for this block group
 */
void ext2fs_bg_flag_set(ext2_filsys fs, dgrp_t group, __u16 bg_flag)
{
	if (fs->super->s_desc_size >= EXT2_MIN_DESC_SIZE_64BIT) {
		struct ext4_group_desc *gdp;
		gdp = (struct ext4_group_desc *) (fs->group_desc) + group;

		gdp->bg_flags |= bg_flag;
		return;
	}

	fs->group_desc[group].bg_flags |= bg_flag;
}

/*
 * Clear a particular flag for this block group
 */
void ext2fs_bg_flag_clear(ext2_filsys fs, dgrp_t group, __u16 bg_flag)
{
	if (fs->super->s_desc_size >= EXT2_MIN_DESC_SIZE_64BIT) {
		struct ext4_group_desc *gdp;
		gdp = (struct ext4_group_desc *) (fs->group_desc) + group;

		gdp->bg_flags &= ~bg_flag;
		return;
	}

	fs->group_desc[group].bg_flags &= ~bg_flag;
}

/*
 * Get the checksum for this block group
 */
__u16 ext2fs_bg_checksum(ext2_filsys fs, dgrp_t group)
{
	if (fs->super->s_desc_size >= EXT2_MIN_DESC_SIZE_64BIT) {
		struct ext4_group_desc *gdp;
		gdp = (struct ext4_group_desc *) (fs->group_desc) + group;

		return gdp->bg_checksum;
	}

	return fs->group_desc[group].bg_checksum;
}

/*
 * Set the checksum for this block group to a previously calculated value
 */
void ext2fs_bg_checksum_set(ext2_filsys fs, dgrp_t group, __u16 checksum)
{
	if (fs->super->s_desc_size >= EXT2_MIN_DESC_SIZE_64BIT) {
		struct ext4_group_desc *gdp;
		gdp = (struct ext4_group_desc *) (fs->group_desc) + group;
		
		gdp->bg_checksum = checksum;
		return;
	}

	fs->group_desc[group].bg_checksum = checksum;
}

/*
 * Get the acl block of a file
 *
 * XXX Ignoring 64-bit file system flag - most places where this is
 * called don't have access to the fs struct, and the high bits should
 * be 0 in the non-64-bit case anyway.
 */
blk64_t ext2fs_file_acl_block(const struct ext2_inode *inode)
{
	return (inode->i_file_acl |
		(__u64) inode->osd2.linux2.l_i_file_acl_high << 32);
}

/*
 * Set the acl block of a file
 */
void ext2fs_file_acl_block_set(struct ext2_inode *inode, blk64_t blk)
{
	inode->i_file_acl = blk;
	inode->osd2.linux2.l_i_file_acl_high = (__u64) blk >> 32;
}


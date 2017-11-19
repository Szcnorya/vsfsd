# vsfsd - Very Simple File System (Demon)
A I-node based memory file system implementation.
Implemented in C++.

## Intro
The program organized a allocated memory as a file system to manage.

Providing following functions for external user.
1. Init file system with given disk size and block size
2. create/delete the file/directory
3. read/write file in binary stream
4. change pwd
5. copy file to new path

## Implemented detail

### Disk layout
Disk:
`super_block` | `block_bitmap` | `inode_bitmap` | `inode_table` | `data area`

`super_block`: magic number to specify vsfsd, and some statistic data to maintain the file system.
```C++
struct super_block{
    //Statistic INFO
    u32 inodes_count;
    u32 used_inode;
    u32 block_count;
    u32 used_block_count;
    u32 blockBitmapBlock;
    u32 iNodeBitmapBlock;
    u32 iNodeTableBlock;
    u32 dataBlock;
    u32 block_size;
    u32 magic;
};
```

`block_bitmap`: Use/Unused map for block
`inode_bitmap`: Use/Unused map for inode
`inode_table` : Inode data area
`data_area` : blocks

### Notice
* Minimum addressable unit is byte.
* All address are 32-bit.
* Only level-1 indirect block is implemented

#ifndef __vsfsd__vsfsd__
#define __vsfsd__vsfsd__

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
using std::vector;
using std::string;
/*********************General Disk Distribution*****************/
//(super_block)(block_bitmap)(inode_bitmap)(inode_table)(data area)
typedef unsigned int u32;
typedef unsigned char u08;

const int maxInodeNum=65536;
const int ROOT_INODE=0;
const int BLOCK_PER_INODE = 6;
const int maxFileName=256;

const char SEPARATOR = '/';
const char ROOT_NAME[] = "";
const char CURRENT_NAME[] = ".";
const char PARENT_NAME[] = "..";

class FileInfo {
public:
    char name[maxFileName];
    u32 size;
    u32 inode;
    u32 atime;
    u32 ctime;
    bool isDirectory;
    bool operator<(const FileInfo& fi) const {
        if (isDirectory != fi.isDirectory)
            return isDirectory > fi.isDirectory;
        else
            return name < fi.name;
    }
};


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
struct inode{
    u08 flag;
    u32 size;
    u32 atime;
    u32 ctime;
    u32 blockAddr[BLOCK_PER_INODE];//1-5 for data Block,5 for indirect block
    bool isDirectory() const{
        return flag&1;
    }
};
struct vsfsd_dir_entry{
    u32 inodeNum;
    u08 name[maxFileName];
};

typedef vector<u08> fdata;
typedef vector<string> StringList;
typedef vector<vsfsd_dir_entry> DirList;
typedef vsfsd_dir_entry DirEntry;

class vsfsd{
private:
    u08 *disk;
    super_block* supb;
    u08* blockBitmap;
    u08* inodeBitmap;
    u08* data;
    inode* InodeTable;
    u32 currentPath;
    
    int Format(u32 diskSize,u32 blockSize);
    u32 allocB();
    void readB(u32 Index,u32 depth,fdata& Data,u32& size);
    void writeB(u32& Index,u32 depth,fdata::const_iterator& Data,u32& size);
    //Free the direct block and the indirect block
    void freeB(u32 Index,u32 depth,u32& size);
    u32 allocI();
    void readI(u32 Index,fdata& Data);
    void writeI(u32 Index,const fdata& Data);
    //freeI release all the block of the I-node
    void freeI(u32 Index);
    //release return the I-node to the unused table
    void releaseI(u32 Index);
    void removeI(u32 Index);
    u08* BlockAt(u32 Index);
    u32 BlockSize();
    u32 AddrPerBlock();
    u32 blockNeed(u32 size);
    //Return empty when fail
    StringList Parse(const string fileName);
    DirList newDirList(u32 current,u32 parent);
    //Encode and decode function in DirectoryFile type
    fdata toFile(const DirList& dirFile);
    DirList toDirList(const fdata& File);
    bool getChildInode(u32 parent,const string &childName,u32& childInode);
    bool gotoChildDir(u32& current,const string &childName);
    bool createChild(u32 parent,const string &childName,bool isDir);
    bool getInodeIndex(const string& path,u32& index);
public:
    vsfsd();
    ~vsfsd();
    bool available();
    string getCurrentPath();
    int Init(u32 diskSize=16*1024*1024,u32 blockSize=1024);
    bool createFileOrDir(const string& filePath,bool dir);
    bool deleteFileOrDir(const string& filePath,bool dir);
    bool readFile(const string& path,fdata& content);
    bool writeFile(const string& path,fdata& content);
    bool changeDir(const string& Path);
    bool copy(const string& src,const string& dest);
    vector<FileInfo> dir();
    void statistic();
};
#endif /* defined(__vsfsd__vsfsd__) */

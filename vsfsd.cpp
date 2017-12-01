#include "vsfsd.h"
#include <ctime>
#include <cstring>
u32 CeilDivide(u32 a,u32 b){
    return a==0?0:(a-1)/b + 1;
}
void setBitmap(u08* Bitmap,u32 Ind, bool val){
    u08 mask;
    int positon=Ind%8;
    if(val){
        mask=1;
        for(int i=0;i<8;i++){
            if(i!=positon){
                mask++;
            }
            mask<<=1;
        }
        Bitmap[Ind]|=mask;
    }
    else{
        mask=0;
        for(int i=0;i<8;i++){
            if(i==positon){
                mask++;
            }
            mask<<=1;
        }
        Bitmap[Ind]&=mask;
    }
}
bool getBitmap(u08* Bitmap,u32 Ind){
    u08 data=Bitmap[Ind];
    int positon=7-Ind%8;
    for(int i=0;i<8;i++){
        if(i==positon)
            return data&1;
        data>>=1;
    }
    //should be never reached
    return 0;
}
vsfsd::vsfsd(){
    disk=NULL;
}
vsfsd::~vsfsd(){
    free(disk);
}
int vsfsd::Init(u32 diskSize,u32 blockSize){
    if(available())
        free(disk);
    disk=(u08*)malloc(diskSize);
    if(disk==NULL){
        printf("Can not open disk\n");
        return 1;
    }
    puts("Successfully connect to disk");
    Format(diskSize,blockSize);
    return 0;
}
int vsfsd::Format(u32 dsz,u32 bsz){
    puts("Start Formatting");
    puts("Building Super Block");
    supb=(super_block*)disk;
    supb->block_count=dsz/bsz;
    supb->block_size=bsz;
    supb->inodes_count=maxInodeNum;
    supb->used_inode=0;
    supb->magic=666;
    u32 bitPerBlock=BlockSize()<<3;
    u32 bbmBlockCount=CeilDivide(supb->block_count, bitPerBlock);
    u32 ibmBlockCount=CeilDivide(supb->inodes_count, bitPerBlock);
    u32 iTableCount=CeilDivide(supb->inodes_count*sizeof(inode), bitPerBlock);
    supb->blockBitmapBlock=1;
    supb->iNodeBitmapBlock=supb->blockBitmapBlock+bbmBlockCount;
    supb->iNodeTableBlock=supb->iNodeBitmapBlock+ibmBlockCount;
    supb->dataBlock=supb->iNodeTableBlock+iTableCount;
    supb->used_block_count=supb->dataBlock;
    blockBitmap=BlockAt(supb->blockBitmapBlock);
    inodeBitmap=BlockAt(supb->iNodeBitmapBlock);
    InodeTable=(inode*)BlockAt(supb->iNodeTableBlock);
    data=BlockAt(supb->dataBlock);
    puts("Building Block Bitmap");
    memset(blockBitmap,0,bbmBlockCount*BlockSize());
    for(u32 i=0;i<supb->used_block_count;i++)
        setBitmap(blockBitmap,i,true);
    puts("Building Inode Bitmap");
    memset(inodeBitmap,0,ibmBlockCount*BlockSize());
    puts("Creating root directory");
    setBitmap(inodeBitmap,ROOT_INODE,true);
    supb->used_inode++;
    inode &rootInode=InodeTable[ROOT_INODE];
    rootInode.size=0;
    rootInode.flag=1;
    rootInode.ctime=time(NULL);
    writeI(ROOT_INODE,toFile(newDirList(ROOT_INODE, ROOT_INODE)));
    currentPath=0;
    puts("Finish Formatting");
    return 0;
}
u32 vsfsd::allocI(){
    if(supb->used_inode==supb->inodes_count)
        return ROOT_INODE;
    for(u32 i=1;i<supb->inodes_count;i++){
        if(!getBitmap(inodeBitmap, i)){
            setBitmap(inodeBitmap, i, true);
            supb->used_inode++;
            InodeTable[i].ctime=time(NULL);
            return i;
        }
    }
    return ROOT_INODE;
}

void vsfsd::writeI(u32 ind,const fdata& file){
    freeI(ind);
    InodeTable[ind].size=file.size();
    u32 size=file.size();
    fdata::const_iterator iter=file.begin();
    for(u32 i=0;i<BLOCK_PER_INODE-1;i++){
        writeB(InodeTable[ind].blockAddr[i],0,iter,size);
    }
    if(size)
        writeB(InodeTable[ind].blockAddr[BLOCK_PER_INODE-1],1,iter,size);
    InodeTable[ind].atime=time(NULL);
}
void vsfsd::readI(u32 ind,fdata& file){
    u32 size=InodeTable[ind].size;
    for(u32 i=0;i<BLOCK_PER_INODE-1;i++){
        readB(InodeTable[ind].blockAddr[i],0,file,size);
    }
    if(size)
        readB(InodeTable[ind].blockAddr[BLOCK_PER_INODE-1],1,file,size);
}
void vsfsd::releaseI(u32 Ind){
    setBitmap(inodeBitmap, Ind, false);
    supb->used_inode--;
}
void vsfsd::freeI(u32 ptr){
    u32 totalBlock = blockNeed(InodeTable[ptr].size);
    for(u32 i=0;i<BLOCK_PER_INODE-1&&totalBlock;i++)
        freeB(InodeTable[ptr].blockAddr[i], 0, totalBlock);
    if(totalBlock)
        freeB(InodeTable[ptr].blockAddr[BLOCK_PER_INODE-1],1, totalBlock);
    InodeTable[ptr].size=0;
}
void vsfsd::removeI(u32 Index){
    if(Index==ROOT_INODE){
        puts("ROOT I-NODE CAN N0T BE REMOVE");
        return;
    }
    if(InodeTable[Index].isDirectory()){
        fdata content;
        readI(Index, content);
        DirList lst=toDirList(content);
        for(DirList::const_iterator ite=lst.begin();ite!=lst.end();){
            if(strcmp((char*)ite->name,PARENT_NAME)&&strcmp((char*)ite->name,CURRENT_NAME)){
                removeI(ite->inodeNum);
                ite=lst.erase(ite);
                
            }
            else
                ite++;
        }
        writeI(Index, toFile(lst));
    }
    freeI(Index);
    releaseI(Index);
}
u32 vsfsd::allocB(){
    if(supb->used_block_count==supb->block_count)
        return 0;
    for(u32 i=supb->dataBlock;i<supb->block_count;i++){
        if(!getBitmap(blockBitmap, i)){
            setBitmap(blockBitmap,i,true);
            supb->used_block_count++;
            return i;
        }
    }
    return 0;
}
void vsfsd::writeB(u32& Index,u32 dep,fdata::const_iterator& ite,u32& size){
    if(size==0)
        return;
    Index=allocB();
    if(Index==0)
        return;
    if(dep==0){
        u08* ptr=BlockAt(Index);
        for(u32 i=0;i<BlockSize()&&size;i++){
            *ptr=*ite;
            size--;
            ++ptr;
            ++ite;
        }
    }
    else{
        u32* ptr=(u32*)BlockAt(Index);
        for(u32 i=0;i<AddrPerBlock()&&size;i++){
            writeB(*ptr,dep-1,ite,size);
            ++ptr;
        }
    }
}
void vsfsd::readB(u32 Index,u32 dep, fdata& data, u32& size){
    if(size==0)
        return;
    if(dep==0){
        u08* ptr = BlockAt(Index);
        for(u32 i=0;i<BlockSize()&&size;++i){
            data.push_back(*ptr);
            ptr++;
            size--;
        }
    }
    else{
        u32* ptr=(u32*)BlockAt(Index);
        for(u32 i=0;i<AddrPerBlock()&&size;i++){
            readB(*ptr, dep-1,data, size);
            ptr++;
        }
    }
}
void vsfsd::freeB(u32 Ind, u32 dep,u32& size){
    if(size==0)
        return;
    size--;
    setBitmap(blockBitmap,Ind,false);
    supb->used_block_count--;
    if(dep){
        u32 *ptr=(u32*)BlockAt(Ind);
        for(u32 i=0;i<AddrPerBlock()&&size;i++){
            freeB(*ptr, dep-1,size);
            ptr++;
        }
    }
}
vector<string> vsfsd::Parse(const string fileName){
    vector<string> result;
    u32 pre=0;
    for(u32 i=0;i<fileName.size();i++)
        if(fileName[i]==SEPARATOR){
            if(i==pre&&i){
                result.clear();
                return result;
            }
            result.push_back(fileName.substr(pre,i-pre));
            pre=i+1;
        }
    if(pre<fileName.size())
        result.push_back(fileName.substr(pre,fileName.size()-pre));
    return result;
}
DirList vsfsd::newDirList(u32 current, u32 parent){
    DirList ret;
    DirEntry tempEntry;
    strcpy((char*)tempEntry.name,CURRENT_NAME);
    tempEntry.inodeNum=current;
    ret.push_back(tempEntry);
    strcpy((char*)tempEntry.name,PARENT_NAME);
    tempEntry.inodeNum=parent;
    ret.push_back(tempEntry);
    return ret;
}
fdata vsfsd::toFile(const DirList& dirFile){
    fdata ret;
    for(DirList::const_iterator dirEntry=dirFile.begin();
        dirEntry!=dirFile.end();
        dirEntry++){
        for(u32 i=0;i<maxFileName;i++){
            ret.push_back((dirEntry->name[i]));
            if(!dirEntry->name[i])
                break;
        }
        for (u32 i = 0; i < 4; ++i)
            ret.push_back((dirEntry->inodeNum >> (3 - i) * 8) & 0xff);
    }
    return ret;
}
DirList vsfsd::toDirList(const fdata& File){
    DirList ret;
    DirEntry tempEntry;
    for(fdata::const_iterator ite=File.begin();
        ite!=File.end();){
        for(u32 i=0;i<maxFileName;i++){
            tempEntry.name[i]=*ite;
            ++ite;
            if(!tempEntry.name[i])
                break;
        }
        tempEntry.inodeNum = 0;
        for (int i = 0; i < 4; ++i) {
            tempEntry.inodeNum <<= 8;
            tempEntry.inodeNum |= *ite;
            ++ite;
        }
        ret.push_back(tempEntry);
    }
    return ret;
}
bool vsfsd::getChildInode(u32 parent, const string& childName,u32& childInode){
    fdata content;
    readI(parent,content);
    DirList childs=toDirList(content);
    for(DirList::const_iterator ite=childs.begin();
        ite!=childs.end();
        ite++){
        if((char*)(ite->name)==childName){
            childInode=ite->inodeNum;
            return true;
        }
    }
    return false;
}
bool vsfsd::gotoChildDir(u32& current,const string& childName){
    u32 child;
    if(getChildInode(current, childName, child)&&InodeTable[child].isDirectory()){
        current=child;
        return true;
    }
    else{
        return false;
    }
}
bool vsfsd::createChild(u32 parent,const string& childName,bool dir){
    if(!childName.size())
        return false;
    fdata content;
    readI(parent,content);
    DirList childs=toDirList(content);
    for(DirList::const_iterator ite=childs.begin();
        ite!=childs.end();
        ite++){
        if((char*)(ite->name)==childName)
            return false;
    }
    u32 newInodeIndex=allocI();
    if(newInodeIndex==ROOT_INODE)
        return false;
    inode& newInode=InodeTable[newInodeIndex];
    newInode.flag=dir;
    newInode.size=0;
    if(dir)
        writeI(newInodeIndex, toFile(newDirList(newInodeIndex,parent)));
    DirEntry newEntry;
    strcpy((char*)newEntry.name,childName.c_str());
    newEntry.inodeNum=newInodeIndex;
    childs.push_back(newEntry);
    writeI(parent, toFile(childs));
    return true;
}
bool vsfsd::getInodeIndex(const string& Path,u32& index){
    StringList lst=Parse(Path);
    if(lst.size()==0)
        return 0;
    index=currentPath;
    StringList::iterator aim=--lst.end();
    for(StringList::iterator ite=lst.begin();ite!=aim;ite++){
        if(ite->size()!=0){
            if(!gotoChildDir(index,*ite))
                return false;
        }
        else
            index=ROOT_INODE;
    }
    if(!getChildInode(index, *aim, index))
        return false;
    return true;
}
u08* vsfsd::BlockAt(u32 Index){
    return disk+(Index)*BlockSize();
}
u32 vsfsd::BlockSize(){
    return supb->block_size;
}
u32 vsfsd::AddrPerBlock(){
    return BlockSize()/sizeof(u32);
}
u32 vsfsd::blockNeed(u32 size){
    u32 dataBlockCount = CeilDivide(size, BlockSize());
    if(dataBlockCount<=BLOCK_PER_INODE-1)
        return dataBlockCount;
    else
        return dataBlockCount+1;
}

/*----------------------PUBLIC---------------------------------*/
bool vsfsd::available(){
    return disk!=NULL;
}
string vsfsd::getCurrentPath(){
    string rst;
    if(available()){
        u32 currentI=currentPath;
        while(1){
            if(currentI==ROOT_INODE){
                if(!rst.size())
                    rst=SEPARATOR+rst;
                break;
            }
            else{
                u32 parentI;
                getChildInode(currentI, PARENT_NAME, parentI);
                fdata content;
                readI(parentI,content);
                DirList brothers=toDirList(content);
                for(DirList::const_iterator ite=brothers.begin();ite!=brothers.end();ite++){
                    if(ite->inodeNum==currentI){
                        rst=SEPARATOR+string((char*)ite->name)+rst;
                        break;
                    }
                }
                currentI=parentI;
            }
        }
    }
    return rst;
}
bool vsfsd::createFileOrDir(const string& filePath,bool dir){
    if(!available()){
        puts("PLEASE LOAD THE DISK");
        return false;
    }
    StringList lst=Parse(filePath);
    if(lst.size()==0)
        return false;
    u32 parentInode=currentPath;
    StringList::iterator aim=--lst.end();
    if (*aim == CURRENT_NAME || *aim == PARENT_NAME) {
        puts("Invalid Path!");
        return false;
    }
    for(StringList::const_iterator ite=lst.begin();
        ite!=aim;
        ite++){
        if(ite->size()){
            if(!gotoChildDir(parentInode, *ite))
                if(createChild(parentInode, *ite, true))
                    gotoChildDir(parentInode, *ite);
        }
        else
            parentInode=ROOT_INODE;
    }
    if(!createChild(parentInode, *aim, dir)){
        puts("ALREADY EXIST SUCH FILE");
        return false;
    }
    return true;
}
bool vsfsd::deleteFileOrDir(const string& filePath, bool dir){
    if(!available()){
        puts("PLEASE LOAD THE DISK");
        return false;
    }
    StringList lst=Parse(filePath);
    if(lst.size()==0)
        return false;
    u32 parentInode=currentPath;
    StringList::iterator aim=--lst.end();
    if (*aim == CURRENT_NAME || *aim == PARENT_NAME) {
        puts("Invalid Path!");
        return false;
    }
    for(StringList::const_iterator ite=lst.begin();
        ite!=aim;
        ite++){
        if(ite->size()){
            if(!gotoChildDir(parentInode, *ite))
                if(createChild(parentInode, *ite, true))
                    gotoChildDir(parentInode, *ite);
        }
        else
            parentInode=ROOT_INODE;
    }
    u32 aimInode;
    if(!getChildInode(parentInode, *aim, aimInode)){
        puts("NO SUCH FILE OR DIRECTORY");
        return false;
    }
    removeI(aimInode);
    fdata content;
    readI(parentInode,content);
    DirList childs=toDirList(content);
    for(DirList::const_iterator ite=childs.begin();ite!=childs.end();ite++){
        if((char*)(ite->name)==*aim){
            childs.erase(ite);
            break;
        }
    }
    writeI(parentInode, toFile(childs));
    currentPath=parentInode;
    return true;
}
bool vsfsd::readFile(const string& path, fdata& content){
    if(!available()){
        puts("PLEASE LOAD THE DISK");
        return false;
    }
    u32 index;
    if(!getInodeIndex(path, index)){
        puts("DIDN'T FOUND THE FILE AT THE PATH");
        return false;
    }
    readI(index, content);
    return true;
}
bool vsfsd::writeFile(const string& path,fdata& content){
    if(!available()){
        puts("PLEASE LOAD THE DISK");
        return false;
    }
    u32 index;
    if(!getInodeIndex(path, index)){
        puts("DIDN'T FOUND THE FILE AT THE PATH");
        return false;
    }
    writeI(index, content);
    return true;
}
bool vsfsd::changeDir(const string& Path){
    if(!available()){
        puts("PLEASE LOAD THE DISK");
        return false;
    }
    u32 idx;
    bool rst=getInodeIndex(Path, idx);
    if(!rst||!InodeTable[idx].isDirectory()){
        puts("INVALID TARGET DIRECTORY");
        return false;
    }
    currentPath=idx;
    return true;
}
bool vsfsd::copy(const string& src,const string& dest){
    if(!available()){
        puts("PLEASE LOAD THE DISK");
        return false;
    }
    fdata content;
    if(!readFile(src, content))
        return false;
    if(!createFileOrDir(dest,false))
        return false;
    if(!writeFile(dest,content))
        return false;
    return true;
}
vector<FileInfo> vsfsd::dir(){
    vector<FileInfo> rst;
    if(!available()){
        puts("PLEASE LOAD THE DISK");
        return rst;
    }
    fdata content;
    FileInfo tmpInfo;
    readI(currentPath, content);
    DirList lst=toDirList(content);
    for(DirList::const_iterator ite=lst.begin();ite!=lst.end();ite++){
        strcpy(tmpInfo.name,(char*)ite->name);
        tmpInfo.inode=ite->inodeNum;
        tmpInfo.size=InodeTable[tmpInfo.inode].size;
        tmpInfo.atime=InodeTable[tmpInfo.inode].atime;
        tmpInfo.ctime=InodeTable[tmpInfo.inode].ctime;
        tmpInfo.isDirectory=InodeTable[tmpInfo.inode].isDirectory();
        rst.push_back(tmpInfo);
    }
    return rst;
}
void vsfsd::statistic(){
    if(!available()){
        puts("PLEASE LOAD THE DISK");
        return;
    }
    printf("CURRENT STATUS:\n");
    printf("INODES_COUNT:%d\n",supb->inodes_count);
    printf("USED_I-NODE:%d\n",supb->used_inode);
    printf("BLOCK_COUNT:%d\n",supb->block_count);
    printf("USED_BLOCK_COUNT:%d\n",supb->used_block_count);
    printf("BLOCK_SIZE:%d\n",supb->block_size);
}

#include <iostream>
#include <cstdio>
#include <vector>
#include <string>
#include <map>
#include <iomanip>
#include <sstream>

#include "vsfsd.h"

typedef std::vector<std::string> Args;
bool UserStatus;
vsfsd FILE_STSTEM;
std::map<std::string,void(*)(Args)> Func;
void format(Args args);
void statistic(Args args);
void mk(Args args);
void mkdir(Args args);
void rm(Args args);
void rmdir(Args args);
void cd(Args args);
void help(Args args);
void cat(Args args);
void cp(Args args);
void exit(Args args);
void dir(Args args);
void Reset(){
    srand(time(NULL));
    printf("vsfsd Version 1.0\n");
    printf("A very simple file system deamon based on the memory storage.\n");
    UserStatus=true;
    Func["format"]=format;
    Func["statistic"]=statistic;
    Func["mk"]=mk;
    Func["mkdir"]=mkdir;
    Func["rm"]=rm;
    Func["rmdir"]=rmdir;
    Func["dir"]=dir;
    Func["cd"]=cd;
    Func["help"]=help;
    Func["cat"]=cat;
    Func["cp"]=cp;
    Func["exit"]=exit;
}
string randomString(u32 size){
    const char CHAR_LIB[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const static size_t CHAR_LIB_LEN = strlen(CHAR_LIB);
    std::string result;
    for (size_t i = 0; i < size; ++i)
        result.push_back(CHAR_LIB[rand() % CHAR_LIB_LEN]);
    return result;
}
Args ParseOp(const string& input){
    Args rst;
    std::istringstream ss(input);
    std::string arg;
    while(ss>>arg){
        rst.push_back(arg);
    }
    return rst;
}
void Exec(Args arg){
    if(arg.size()){
        if(Func.find(arg[0])!=Func.end()){
            Func[arg[0]](arg);
        }
        else{
            puts("INVALID OPERATION");
        }
    }
}
void createRdmFile(const string& filepath,u32 size){
    FILE_STSTEM.createFileOrDir(filepath,false);
    fdata content;
    string rcontent=randomString(size);
    for(string::const_iterator ite=rcontent.begin();
        ite!=rcontent.end();
        ite++)
        content.push_back((u08)(*ite));
    FILE_STSTEM.writeFile(filepath, content);
}
int main(int argc, const char * argv[]) {
    Reset();
    while(UserStatus){
        std::string input;
        std::cout << "\n" << FILE_STSTEM.getCurrentPath() << ">";
        std::getline(std::cin, input);
        Exec(ParseOp(input));
    }
    return 0;
}
void format(Args arg){
    u32 diskSize=atoi(arg[1].c_str());
    u32 blockSize=atoi(arg[2].c_str());
    if(diskSize>=16000000&&blockSize>=128)
        FILE_STSTEM.Init(diskSize,blockSize);
    FILE_STSTEM.createFileOrDir("/dir1/dir2", true);
    createRdmFile("/dir1/file1",100);
    createRdmFile("/dir1/dir2/file2",100);
}
void statistic(Args arg){
    FILE_STSTEM.statistic();
}
void mk(Args arg){
    createRdmFile(arg[1],atoi(arg[2].c_str()));
}
void mkdir(Args arg){
    FILE_STSTEM.createFileOrDir(arg[1],true);
}
void rm(Args arg){
    FILE_STSTEM.deleteFileOrDir(arg[1], false);
}
void rmdir(Args arg){
    FILE_STSTEM.deleteFileOrDir(arg[1], true);
}
void cd(Args arg){
    FILE_STSTEM.changeDir(arg[1]);
}
void help(Args arg){
    
}
void cp(Args arg){
    FILE_STSTEM.copy(arg[1],arg[2]);
}
void cat(Args arg){
    fdata content;
    FILE_STSTEM.readFile(arg[1], content);
    for(fdata::const_iterator ite=content.begin();ite!=content.end();ite++){
        std::cout<<*ite;
    }
    std::cout<<std::endl;
}
void dir(Args arg){
    vector<FileInfo> vv = FILE_STSTEM.dir();
    std::cout<<"NAME    |    SIZE    |    ATIME    |    CTIME    |    DIRECTORY?"<<std::endl;
    for(vector<FileInfo>::const_iterator ite=vv.begin();
        ite!=vv.end();
        ite++){
        std::cout<<ite->name<<"  |  "<<ite->size<<"  |  "<<ite->atime<<"  |  "<<ite->ctime<<"  |  "<<ite->isDirectory<<std::endl;
    }
}
void exit(Args arg){
    std::cout<<"Good bye"<<std::endl;
    UserStatus=false;
}

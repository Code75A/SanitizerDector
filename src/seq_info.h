#ifndef SEQ_INFO_H
#define SEQ_INFO_H

#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "llvm/ADT/BitVector.h"

namespace sseq
{
class SeqInfo
{
public:
    //接收参数
    SeqInfo(std::string fname, llvm::BitVector f) :file_name{ fname }, vflags(f)\
     { /*parse_file();*/}

    void parse_file()
    {
        std::ifstream ofs(file_name);
        if (!ofs.is_open())
        {
            std::cout << "[error]open file:" << file_name << " failed!\n";
            exit(-1);
        }
        std::cout << "open file:" << file_name << "!\n";
        std::string id;
        while (!ofs.eof())
        {
            ofs >> id; //文件输入
        }
        ofs.close();
    }

//此处可以添加成员
    // std::string module;
    std::string file_name;
    llvm::BitVector vflags;
};


}  // namespace sseq

#endif
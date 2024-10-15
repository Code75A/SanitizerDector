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
namespace sseq
{
class SeqInfo
{
public:
    //接收参数
    SeqInfo(std::string fn) :file_name{ fn }\
     { /*parse_file()*/;}

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
    std::string file_name;
};


}  // namespace sseq

#endif
#include "action.h"

#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <iostream>
#include <llvm/Support/CommandLine.h>

static llvm::cl::OptionCategory MyASTSlicer_category("myastslicer options");
static llvm::cl::extrahelp
    common_help(clang::tooling::CommonOptionsParser::HelpMessage);

    //commandLine Arguments
// static llvm::cl::opt<std::string> path_info_f("inst", llvm::cl::desc("path_info_file"),
//                                 llvm::cl::cat(MyASTSlicer_category));


int main(int argc, const char **argv)
{
    // int argc_f           = argc - 2;
    auto expected_parser = clang::tooling::CommonOptionsParser::create(
        argc,
        argv,
        MyASTSlicer_category,
        llvm::cl::NumOccurrencesFlag::ZeroOrMore
        );
    if (!expected_parser)
    {
        // Fail gracefully for unsupported options.
        llvm::errs() << expected_parser.takeError();
        return 1;
    }
    clang::tooling::CommonOptionsParser &options_parser = expected_parser.get();
    
    // clang::tooling::ClangTool tool(options_parser.getCompilations(),
    //                                options_parser.getSourcePathList());
    clang::tooling::ClangTool tool(
        options_parser.getCompilations(),
        options_parser.getCompilations().getAllFiles());

//add
    clang::tooling::ArgumentsAdjuster ardj = clang::tooling::getInsertArgumentAdjuster("-I/usr/local/llvm/lib/clang/12.0.0/include");
    tool.appendArgumentsAdjuster(ardj);

    std::cout << "[start]\n";

    //如果有需要输入参数，修改此处
    //argv[0]是程序exe名字，1之后是参数，当前demo里，argv[1]是编译数据库compile_commands.json的路径
    std::string mode="";
    std::string opt_set="";

    llvm::BitVector vflags(FLAG_WIDTH,false);

    if(argc>=3)
        mode=argv[2];

    if(mode=="div"){
        vflags[MAIN_BIT]=1;
        if(argc>=4)
            opt_set=argv[3];

        if(opt_set=="print"){
            vflags[OPT_BIT]=1;
        }
        else{
            vflags[OPT_BIT]=0;
        }
    }
    else if(mode=="shf"){
        vflags[MAIN_BIT]=1;
        mode="div";
    }
    else{
        vflags[MAIN_BIT]=1;
        mode="div";
    }

    std::cout<<mode<<"---"<<opt_set<<std::endl;

    sseq::SeqInfo seq_info("",vflags);
    std::unique_ptr<sseq::SeqFactory> sseq_factory =
        std::make_unique<sseq::SeqFactory>(seq_info);// 接下来的操作在seq_action.h

    tool.run(sseq_factory.get());
    
    std::cout << "[exit]\n";
    return 0;
}

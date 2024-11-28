#include "action.h"

#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <iostream>
#include <llvm/Support/CommandLine.h>

static llvm::cl::OptionCategory MyASTSlicer_category("myastslicer options");
static llvm::cl::extrahelp
    common_help(clang::tooling::CommonOptionsParser::HelpMessage);

    //commandLine Arguments
static llvm::cl::opt<std::string> path_info_f("inst", llvm::cl::desc("path_info_file"),
                                llvm::cl::cat(MyASTSlicer_category));
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
    std::string module = argv[2];            //argv[2]是插桩模式
    if(module!="div" && module!="print")
        module="div";                        //default
    sseq::SeqInfo seq_info(module);
    std::unique_ptr<sseq::SeqFactory> sseq_factory =
        std::make_unique<sseq::SeqFactory>(seq_info);// 接下来的操作在seq_action.h

    tool.run(sseq_factory.get());
    
    std::cout << "[exit]\n";
    return 0;
}

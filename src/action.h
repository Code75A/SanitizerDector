#ifndef FCOSAL_ACTION_H
#define FCOSAL_ACTION_H

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Stmt.h"

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "seq_info.h"
#include "tool.h"

#define FLAG_WIDTH 2
#define MAIN_BIT 0
#define OPT_BIT 1

namespace sseq
{
static void replace_suffix(std::string &fn, std::string with)
{
    int index = fn.find_last_of(".");
    fn        = fn.substr(0, index) + with+fn.substr(index);
}
class SeqASTVisitor : public clang::RecursiveASTVisitor<SeqASTVisitor>
{
    private:
        clang::ASTContext *_ctx;
        clang::Rewriter &_rewriter;
        SeqInfo &seq_info;
    public:
    explicit SeqASTVisitor(clang::ASTContext *ctx,
                            clang::Rewriter &R,
                            SeqInfo &seq_i)
        : _ctx(ctx), _rewriter(R), seq_info{ seq_i }
    {
    }
    ~SeqASTVisitor(){
    }

    bool isInSystemHeader(const clang::SourceLocation &loc);
    clang::SourceRange get_sourcerange(clang::Stmt *stmt);
    clang::SourceRange get_decl_sourcerange(clang::Decl *stmt);
    bool VisitFunctionDecl(clang::FunctionDecl *func_decl); //遍历函数节点

    bool getFlag(int index){
      if(index>=FLAG_WIDTH)
        return false;
      else return seq_info.vflags[index];
    };

    void judgePrint(const clang::BinaryOperator *bop,clang::SourceManager& SM,const int c,clang::SourceLocation &loc);
    void judgeDiv(const clang::BinaryOperator *bop,std::string* insertStr,int &count,clang::SourceManager& SM,const int c);
};
class SeqASTConsumer : public clang::ASTConsumer
{
  public:
    explicit SeqASTConsumer(clang::CompilerInstance &CI,
                             clang::Rewriter &R,
                             SeqInfo &seq_i)
        :  _visitor(&CI.getASTContext(), R, seq_i)
    {
    }

    virtual void HandleTranslationUnit(clang::ASTContext &ctx)
    {
        _visitor.TraverseDecl(ctx.getTranslationUnitDecl());//这句话开始触发Visit系列函数
    }

  private:
    SeqASTVisitor _visitor;
};

class SeqFrontendAction : public clang::ASTFrontendAction
{
  public:
    SeqFrontendAction(SeqInfo &seq_i) : seq_info{ seq_i }
    {}
    virtual std::unique_ptr<clang::ASTConsumer>
        CreateASTConsumer(clang::CompilerInstance &compiler,
                          llvm::StringRef in_file)
    {
         _rewriter.setSourceMgr(compiler.getSourceManager(),
                               compiler.getLangOpts());
        return std::make_unique<SeqASTConsumer>(
            compiler, _rewriter, seq_info);
    }
    //写文件 
    void EndSourceFileAction() override
    {
        clang::SourceManager &SM = _rewriter.getSourceMgr();
        std::error_code ec;
        std::string file_name =
            SM.getFileEntryForID(SM.getMainFileID())->getName().str();

        //更改后缀
        if(seq_info.vflags[MAIN_BIT]){
          if(seq_info.vflags[OPT_BIT])
            replace_suffix(file_name, "_print");
          else replace_suffix(file_name, "_div");
        }
        else
          replace_suffix(file_name, "_test");
          
        
        llvm::raw_fd_stream fd(file_name, ec);
        _rewriter.getEditBuffer(SM.getMainFileID()).write(fd);
    }

  private:
    SeqInfo &seq_info;
    clang::Rewriter _rewriter;//这个类可以编辑源代码
};


class SeqFactory : public clang::tooling::FrontendActionFactory
{
  public:
    SeqFactory(SeqInfo &seq_i) : seq_info{ seq_i }{
    }
    std::unique_ptr<clang::FrontendAction> create() override
    {
        return std::make_unique<SeqFrontendAction>(seq_info);
    }

  private:
    SeqInfo &seq_info;
};
}  // namespace sseq
#endif
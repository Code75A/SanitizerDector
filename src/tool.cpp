#include "tool.h"

namespace sseq{

    bool Tool::isIdentifierPart(char ch) {
        return std::isalpha(ch) || std::isdigit(ch) || (ch == '_');
    }
    std::string Tool::get_stmt_string(const clang::Stmt *S){
        if(S==nullptr){
            return "0";
        }
        clang::LangOptions LO;
        std::string buffer;
        llvm::raw_string_ostream strout(buffer);
        S->printPretty(strout, nullptr, clang::PrintingPolicy(LO));
        return strout.str(); 
    }
    std::string Tool::get_decl_string(const clang::Decl *S){
        if(S==nullptr){
            return "0";
        }
        clang::LangOptions LO=S->getASTContext().getLangOpts();
        std::string buffer;
        llvm::raw_string_ostream strout(buffer);
        S->print(strout, clang::PrintingPolicy(LO));
        std::string ret=strout.str();
        return ret;
    }
    std::string Tool::get_qualType_string(const clang::QualType &T){//这里改成了传递引用
        if(T.isNull()){
            return "0";
        }
        clang::LangOptions LO;
        std::string buffer;
        llvm::raw_string_ostream strout(buffer);
        T.getLocalUnqualifiedType().getNonReferenceType().print(strout, clang::PrintingPolicy(LO));
        return strout.str();
    }

    // 判断类型定义是否为函数指针类型
    bool Tool::isFunctionPointerTypedef(const clang::TypedefDecl *TD) {
        return TD->getUnderlyingType()->isFunctionPointerType();
    }
    
} //namespace sseq

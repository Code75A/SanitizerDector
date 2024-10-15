#ifndef TOOL_H
#define TOOL_H

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"

#include <clang/AST/Decl.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/Tooling.h>
#include <iostream>
#include <unistd.h>

namespace sseq{

//
class Tool{
public:
    static bool isIdentifierPart(char ch);
    static std::string get_stmt_string(const clang::Stmt *S);
    static std::string get_decl_string(const clang::Decl *S);
    static std::string get_qualType_string(const clang::QualType &T);
    // 判断类型定义是否为函数指针类型
    static bool isFunctionPointerTypedef(const clang::TypedefDecl *TD);
};
    
} //namespace sseq
#endif
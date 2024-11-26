#include "action.h"
namespace sseq
{

    bool SeqASTVisitor::isInSystemHeader(const clang::SourceLocation &loc) {
        auto &SM = _ctx->getSourceManager();
        if(SM.getFilename(loc)=="") return true;
        std::string filename = SM.getFilename(loc).str();
        if(filename.find("/usr/include/")!=std::string::npos) return true;
        if(filename.find("llvm-12.0.0.obj/")!=std::string::npos) return true; //结合实际情况 当前工具开发使用llvm12
        return false;
    }
    clang::SourceRange SeqASTVisitor::get_decl_sourcerange(clang::Decl *stmt){
        clang::SourceRange SR = stmt->getSourceRange();
        auto &SM = _ctx->getSourceManager();
        auto &LO = _ctx->getLangOpts();
        clang::SourceLocation  loc = SR.getBegin();
        clang::SourceLocation  end = SR.getEnd();
        if(loc.isMacroID()){
            clang::FileID FID = SM.getFileID(loc);
            const clang::SrcMgr::SLocEntry *Entry = &SM.getSLocEntry(FID);
            while (Entry->getExpansion().getExpansionLocStart().isMacroID()) {
                loc = Entry->getExpansion().getExpansionLocStart();
                FID = SM.getFileID(loc);
                Entry = &SM.getSLocEntry(FID);
            }
            loc = SM.getExpansionLoc(loc);
            if(end.isMacroID()){
                FID = SM.getFileID(end);
                const clang::SrcMgr::SLocEntry *Entry = &SM.getSLocEntry(FID);
                while (Entry->getExpansion().getExpansionLocStart().isMacroID()) {
                    end = Entry->getExpansion().getExpansionLocStart();
                    FID = SM.getFileID(end);
                    Entry = &SM.getSLocEntry(FID);
                }
                end = Entry->getExpansion().getExpansionLocEnd();
            }
            else end = SM.getExpansionLoc(end);
        }
        else if(end.isMacroID()){
            clang::FileID FID = SM.getFileID(end);
            const clang::SrcMgr::SLocEntry *Entry = &SM.getSLocEntry(FID);
            while (Entry->getExpansion().getExpansionLocStart().isMacroID()) {
                end = Entry->getExpansion().getExpansionLocStart();
                FID = SM.getFileID(end);
                Entry = &SM.getSLocEntry(FID);
            }
            end = Entry->getExpansion().getExpansionLocEnd();
        }
        
        
        return clang::SourceRange(loc,end);
    }
    //获得stmt的range
    clang::SourceRange SeqASTVisitor::get_sourcerange(clang::Stmt *stmt){
        clang::SourceRange SR = stmt->getSourceRange();
        auto &SM = _ctx->getSourceManager();
        auto &LO = _ctx->getLangOpts();
        clang::SourceLocation  loc = SR.getBegin();
        clang::SourceLocation  end = SR.getEnd();
        if(loc.isMacroID()){
            clang::FileID FID = SM.getFileID(loc);
            const clang::SrcMgr::SLocEntry *Entry = &SM.getSLocEntry(FID);
            while (Entry->getExpansion().getExpansionLocStart().isMacroID()) {
                loc = Entry->getExpansion().getExpansionLocStart();
                FID = SM.getFileID(loc);
                Entry = &SM.getSLocEntry(FID);
            }
            loc = SM.getExpansionLoc(loc);
            if(end.isMacroID()){
                FID = SM.getFileID(end);
                const clang::SrcMgr::SLocEntry *Entry = &SM.getSLocEntry(FID);
                while (Entry->getExpansion().getExpansionLocStart().isMacroID()) {
                    end = Entry->getExpansion().getExpansionLocStart();
                    FID = SM.getFileID(end);
                    Entry = &SM.getSLocEntry(FID);
                }
                end = Entry->getExpansion().getExpansionLocEnd();
            }
            else end = SM.getExpansionLoc(end);
        }
        else if(end.isMacroID()){
            clang::FileID FID = SM.getFileID(end);
            const clang::SrcMgr::SLocEntry *Entry = &SM.getSLocEntry(FID);
            while (Entry->getExpansion().getExpansionLocStart().isMacroID()) {
                end = Entry->getExpansion().getExpansionLocStart();
                FID = SM.getFileID(end);
                Entry = &SM.getSLocEntry(FID);
            }
            end = Entry->getExpansion().getExpansionLocEnd();
        }
        
        return clang::SourceRange(loc,end);

    }

    void generateArray(int n,std::string v_name,std::string *insertStr){
        std::string num=std::to_string(n);
        std::string a="SaniTestArr";
        *insertStr+=("\n\tSaniCatcher"+num+"=SaniTestArr"+num+"["+v_name+"-1];\n");
        return ;
    }

    std::string initSani(int n){
        if(n==0) return "";

        std::string Arr="int SaniTestArr0[10]";
        for(int i=1;i<n;i++){
            Arr+=",SaniTestArr"+std::to_string(i)+"[10]";
        }
        Arr+=";\n";

        std::string Cat="int SaniCatcher0";
        for(int i=1;i<n;i++){
            Cat+=",SaniCatcher"+std::to_string(i);
        }
        Cat+=";\n";

        return Arr+Cat;
    }

    void SeqASTVisitor::judgeDiv(const clang::BinaryOperator *bop,std::string* insertStr,int &count,clang::SourceManager& SM,const int c){
        if(bop == nullptr)
        {
            std::cout<<"warnning:encounter nullptr,maybe not a binaryoperator\n";
            return ;
        }

        clang::BinaryOperator::Opcode op = bop->getOpcode();

        clang::Expr *lhs=bop->getLHS();clang::Expr *rhs=bop->getRHS();

        std::string prt=" print(\"XXX\") && ";
        if(op == clang::BinaryOperator::Opcode::BO_LAnd || SM.getSpellingColumnNumber(rhs->getBeginLoc())>c){
            _rewriter.InsertTextBefore(rhs->getBeginLoc(),prt);
        }

        //std::cout<<"Class:"<<lhs->getStmtClassName()<<"--"<<rhs->getStmtClassName()<<std::endl;


        if((op == clang::BinaryOperator::Opcode::BO_Div || op == clang::BinaryOperator::Opcode::BO_Rem) && 
        (SM.getSpellingColumnNumber(rhs->getEndLoc())>c && SM.getSpellingColumnNumber(rhs->getBeginLoc())<c ))
        {
            std::cout<<"check"<<SM.getSpellingColumnNumber(rhs->getBeginLoc())<<std::endl;
            std::cout<<"check"<<SM.getSpellingColumnNumber(rhs->getEndLoc())<<std::endl;
            generateArray(count++,Tool::get_stmt_string(rhs),insertStr);
        }
        //std::cout<<"check"<<SM.getSpellingColumnNumber(lhs->getEndLoc())<<"  ";
        
        std::cout<<"check lhs:"<<Tool::get_stmt_string(lhs)<<std::endl;
        if((Tool::get_stmt_string(lhs).find('/')!=-1 || Tool::get_stmt_string(lhs).find('%')!=-1) && SM.getSpellingColumnNumber(lhs->getEndLoc()) >= c){
            std::cout<<"check lhs:"<<Tool::get_stmt_string(lhs)<<std::endl;
            
            const clang::BinaryOperator *bopl=llvm::dyn_cast<clang::BinaryOperator>(lhs);
            while(!bopl)
            {
                std::string type=lhs->getStmtClassName();
                std::cout<<"lhs type:"<<type<<std::endl;
                if(type=="ParenExpr"){
                    clang::ParenExpr *plhs = llvm::dyn_cast<clang::ParenExpr>(lhs);
                    lhs=plhs->getSubExpr();
                }
                else if(type=="ImplicitCastExpr"){
                    clang::ImplicitCastExpr *plhs = llvm::dyn_cast<clang::ImplicitCastExpr>(lhs);
                    lhs=plhs->getSubExpr();

                    std::cout<<Tool::get_stmt_string(lhs)<<std::endl;
                    std::cout<<"Class:"<<lhs->getStmtClassName()<<std::endl;
                }
                else if(type=="UnaryOperator"){
                    clang::UnaryOperator *plhs = llvm::dyn_cast<clang::UnaryOperator>(lhs);
                    lhs=plhs->getSubExpr();
                }
                else if(type=="CStyleCastExpr"){
                    clang::CStyleCastExpr *plhs = llvm::dyn_cast<clang::CStyleCastExpr>(lhs);
                    lhs=plhs->getSubExpr();
                }
                else if(type=="CallExpr"){
                    for (auto &exp : lhs->children() )
                        {
                            if(SM.getSpellingColumnNumber(exp->getEndLoc()) >= c)
                            {
                                lhs=llvm::dyn_cast<clang::Expr>(exp);
                                std::cout<<Tool::get_stmt_string(lhs)<<std::endl;
                                break;
                           }
                        }
                }


                bopl=llvm::dyn_cast<clang::BinaryOperator>(lhs);
            }
            //std::cout<<"SUCC:"<<Tool::get_stmt_string(lhs)<<std::endl;
                judgeDiv(bopl,insertStr,count,SM,c);
        }

        std::cout<<"check rhs:"<<Tool::get_stmt_string(rhs)<<std::endl;
        if((Tool::get_stmt_string(rhs).find('/')!=-1 || Tool::get_stmt_string(rhs).find('%')!=-1) && SM.getSpellingColumnNumber(rhs->getBeginLoc()) <= c){
            std::cout<<"check rhs:"<<Tool::get_stmt_string(rhs)<<std::endl;

            const clang::BinaryOperator *bopr=llvm::dyn_cast<clang::BinaryOperator>(rhs);
            while(!bopr)
            {
                std::string type=rhs->getStmtClassName();
                std::cout<<"rhs type:"<<type<<std::endl;
                if(type=="ParenExpr"){
                    clang::ParenExpr *prhs = llvm::dyn_cast<clang::ParenExpr>(rhs);
                    rhs=prhs->getSubExpr();
                }
                else if(type=="ImplicitCastExpr"){
                    clang::ImplicitCastExpr *prhs = llvm::dyn_cast<clang::ImplicitCastExpr>(rhs);
                    rhs=prhs->getSubExpr();
                }
                else if(type=="UnaryOperator"){
                    clang::UnaryOperator *prhs = llvm::dyn_cast<clang::UnaryOperator>(rhs);
                    rhs=prhs->getSubExpr();
                }
                else if(type=="CStyleCastExpr"){
                    clang::CStyleCastExpr *prhs = llvm::dyn_cast<clang::CStyleCastExpr>(rhs);
                    rhs=prhs->getSubExpr();
                }
                else if(type=="CallExpr"){
                    for (auto &exp : rhs->children() )
                        {
                            if(SM.getSpellingColumnNumber(exp->getEndLoc()) >= c)
                            {
                                rhs=llvm::dyn_cast<clang::Expr>(exp);
                                std::cout<<Tool::get_stmt_string(rhs)<<std::endl;
                                break;
                           }
                        }
                }


                bopr=llvm::dyn_cast<clang::BinaryOperator>(rhs);
            }
            //std::cout<<"SUCC:"<<Tool::get_stmt_string(rhs)<<std::endl;
                judgeDiv(bopr,insertStr,count,SM,c);
            
        }
        return ;
    }

    void find_UB(int &l,int &c,const std::string& fname){
        
        std::ifstream file(fname);
        if(!file){
            std::cout<<"file not exists"<<std::endl;
            return ;
        }

        l=0;
        std::string cur;

        while(std::getline(file,cur)){
            l++;
            if(cur.find("/*UBFUZZ*/") != -1){
                c=cur.find("/*UBFUZZ*/");
                std::cout<<"line:"<<l<<" column:"<<c<<std::endl;
                return ;
            }
            
        }

        l=-1;
        return;

    }

    bool SeqASTVisitor::VisitFunctionDecl(clang::FunctionDecl *func_decl){
        if (!_ctx->getSourceManager().isInMainFile(func_decl->getLocation()))
                return true;
        if (isInSystemHeader(func_decl->getLocation()))
            return true;
            
            
    //    func_decl->dump();
        
        
        clang::SourceManager &SM = _ctx->getSourceManager();
        std::string filePath_abs = SM.getFilename(func_decl->getLocation()).str(); //绝对路径
        std::string filePath =filePath_abs;
        if(filePath.find("src/")!=std::string::npos){
            int pos = filePath.find("src/");
            size_t idx = filePath.find_last_of("/", pos);
        
            if (idx != std::string::npos) {
                // 提取从最后一个 '/' 到字符串末尾的子字符串
                filePath = filePath.substr(idx + 1);
            }
        }

        int UBFUZZ_line=-1;
        int UBFUZZ_column=-1;
        static bool fir=false;
        find_UB(UBFUZZ_line,UBFUZZ_column,filePath);

        if(UBFUZZ_line==-1){
            std::cout<<"Cant find a UBFUZZ sign"<<std::endl;
        }
        else{
            std::cout<<"UBFUZZ in line"<<UBFUZZ_line<<std::endl;
        }

        std::string func_name = func_decl->getNameAsString();
        std::cout<<"\n| Function [" << func_decl->getNameAsString() << "] defined in: " << filePath << "\n";

        

        if (func_decl == func_decl->getDefinition())
        {
            clang::Stmt *body_stmt = func_decl->getBody();
            int count=0;
            for (auto &stmt : body_stmt->children())
            {
                //遍历这个函数中的语句
                //在第一个语句之前插入内容X
                
                //_rewriter.InsertTextAfter(stmt->getEndLoc(),s);
                
                // 方法1： 在此处判断语句类型 如果是有子语句的语句（如for\if\switch\while)则遍历其子语句查找除法运算和模运算 
                // 可以通过get_stmt_string 可以输出该语句的内容，通过判断是否存在"/"或"%"字符来进行一次筛选
                if(SM.getSpellingLineNumber(stmt->getEndLoc()) >= UBFUZZ_line && !fir){

                    fir=true;

                    std::cout<<Tool::get_stmt_string(stmt)<<std::endl;
                    std::string type=stmt->getStmtClassName();
                    std::cout<<"type:"<<type<<std::endl;

                        while(type == "ForStmt" || type == "IfStmt")
                        {
                            fir=false;
                            if(type=="ForStmt"){
                                clang::ForStmt *FS = llvm::dyn_cast<clang::ForStmt>(stmt);
                                clang::Stmt *sbody=FS->getBody();
                                //std::cout<<sbody->getStmtClassName()<<std::endl;
                                    for (auto &s_stmt : sbody->children() )
                                    {
                                        // std::cout<<1;
                                        // std::cout<<Tool::get_stmt_string(s_stmt);
                                        // std::cout<<"STMT:"<<SM.getSpellingLineNumber(s_stmt->getEndLoc())<<std::endl;
                                        if(SM.getSpellingLineNumber(s_stmt->getEndLoc()) >= UBFUZZ_line)
                                        {
                                            fir=true;
                                            stmt=s_stmt;

                                            type=stmt->getStmtClassName();
                                            std::cout<<"type:"<<type<<std::endl;
                                            break;
                                        }
                                    }
                            }
                            else if(type == "IfStmt"){
                                clang::IfStmt *IS = llvm::dyn_cast<clang::IfStmt>(stmt);
                                bool found=false;

                                clang::Stmt *ibody=IS->getCond();
                                for (auto &s_stmt : ibody->children() ){
                                    std::cout<<"STMT:"<<SM.getSpellingLineNumber(s_stmt->getEndLoc())<<std::endl;
                                            if(SM.getSpellingLineNumber(s_stmt->getEndLoc()) >= UBFUZZ_line)
                                            {
                                                fir=true;
                                                stmt=s_stmt;

                                                type=stmt->getStmtClassName();
                                                std::cout<<"type:"<<type<<std::endl;
                                                found=true;
                                                break;
                                            }
                                }

                                if(!found){
                                        ibody=IS->getThen();
                                    for (auto &s_stmt : ibody->children() ){
                                            std::cout<<"STMT:"<<SM.getSpellingLineNumber(s_stmt->getEndLoc())<<std::endl;
                                            if(SM.getSpellingLineNumber(s_stmt->getEndLoc()) >= UBFUZZ_line)
                                            {
                                                fir=true;
                                                stmt=s_stmt;

                                                type=stmt->getStmtClassName();
                                                std::cout<<"type:"<<type<<std::endl;
                                                found=true;
                                                break;
                                            }
                                        }
                                }
                                
                                if(!found){
                                        ibody = IS->getElse();
                                    if(ibody)
                                        for (auto &s_stmt : ibody->children() ){
                                                // std::cout<<1;
                                                // std::cout<<Tool::get_stmt_string(s_stmt);
                                                std::cout<<"STMT:"<<SM.getSpellingLineNumber(s_stmt->getEndLoc())<<std::endl;
                                                if(SM.getSpellingLineNumber(s_stmt->getEndLoc()) >= UBFUZZ_line){
                                                    fir=true;
                                                    stmt=s_stmt;

                                                    type=stmt->getStmtClassName();
                                                    std::cout<<"type:"<<type<<std::endl;
                                                    break;
                                                }
                                            }
                                }

                                
                            }

                            std::cout<<Tool::get_stmt_string(stmt)<<std::endl;
                        }

                        if(const clang::BinaryOperator *bop = llvm::dyn_cast<clang::BinaryOperator>(stmt)){// 这样是判断是不是二元运算
                        
                        clang::BinaryOperator::Opcode op = bop->getOpcode();//运算符，如op==clang::BinaryOperator::Opcode::BO_Comma 可以判断运算符是不是逗号
                        
                        clang::Expr *lhs = bop->getLHS(); clang::Expr *rhs = bop->getRHS();

                        std::string* insertStr=new std::string;
                        *insertStr="";
                        
                        std::string stmt_string=Tool::get_stmt_string(stmt);
                        if(stmt_string.find('/')!=-1 || stmt_string.find('%')!=-1){
                            judgeDiv(bop,insertStr,count,SM,UBFUZZ_column);
                        }
                        insertStr->pop_back();
                        *insertStr += "/*A_QUITE_UNIQUE_FLAG*/\n";
                        _rewriter.InsertTextBefore(stmt->getBeginLoc(),*insertStr);
                        }
                    //    if(lhs==nullptr || rhs==nullptr) return false;//防止空指针
                    //   std::string lhs_class = lhs->getStmtClassName(),rhs_class = rhs->getStmtClassName();//需要考虑运算符左右子语句可能也是二元运算。所以可能需要设计递归函数
                        
                }
               
            }
            _rewriter.InsertTextBefore(func_decl->getBeginLoc(),initSani(count));
        }

        
        return true;
    }

};

/*
                    if(op==clang::BinaryOperator::Opcode::BO_Assign)
                    {
                        clang::Expr *lhs = bop->getLHS(); clang::Expr *rhs = bop->getRHS();//获取右边的
                        
                        std::cout<<Tool::get_stmt_string(lhs)<<"="<<Tool::get_stmt_string(rhs)<<std::endl;

                        if(const clang::BinaryOperator *bopsec =llvm::dyn_cast<clang::BinaryOperator>(rhs)){
                            clang::BinaryOperator::Opcode opsec = bopsec->getOpcode();
                            clang::Expr *rhssec = bopsec->getRHS();//获取右边的

                            if(opsec==clang::BinaryOperator::Opcode::BO_Div){

                                _rewriter.InsertTextBefore(stmt->getBeginLoc(),
                                generateArray(count++,Tool::get_stmt_string(rhssec)));
                            }
                        }
                        
                    }

                    */


//方法2： 可以尝试写一个VisitBinaryOperator，再看看能不能通过这个节点找节点获取到对应的函数节点

               //一些API：get_stmt_string输出语句内容，
               //获取SourceLocation对应的行列：  int line= SM.getPresumedLineNumber(Loc), col = SM.getPresumedColumnNumber(Loc); 
               // 在loc之后插入str： _rewriter.InsertTextAfter(loc,str);
               // 遍历子语句直接auto， IF语句获取为true的部分是ifstmt->getThen()， 其他语句具体查询clang的文档。if while这些语句还要考虑条件，也可以查询文档得知如何获取。

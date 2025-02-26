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

    //生成第n对SaniCatcher和SaniTestArr,存入insertStr，以mode调控（如果需要添加更多模式，请更改mode）
    void generateArray(int n,std::string v_name,std::string *insertStr,const std::string mode){
        
        std::string num=std::to_string(n);
        //std::string a="SaniTestArr";
        if(mode=="div")
            *insertStr=("(SaniCatcher"+num+"=SaniTestArr"+num+"[abs("+v_name+")-1]),");
        else if(mode=="shf")
            *insertStr=("\n\tSaniCatcher=SaniTestArr["+v_name+"];\n");
        else
            std::cout<<"Error: generateArray encounter invalid mode type."<<std::endl;
        return ;
    }
    //生成第lst_n~n对SaniCatcher和SaniTestArr的定义字符串
    std::string initSani(int lst_n,int n){

        std::cout<<"Sanicount:"<<n<<std::endl;

        if(lst_n==n) return "";

        std::string Arr="int SaniTestArr"+std::to_string(lst_n)+"[10]";
        for(int i=lst_n+1;i<n;i++){
            Arr+=",SaniTestArr"+std::to_string(i)+"[10]";
        }
        Arr+=";\n";

        std::string Cat="int SaniCatcher"+std::to_string(lst_n);
        for(int i=lst_n+1;i<n;i++){
            Cat+=",SaniCatcher"+std::to_string(i);
        }
        Cat+=";\n";

        return Arr+Cat;
    }
    //生成shift模式下SaniCatcher和SaniTestArr的定义字符串,位移bits位
    std::string initSani_shf(int bits){
        if(bits==-1) return "";

        std::string num=std::to_string(bits);
        std::string Arr="int SaniTestArr["+num+"];\n";

        std::string Cat="int SaniCatcher;\n";

        return Arr+Cat;
    }

    //true:str含/或%
    bool PosDivideZero(std::string str){
        return str.find('/')!=-1 || str.find('%')!=-1;
    }

    //TODO:注释
    void GetSubExpr(clang::Stmt *&stmt,clang::Stmt *&ori_stmt, std::string type,bool &fir,clang::SourceManager& SM,int &UBFUZZ_line){
        while(type == "ForStmt" || type == "IfStmt"){
            fir=false;
            if(type=="ForStmt"){
                clang::ForStmt *FS = llvm::dyn_cast<clang::ForStmt>(stmt);
                clang::Stmt *sbody=FS->getBody();
                //std::cout<<sbody->getStmtClassName()<<std::endl;
                for (auto &s_stmt : sbody->children() ){
                    // std::cout<<Tool::get_stmt_string(s_stmt);
                    // std::cout<<"STMT:"<<SM.getSpellingLineNumber(s_stmt->getEndLoc())<<std::endl;
                    if(SM.getSpellingLineNumber(s_stmt->getEndLoc()) >= UBFUZZ_line){
                        fir=true;
                        stmt=s_stmt;
                        ori_stmt=stmt;
                        type=stmt->getStmtClassName();
                        //std::cout<<"type:"<<type<<std::endl;
                        break;
                    }
                }
            }
            else if(type == "IfStmt"){
            //查询条件
                clang::IfStmt *IS = llvm::dyn_cast<clang::IfStmt>(stmt);
                bool found=false;
                clang::Stmt *ibody=IS->getCond();
                for (auto &s_stmt : ibody->children() ){
                //std::cout<<"STMT:"<<SM.getSpellingLineNumber(s_stmt->getEndLoc())<<std::endl;
                    if(SM.getSpellingLineNumber(s_stmt->getEndLoc()) >= UBFUZZ_line){
                        fir=true;
                        stmt=s_stmt;
                        type=stmt->getStmtClassName();
                        //std::cout<<"type:"<<type<<std::endl;
                        found=true;
                        break;
                    }
                }
                //查询then分支
                if(!found){
                ibody=IS->getThen();
                for (auto &s_stmt : ibody->children() ){
                    //std::cout<<"STMT:"<<SM.getSpellingLineNumber(s_stmt->getEndLoc())<<std::endl;
                    if(SM.getSpellingLineNumber(s_stmt->getEndLoc()) >= UBFUZZ_line){
                        fir=true;
                        stmt=s_stmt;
                        ori_stmt=stmt;
                        type=stmt->getStmtClassName();
                        //std::cout<<"type:"<<type<<std::endl;
                        found=true;
                        break;
                    }
                }
                }
                //查询else分支
                if(!found){
                   ibody = IS->getElse();
                if(ibody)
                for (auto &s_stmt : ibody->children() ){
                    //std::cout<<Tool::get_stmt_string(s_stmt);
                    //std::cout<<"STMT:"<<SM.getSpellingLineNumber(s_stmt->getEndLoc())<<std::endl;
                    if(SM.getSpellingLineNumber(s_stmt->getEndLoc()) >= UBFUZZ_line){
                        fir=true;
                        stmt=s_stmt;
                        ori_stmt=stmt;
                        type=stmt->getStmtClassName();
                        //std::cout<<"type:"<<type<<std::endl;
                        break;
                    }
                }
                }
                //查询结束
                }
            //std::cout<<Tool::get_stmt_string(stmt)<<std::endl;
        }
    }
    //穷举UBFUZZ mode所有需要拆括号（提取SubExpr）的情况，并用SubExpr替换Expr
    //TODO 
    void GetSimplifiedExprUBFuzz(){

    }
    //穷举所有需要拆括号（提取SubExpr）的情况，并用SubExpr替换Expr
    void GetSimplifiedExpr(clang::Expr *&hs,const clang::BinaryOperator *&bop,std::string type){
        while(!bop){
            std::cout<<"nop bop"<<std::endl;
            type=hs->getStmtClassName();
                
            if(type=="ParenExpr"){
                clang::ParenExpr *phs = llvm::dyn_cast<clang::ParenExpr>(hs);
                hs=phs->getSubExpr();

                std::cout<<Tool::get_stmt_string(hs)<<std::endl;
            }
            else if(type=="ImplicitCastExpr"){
                clang::ImplicitCastExpr *phs = llvm::dyn_cast<clang::ImplicitCastExpr>(hs);
                hs=phs->getSubExpr();
                
                std::cout<<Tool::get_stmt_string(hs)<<std::endl;
                //std::cout<<"Class:"<<hs->getStmtClassName()<<std::endl;
            }
            else if(type=="UnaryOperator"){
                clang::UnaryOperator *phs = llvm::dyn_cast<clang::UnaryOperator>(hs);
                hs=phs->getSubExpr();

                std::cout<<Tool::get_stmt_string(hs)<<std::endl;
            }
            else if(type=="CStyleCastExpr"){
                clang::CStyleCastExpr *plhs = llvm::dyn_cast<clang::CStyleCastExpr>(hs);
                hs=plhs->getSubExpr();

                std::cout<<Tool::get_stmt_string(hs)<<std::endl;
            }
            else
            {
                std::cout<<"base:"<<type<<std::endl;
                bop=llvm::dyn_cast<clang::BinaryOperator>(hs);
                return;
            }

            bop=llvm::dyn_cast<clang::BinaryOperator>(hs);
        }
        std::cout<<"clear"<<std::endl;
        return ;
    }
    
    //TODO:注释
    void SeqASTVisitor::judgeShf(const clang::BinaryOperator *bop,const clang::BinaryOperator* last_bop,std::string* insertStr,clang::SourceManager& SM,int& bits){
        if(bop == nullptr){
            std::cout<<"warnning:encounter nullptr,maybe not a binaryoperator\n";
            return ;
        }

        //c makes nonsense

        clang::BinaryOperator::Opcode op = bop->getOpcode();
        clang::Expr *lhs=bop->getLHS();clang::Expr *rhs=bop->getRHS();

        if(Tool::get_stmt_string(lhs).find("+ (MUT_VAR)")!=-1){
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
                    for (auto &exp : lhs->children() ){;}
                    std::cout<<"Debug: Unexpected CallExpr."<<std::endl;
                }
                bopl=llvm::dyn_cast<clang::BinaryOperator>(lhs);
            }
            //std::cout<<"SUCC:"<<Tool::get_stmt_string(lhs)<<std::endl;
                judgeShf(bopl,bop,insertStr,SM,bits);
        }
        else if(Tool::get_stmt_string(rhs).find("+ (MUT_VAR)")!=-1){
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
                    for (auto &exp : rhs->children() ){
                            for (auto &exp : rhs->children() ){;}
                            std::cout<<"Debug: Unexpected CallExpr."<<std::endl;
                        }
                }


                bopr=llvm::dyn_cast<clang::BinaryOperator>(rhs);
            }
            //std::cout<<"SUCC:"<<Tool::get_stmt_string(rhs)<<std::endl;
                judgeShf(bopr,bop,insertStr,SM,bits);
        }
        else{
            std::cout<<"then rhs==mutvar:"<<Tool::get_stmt_string(rhs)<<std::endl;
            clang::Expr *fin_lhs=last_bop->getLHS();clang::Expr *fin_rhs=last_bop->getRHS();

            std::string type=fin_lhs->getStmtClassName();
            while(type=="ParenExpr"){
                clang::ParenExpr *plhs = llvm::dyn_cast<clang::ParenExpr>(fin_lhs);
                fin_lhs=plhs->getSubExpr();
                type=fin_lhs->getStmtClassName();
            }


            if(type=="ImplicitCastExpr"||type=="CStyleCastExpr"){
                bits=_ctx->getTypeSize(fin_lhs->getType());
            }
            else {
                std::cout<<"type:"<<type<<std::endl;
                bits=-1;
                std::cout<<"Debug: Unexpected situation when generate shf_Array."<<std::endl;
            }

            std::cout<<"Fin_Type:"<<fin_lhs->getType().getAsString()<<std::endl;

            generateArray(1,Tool::get_stmt_string(fin_rhs),insertStr,"shf");

        }
        
        return ;
    }
    //TODO:注释
    void SeqASTVisitor::judgePrint(const clang::BinaryOperator *bop,clang::SourceManager& SM,const int c,clang::SourceLocation &loc){
        if(bop == nullptr){
            std::cout<<"warnning:encounter nullptr,maybe not a binaryoperator\n";
            return ;
        }

        clang::BinaryOperator::Opcode op = bop->getOpcode();
        clang::Expr *lhs=bop->getLHS();clang::Expr *rhs=bop->getRHS();


        llvm::StringRef opcodeStr = clang::BinaryOperator::getOpcodeStr(op);
        std::cout << "Opcode: " << opcodeStr.str() << std::endl;
        printf("%d vs %d\n",SM.getSpellingColumnNumber(rhs->getBeginLoc()),c);

        

        if(op==clang::BinaryOperator::Opcode::BO_LAnd && SM.getSpellingColumnNumber(rhs->getBeginLoc())<c && SM.getSpellingColumnNumber(rhs->getEndLoc())>=c){
            loc=rhs->getBeginLoc();
            printf("update loc: %d\n",SM.getSpellingColumnNumber(loc));
        }

        std::cout<<"check lhs:"<<Tool::get_stmt_string(lhs)<<std::endl;
        if((PosDivideZero(Tool::get_stmt_string(lhs))) && SM.getSpellingColumnNumber(lhs->getEndLoc()) >= c){
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
                judgePrint(bopl,SM,c,loc);
        }

        std::cout<<"check rhs:"<<Tool::get_stmt_string(rhs)<<std::endl;
        if((PosDivideZero(Tool::get_stmt_string(rhs))) && SM.getSpellingColumnNumber(rhs->getBeginLoc()) < c){
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
                judgePrint(bopr,SM,c,loc);
            
        }
        return ;

    }
    //TODO:注释
    void SeqASTVisitor::judgeDiv(const clang::BinaryOperator *bop,std::string* insertStr,int &count,clang::SourceManager& SM,const int c,clang::SourceLocation &loc){
        if(bop == nullptr){
            std::cout<<"warnning:encounter nullptr,maybe not a binaryoperator\n";
            return ;
        }

        clang::BinaryOperator::Opcode op = bop->getOpcode();
        clang::Expr *lhs=bop->getLHS();clang::Expr *rhs=bop->getRHS();
        
        if((op == clang::BinaryOperator::Opcode::BO_Div || op == clang::BinaryOperator::Opcode::BO_Rem) && 
        (SM.getSpellingColumnNumber(rhs->getEndLoc())>c && SM.getSpellingColumnNumber(rhs->getBeginLoc())<c ))
        {
            std::cout<<"check"<<SM.getSpellingColumnNumber(rhs->getBeginLoc())<<std::endl;
            std::cout<<"check"<<SM.getSpellingColumnNumber(rhs->getEndLoc())<<std::endl;
            count=1;
            generateArray(0,Tool::get_stmt_string(rhs),insertStr,"div");

            loc=lhs->getBeginLoc();
        }
        
        std::cout<<"check lhs:"<<Tool::get_stmt_string(lhs)<<std::endl;
        if((PosDivideZero(Tool::get_stmt_string(lhs))) && SM.getSpellingColumnNumber(lhs->getEndLoc()) >= c){
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
                judgeDiv(bopl,insertStr,count,SM,c,loc);
        }

        std::cout<<"check rhs:"<<Tool::get_stmt_string(rhs)<<std::endl;
        if((PosDivideZero(Tool::get_stmt_string(rhs))) && SM.getSpellingColumnNumber(rhs->getBeginLoc()) <= c){
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
                judgeDiv(bopr,insertStr,count,SM,c,loc);
            
        }
        return ;
    }
    //TODO:注释
    void SeqASTVisitor::judgeDivWithoutUBFuzz(const clang::BinaryOperator *bop,std::string* insertStr,int &count,clang::SourceManager& SM,clang::Rewriter &_rewriter){
        std::cout<<"judge_insert:"<<std::endl;
        if(bop == nullptr){
            std::cout<<"warnning:encounter nullptr,maybe not a binaryoperator\n";
            return ;
        }
        
        clang::BinaryOperator::Opcode op = bop->getOpcode();
        clang::Expr *lhs=bop->getLHS();clang::Expr *rhs=bop->getRHS();

        if(op == clang::BinaryOperator::Opcode::BO_Div || op == clang::BinaryOperator::Opcode::BO_Rem)
        {
            generateArray(count++,Tool::get_stmt_string(rhs),insertStr,"div");

            std::cout<<"count++"<<std::endl;

            _rewriter.InsertTextBefore(lhs->getBeginLoc(),*insertStr);
        }
        else
            std::cout<<"\topcode not match\n";

        std::cout<<"check lhs:"<<Tool::get_stmt_string(lhs)<<std::endl;
        if(PosDivideZero(Tool::get_stmt_string(lhs))){
            const clang::BinaryOperator *bopl=llvm::dyn_cast<clang::BinaryOperator>(lhs);
            std::string type;

            GetSimplifiedExpr(lhs,bopl,type);
            //std::cout<<"SUCC:"<<Tool::get_stmt_string(lhs)<<std::endl;
            if(bopl){
                std::string* ninsertStr=new std::string;
                *ninsertStr="";
                judgeDivWithoutUBFuzz(bopl,ninsertStr,count,SM,_rewriter);
            }
                
        }

        std::cout<<"check rhs:"<<Tool::get_stmt_string(rhs)<<std::endl;
        if(PosDivideZero(Tool::get_stmt_string(rhs))){
            const clang::BinaryOperator *bopr=llvm::dyn_cast<clang::BinaryOperator>(rhs);
            std::string type;

            GetSimplifiedExpr(rhs,bopr,type);
            //std::cout<<"SUCC:"<<Tool::get_stmt_string(rhs)<<std::endl;
            if(bopr){
                std::string* ninsertStr=new std::string;
                *ninsertStr="";
                judgeDivWithoutUBFuzz(bopr,ninsertStr,count,SM,_rewriter);
            }
            else
            {
                std::cout<<"what.";
            }
                
        }
        return ;
    }
    //TODO:注释
    void SeqASTVisitor::JudgeAndInsert(clang::Stmt* &stmt,const clang::BinaryOperator *bop,clang::Rewriter &_rewriter,int &count,clang::SourceManager& SM,std::string type){
        std::cout<<type<<std::endl;
        if(type=="DeclStmt"){
            clang::DeclStmt *phs = llvm::dyn_cast<clang::DeclStmt>(stmt);
            clang::Decl * decl=phs->getSingleDecl();

            if (clang::VarDecl *varDecl = llvm::dyn_cast<clang::VarDecl>(decl)) {
                clang::Expr * expr = varDecl->getInit();

                if(!(bop = llvm::dyn_cast<clang::BinaryOperator>(expr)));
                    GetSimplifiedExpr(expr,bop,expr->getStmtClassName());

                std::string* insertStr=new std::string;
                *insertStr="";

                judgeDivWithoutUBFuzz(bop,insertStr,count,SM,_rewriter);
            }
        }
        else if(type=="CallExpr"){
            clang::CallExpr* callexpr = llvm::dyn_cast<clang::CallExpr>(stmt);
            int numArgs = callexpr->getNumArgs();

            assert(numArgs>=0);

            for(int i = 0; i < numArgs; i++){
                clang::Expr* arg = callexpr->getArg(i);

                if(PosDivideZero(Tool::get_stmt_string(arg))){
                    if(!(bop = llvm::dyn_cast<clang::BinaryOperator>(arg)))
                        GetSimplifiedExpr(arg,bop,stmt->getStmtClassName());
                    
                    std::string* insertStr=new std::string;
                    *insertStr="";

                    judgeDivWithoutUBFuzz(bop,insertStr,count,SM,_rewriter);
                }
            }
        }
        else{
            std::cout<<"judge_insert:"<<Tool::get_stmt_string(stmt)<<std::endl;

            clang::Expr* expr = llvm::dyn_cast<clang::Expr>(stmt);

            if(!(bop = llvm::dyn_cast<clang::BinaryOperator>(stmt)))
                GetSimplifiedExpr(expr,bop,stmt->getStmtClassName());

            std::string* insertStr=new std::string;
            *insertStr="";
            
            judgeDivWithoutUBFuzz(bop,insertStr,count,SM,_rewriter);
            
        }


    }

    //TODO:注释
    void SeqASTVisitor::LoopChildren(clang::Stmt*& stmt,const clang::BinaryOperator *bop,clang::Rewriter &_rewriter, int &count, clang::SourceManager& SM, std::string type,std::string stmt_string)
    {
        if(type=="ForStmt"){
                                clang::ForStmt *FS = llvm::dyn_cast<clang::ForStmt>(stmt);
                                clang::Stmt *scond=FS->getCond();
                                for(auto &s_stmt : scond->children()){
                                    stmt_string=Tool::get_stmt_string(s_stmt);

                                    type = s_stmt->getStmtClassName();

                                    if(type == "ForStmt" || type == "IfStmt" || type == "CompoundStmt" )
                                        LoopChildren(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),stmt_string);

                                    else if(PosDivideZero(stmt_string)){
                                        JudgeAndInsert(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName());
                                    }
                                }

                                clang::Stmt *sbody=FS->getBody();
                                for (auto &s_stmt : sbody->children() ){
                                    stmt_string=Tool::get_stmt_string(s_stmt);

                                    type = s_stmt->getStmtClassName();

                                    if(type == "ForStmt" || type == "IfStmt" || type == "CompoundStmt" )
                                        LoopChildren(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),stmt_string);

                                    else if(PosDivideZero(stmt_string)){
                                        JudgeAndInsert(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName());
                                    }
                                }
                            }
        else if(type == "IfStmt"){
                            //查询条件
                                clang::IfStmt *IS = llvm::dyn_cast<clang::IfStmt>(stmt);
                            
                                clang::Stmt *ibody=IS->getCond();
                                for (auto &s_stmt : ibody->children() ){
                                    stmt_string=Tool::get_stmt_string(s_stmt);

                                    type = s_stmt->getStmtClassName();

                                    if(type == "ForStmt" || type == "IfStmt" || type == "CompoundStmt" )
                                        LoopChildren(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),stmt_string);

                                    else if(PosDivideZero(stmt_string)){
                                        JudgeAndInsert(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName());
                                    }
                                }

                                ibody=IS->getThen();
                                for (auto &s_stmt : ibody->children() ){
                                    stmt_string=Tool::get_stmt_string(s_stmt);

                                    type = s_stmt->getStmtClassName();

                                    if(type == "ForStmt" || type == "IfStmt" || type == "CompoundStmt" )
                                        LoopChildren(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),stmt_string);

                                    else if(PosDivideZero(stmt_string)){
                                        JudgeAndInsert(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName());
                                    }
                                }
                                
                                ibody = IS->getElse();
                                if(ibody){
                                for (auto &s_stmt : ibody->children() ){
                                    stmt_string=Tool::get_stmt_string(s_stmt);

                                    type = s_stmt->getStmtClassName();

                                    if(type == "ForStmt" || type == "IfStmt" || type == "CompoundStmt" )
                                        LoopChildren(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),stmt_string);

                                    else if(PosDivideZero(stmt_string)){
                                        JudgeAndInsert(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName());
                                    }
                                }
                                }
                            }
        else if(type == "CompoundStmt")
        {
            clang::CompoundStmt *CS = llvm::dyn_cast<clang::CompoundStmt>(stmt);
            for (auto &s_stmt : CS->body()) 
            {
                std::cout<<Tool::get_stmt_string(s_stmt)<<std::endl;

                stmt_string=Tool::get_stmt_string(s_stmt);

                                    type = s_stmt->getStmtClassName();

                                    if(type == "ForStmt" || type == "IfStmt" || type == "CompoundStmt" )
                                        LoopChildren(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),stmt_string);

                                    else if(PosDivideZero(stmt_string)){
                                        std::cout<<"MayInsert."<<std::endl;
                                        JudgeAndInsert(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName());
                                    }
            }
        }
    }

    //定位UBFUZZ位置并存入line和column
    void find_UB(int &l,int &c,const std::string& fname){
        std::ifstream file(fname);
        if(!file){
            std::cout<<"file not exists--in find_UB"<<std::endl;
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
    //TODO:注释
    void find_INTOP(std::pair<int,int> &intopl, std::pair<int,int> &intopr,const std::string& fname,int UB_line){
        std::ifstream file(fname);
        if(!file){
            std::cout<<"file not exists--in find_INTOP"<<std::endl;
            return ;
        }
        int temp_l=0;
        std::string cur;
        std::string def,INTOP_sign,MUT_VAR;

        std::string INTOP_L,INTOP_R;
        while(std::getline(file,cur)){
            temp_l++;

            if(temp_l<UB_line+1)
                continue;
            else if(temp_l==UB_line+1){
                std::istringstream iss(cur);
                iss>>def>>INTOP_sign>>MUT_VAR;
                INTOP_R=INTOP_L=INTOP_sign;

                // if(INTOP_sign[6]=='L')
                //     INTOP_R[6]='R';
                // else if(INTOP_sign[6]=='R')
                //     INTOP_L[6]='L';
                // else{
                //     std::cout<<"Error: Invalid INTOP form."<<std::endl;
                //     return;
                // }
                // continue;
            }
            else{
                // int l=cur.find(INTOP_L);
                // int r=cur.find(INTOP_R);
                // if( (l!=-1 && r==-1) || (l==-1 && r!=-1)){
                //     std::cout<<"Error: INTOP_L/R not in the same row."<<std::endl;
                //     return ;
                // }
                // if(l != -1 &&r != -1){
                //     intopr.first=intopl.first=temp_l;
                //     intopl.second=l;
                //     intopr.second=r;
                //     return;
                // }

                int c=cur.find(INTOP_sign+")");
                if(c!=-1){
                    intopl.first=temp_l;
                    intopl.second=c;
                    return;
                }
            }
        }
        return ;

    }
    
    //寻找是否带有stdlib.h头文件,如果没有则会自动补充在文件首部
    void find_stdlib(const std::string& fname,clang::Rewriter &_rewriter,bool &fir){
        std::ifstream file(fname);
        if(!file){
            std::cout<<"file not exists--in find_UB"<<std::endl;
            return ;
        }
        std::string cur;
        while(std::getline(file,cur)){
            if(cur.find("stdlib.h") != -1 && cur.find("#include") != -1){
                return ;
            }
        }

        clang::SourceLocation insertLoc = _rewriter.getSourceMgr().getLocForStartOfFile(_rewriter.getSourceMgr().getMainFileID());

        if(!fir)
        {
            _rewriter.InsertText(insertLoc,"#include <stdlib.h>\n");
            fir=true;
        }
        

        return;
    }
    
    int last_count=0;
    int count=0;   
    //WARNING: used in diffrent roles in mut and ubfuzz, may be unhealthy.
    static bool fir=false;//short for 'first' 

    bool SeqASTVisitor::VisitFunctionDecl(clang::FunctionDecl *func_decl){
        if (!_ctx->getSourceManager().isInMainFile(func_decl->getLocation()))
            return true;
        if (isInSystemHeader(func_decl->getLocation()))
            return true;
            
        //*****打印AST
        //func_decl->dump();
        //*****
        
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

        //--------UBFUZZ localized--------
        if(!getFlag(MUT_BIT)){
            find_UB(UBFUZZ_line,UBFUZZ_column,filePath);
            if(UBFUZZ_line==-1)
                std::cout<<"Cant find a UBFUZZ sign"<<std::endl;
            else
                std::cout<<"UBFUZZ in line"<<UBFUZZ_line<<std::endl;
        }
        //--------UBFUZZ localized end--------

        std::pair<int,int> intopl_loc=std::make_pair(-1,-1);
        std::pair<int,int> intopr_loc=std::make_pair(-1,-1);
        int bits=-1;

        //--------shift redirection--------        
        if(!getFlag(MAIN_BIT)){
            find_INTOP(intopl_loc,intopr_loc,filePath,UBFUZZ_line);
            UBFUZZ_line=intopl_loc.first;
        }
        std::cout<<"check:"<<intopl_loc.first<<":"<<intopl_loc.second<<" & "<<intopr_loc.second<<std::endl;
        //--------shift redirection end -------- 

        std::string func_name = func_decl->getNameAsString();
        std::cout<<"\n| Function [" << func_decl->getNameAsString() << "] defined in: " << filePath << "\n";
        
        if (func_decl == func_decl->getDefinition())
        {

            clang::Stmt *body_stmt = func_decl->getBody();
            //SaniCatcherPairs count
                        
            //遍历这个函数中的语句
            if(getFlag(MAIN_BIT) && getFlag(MUT_BIT))
            {
                const clang::BinaryOperator *bop = nullptr;
                //clang::BinaryOperator::Opcode op;
                //clang::Expr *lhs; clang::Expr *rhs;

                for (auto &stmt : body_stmt->children())
                {
                    auto ori_stmt = stmt;
                    clang::SourceLocation insert_loc = ori_stmt->getBeginLoc();

                    std::string stmt_string=Tool::get_stmt_string(stmt);
                    
                    if(PosDivideZero(stmt_string))
                    {
                        std::cout<<"stmt:"<<stmt_string<<std::endl;
                        std::string type = stmt->getStmtClassName();

                        
                        if(type == "ForStmt" || type == "IfStmt" || type == "CompoundStmt" )
                            LoopChildren(stmt,bop,_rewriter,count,SM,type,stmt_string);
                        else
                        {
                            JudgeAndInsert(stmt,bop,_rewriter,count,SM,stmt->getStmtClassName());     
                        }
                                // if(getFlag(OPT_BIT)){
                                //     std::cout<<"mode: print"<<std::endl;

                                //     clang::SourceLocation loc=ori_stmt->getBeginLoc();
                                //     judgePrint(bop,SM,UBFUZZ_column,loc);
                                            
                                //     *insertStr=" fprintf(stderr, \"ACT_CHECK_CODE\") &&";
                                //     if(loc!=ori_stmt->getBeginLoc())
                                //         _rewriter.InsertTextBefore(loc,*insertStr);
                                //     else
                                //         _rewriter.InsertTextBefore(ori_stmt->getBeginLoc(),"fprintf(stderr, \"ACT_CHECK_CODE\");\n");
                                // } ?
                    }
                    else continue;
                }

            }
            else
            {
                for (auto &stmt : body_stmt->children())
                {
                    //定位到含有/*UBFUZZ*/的那行Stmt
                    std::cout<<"checkline:"<<SM.getSpellingLineNumber(stmt->getEndLoc());
                    
                    if(SM.getSpellingLineNumber(stmt->getEndLoc()) >= UBFUZZ_line && !fir){
                        auto ori_stmt = stmt;
                        clang::SourceLocation insert_loc=ori_stmt->getBeginLoc();

                        fir=true;
                        
                        std::string type=stmt->getStmtClassName();
                        
                        GetSubExpr(stmt,ori_stmt,type,fir,SM,UBFUZZ_line);
                        
                        // 判断是不是二元运算
                        if(const clang::BinaryOperator *bop = llvm::dyn_cast<clang::BinaryOperator>(stmt)){
                            clang::BinaryOperator::Opcode op = bop->getOpcode();//运算符，如op==clang::BinaryOperator::Opcode::BO_Comma 可以判断运算符是不是逗号
                            clang::Expr *lhs = bop->getLHS(); clang::Expr *rhs = bop->getRHS();

                            std::string* insertStr=new std::string;
                            *insertStr="";
                                
                            if(getFlag(MAIN_BIT)){
                                //for '&& situation', print the check_code
                                if(getFlag(OPT_BIT)){
                                    std::cout<<"mode: print"<<std::endl;

                                    clang::SourceLocation loc=ori_stmt->getBeginLoc();
                                    judgePrint(bop,SM,UBFUZZ_column,loc);
                                        
                                    *insertStr=" fprintf(stderr, \"ACT_CHECK_CODE\") &&";
                                    if(loc!=ori_stmt->getBeginLoc())
                                        _rewriter.InsertTextBefore(loc,*insertStr);
                                    else
                                        _rewriter.InsertTextBefore(ori_stmt->getBeginLoc(),"fprintf(stderr, \"ACT_CHECK_CODE\");\n");
                                }
                                else{
                                    std::cout<<"mode: div"<<std::endl;

                                    std::string stmt_string=Tool::get_stmt_string(stmt);
                                    if(PosDivideZero(stmt_string))
                                        judgeDiv(bop,insertStr,count,SM,UBFUZZ_column,insert_loc);
                                    
                                    //insertStr->pop_back(); //delete the last character
                                    *insertStr += "/*A_QUITE_UNIQUE_FLAG*/\n";
                                    _rewriter.InsertTextBefore(insert_loc,*insertStr);
                                }   
                            }
                            else{
                                std::cout<<"mode: shift"<<std::endl;

                                std::string stmt_string=Tool::get_stmt_string(stmt);

                                std::cout<<stmt_string<<"-----"<<std::endl;

                                //UBFUZZ_column=stmt_string.find("+ (MUT_VAR)");

                                
                                judgeShf(bop,nullptr,insertStr,SM,bits);

                                _rewriter.InsertTextBefore(ori_stmt->getBeginLoc(),*insertStr);

                            }

                        }
                        //    if(lhs==nullptr || rhs==nullptr) return false;//防止空指针
                        //   std::string lhs_class = lhs->getStmtClassName(),rhs_class = rhs->getStmtClassName();//需要考虑运算符左右子语句可能也是二元运算。所以可能需要设计递归函数
                    }
                }
            }
            
            std::cout<<"count:"<<count<<std::endl;

            if(getFlag(MAIN_BIT))
                _rewriter.InsertTextBefore(func_decl->getBeginLoc(),initSani(last_count,count));
            else
                _rewriter.InsertTextBefore(func_decl->getBeginLoc(),initSani_shf(bits));
            
            last_count=count;

            if(getFlag(MAIN_BIT))
                find_stdlib(filePath,_rewriter,fir);
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

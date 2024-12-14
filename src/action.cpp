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

//TODO: getSubExpr -> function

    void generateArray(int n,std::string v_name,std::string *insertStr,const std::string mode){
        
        std::string num=std::to_string(n);
        //std::string a="SaniTestArr";
        if(mode=="div")
            *insertStr=("\n\tSaniCatcher0=SaniTestArr0["+v_name+"-1];\n");
        else if(mode=="shf")
            *insertStr=("\n\tSaniCatcher=SaniTestArr["+v_name+"];\n");
        else
            std::cout<<"Error: generateArray encounter invalid mode type."<<std::endl;
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
    std::string initSani_shf(int bits){
        if(bits==-1) return "";

        std::string num=std::to_string(bits);
        std::string Arr="int SaniTestArr["+num+"];\n";

        std::string Cat="int SaniCatcher;\n";

        return Arr+Cat;
    }

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
                judgePrint(bopl,SM,c,loc);
        }

        std::cout<<"check rhs:"<<Tool::get_stmt_string(rhs)<<std::endl;
        if((Tool::get_stmt_string(rhs).find('/')!=-1 || Tool::get_stmt_string(rhs).find('%')!=-1) && SM.getSpellingColumnNumber(rhs->getBeginLoc()) < c){
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

    void SeqASTVisitor::judgeDiv(const clang::BinaryOperator *bop,std::string* insertStr,int &count,clang::SourceManager& SM,const int c){
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
            generateArray(count,Tool::get_stmt_string(rhs),insertStr,"div");
        }
        
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

        //--------UBFUZZ localized--------
        int UBFUZZ_line=-1;
        int UBFUZZ_column=-1;
        static bool fir=false;

        find_UB(UBFUZZ_line,UBFUZZ_column,filePath);
        if(UBFUZZ_line==-1)
            std::cout<<"Cant find a UBFUZZ sign"<<std::endl;
        else
            std::cout<<"UBFUZZ in line"<<UBFUZZ_line<<std::endl;
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
            int count=0;
            for (auto &stmt : body_stmt->children())//遍历这个函数中的语句
            {
                if(SM.getSpellingLineNumber(stmt->getEndLoc()) >= UBFUZZ_line && !fir){//定位到含有/*UBFUZZ*/的那行Stmt
                    auto ori_stmt=stmt;
                    fir=true;
                    // std::cout<<Tool::get_stmt_string(stmt)<<std::endl;
                     std::string type=stmt->getStmtClassName();
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

                    if(const clang::BinaryOperator *bop = llvm::dyn_cast<clang::BinaryOperator>(stmt)){// 这样是判断是不是二元运算
                        clang::BinaryOperator::Opcode op = bop->getOpcode();//运算符，如op==clang::BinaryOperator::Opcode::BO_Comma 可以判断运算符是不是逗号
                        clang::Expr *lhs = bop->getLHS(); clang::Expr *rhs = bop->getRHS();

                        std::string* insertStr=new std::string;
                        *insertStr="";
                            
                        if(getFlag(MAIN_BIT)){
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
                                if(stmt_string.find('/')!=-1 || stmt_string.find('%')!=-1)
                                    judgeDiv(bop,insertStr,count,SM,UBFUZZ_column);
                                
                                insertStr->pop_back();
                                *insertStr += "/*A_QUITE_UNIQUE_FLAG*/\n";
                                _rewriter.InsertTextBefore(ori_stmt->getBeginLoc(),*insertStr);
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
            if(getFlag(MAIN_BIT))
                _rewriter.InsertTextBefore(func_decl->getBeginLoc(),initSani(count));
            else
                _rewriter.InsertTextBefore(func_decl->getBeginLoc(),initSani_shf(bits));
                
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
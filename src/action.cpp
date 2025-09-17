//优化TODO
//!JudgeByMode是重复代码的简化函数，但是替换未完全
//1.可以用Expr*->IgnoreParenCasts();完成Expr类型的外部括号、转换的去除
//2.可以用Rewriter::ReplaceText进行某些语句的替换

//P.S.
//所有mode[UBFUZZ]代码为第一阶段测试时使用的代码，针对特殊构建的样例进行测试，在对真实项目进行检测时不调用（可忽略）

#include "action.h"
namespace sseq
{
    // -----------null模式全局变量-----------
    std::unordered_set<std::string> w_NameList;
    std::unordered_set<std::string> global_w_NameList={
        "stderr","stdout","stdin","va_start","va_arg","va_end"
    };
    std::string global_current_func_name ="";
    //global flags
    bool FOR_COND = false;
    bool DO_WHILE_MACRO = false;
    bool OUTLOOP = false;
    // -----------null模式全局变量-----------
    
    int last_count=0;
    int count=0;  

    //WARNING: 在[mut]模式和[UBFUZZ]模式中承担了不同的职责，不健康，但无大碍
    static bool fir = false;

    #pragma region 返回值bool判定函数

    //true:str含/或%
    bool PosDivideZero(std::string str){
        return str.find('/')!=-1 || str.find('%')!=-1;
    }
    //true:str含>>或<<（囊括<<=和>>=）
    bool PosShfOverflow(std::string str){
        return str.find("<<")!=-1 || str.find(">>")!=-1;
    }
    //true:str含有非字母数字下划线字符
    bool ContainSpecialChar(std::string str){
        for(char ch : str){
            if(!(ch == '_' || (ch <= '9' && ch >= '0')||(ch <= 'z' && ch >= 'a') || (ch <= 'Z' && ch >= 'A'))){
               ;//std::cout<<"[check special char]:"<<ch<<std::endl;
                return true;
            }
        }
        return false;
    }

    //true:是需要调用'遍历子语句(LoopChildren)'的Stmt类型
    bool NeedLoopChildren(std::string type){
        if(type == "ForStmt" || type == "IfStmt" || type == "CompoundStmt" ||
         type == "WhileStmt" || type == "SwitchStmt" || type == "LabelStmt" || 
         type == "DoStmt" )
            return true;
        else 
            return false;
    }
    //true:是需要对其子语句'JudgeAndPtrTrack'的Stmt类型
    bool NeedLoopPtrTrack(std::string type){
        if(type == "BinaryOperator" || type == "CallExpr" || type == "UnaryOperator" ||
             type == "ConditionalOperator")
            return true;
        else 
            return false;
    }
    //true:此bop是动态分配内存赋值类型
    bool isMallocAssignment(clang::BinaryOperator* bop){
        if (!bop->isAssignmentOp()) return false;

        clang::Expr* rhs = bop->getRHS()->IgnoreParenCasts();

        if (auto* callExpr = llvm::dyn_cast<clang::CallExpr>(rhs)) {
            ;//std::cout<<"[this assign is call_expr."<<std::endl;
            if (auto* callee = callExpr->getCallee()) {
                callee = callee->IgnoreImpCasts();

                if (auto* declRef = llvm::dyn_cast<clang::DeclRefExpr>(callee)){
                    std::string name = declRef->getNameInfo().getAsString();
                    ;//std::cout<<"[this assign is:"<<name<<std::endl;
                    return ((name=="ALLOCA") || (name == "alloc") || (name == "_alloc") || (name == "malloc"));
                }
                
            }
        }

        return false;
    }
    //true: char, short, int, long, bool | enum
    bool isIntegerVar(const clang::ValueDecl *VD){
        //std::cout<<"[isIntegerVar]:"<<VD->getNameAsString()<<std::endl;
        //return true;

        clang::QualType QT = VD->getType();
        const clang::Type *Ty = QT.getTypePtrOrNull();
        if (!Ty) return false;


        Ty = Ty->getUnqualifiedDesugaredType();

        if (const clang::BuiltinType *BT = dyn_cast<clang::BuiltinType>(Ty)) {
            return BT->isInteger(); 
        }

        if (Ty->isEnumeralType())
            return true;

        return false;
    }

    //true:变量名字不在白名单(w_NameList)中
    bool NotInWNameList(std::string name){
        if(global_w_NameList.find(name) == global_w_NameList.end()
        && w_NameList.find(name) == w_NameList.end())
            return true;
        return false;
    }

    //true:不是全局变量且不是enum
    bool NotGlobalNorEnum(clang::ValueDecl* vdecl){
        if(const clang::VarDecl* varDecl = llvm::dyn_cast<clang::VarDecl>(vdecl)){
            if(varDecl->hasGlobalStorage()) return false;
        }

        clang::QualType QT = vdecl->getType();
        ;//std::cout<<"[checkEnum]:"<<QT.getAsString()<<std::endl;
        if(QT->isEnumeralType()) return false;

        if(isa<clang::EnumConstantDecl>(vdecl)) return false;

        return true;
    }

    bool SeqASTVisitor::isInSystemHeader(const clang::SourceLocation &loc) {
        auto &SM = _ctx->getSourceManager();
        if(SM.getFilename(loc)=="") return true;
        std::string filename = SM.getFilename(loc).str();
        if(filename.find("/usr/include/")!=std::string::npos) return true;
        if(filename.find("llvm-12.0.0.obj/")!=std::string::npos) return true; //结合实际情况 当前工具开发使用llvm12
        return false;
    }

    #pragma endregion

    #pragma region 工具函数

    //通过( ->_),(*->_S)将str转变为合法变量名
    //warning:如果原变量名很极端仍可能出现不合法变量名
    std::string strTrans(const std::string str) {
        std::string result;
        for (char ch : str) {
            if (ch == ' ') {
                result += '_';
            } else if (ch == '*') {
                result += "_S";
            } else if ((ch >= '0' && ch <= '9') ||
                        (ch >= 'a' && ch <= 'z') || 
                        (ch >= 'A' && ch <= 'Z')) {
                result += ch;
            }
            else{
                result += '_';
            }
        }
        return result;
    }

    //简化file_name中的路径和后缀 ，获取纯净文件名
    void SimplifiedFileName(std::string& file_name){
        
        size_t path = file_name.find('/');
        if(path!=-1)
            path = file_name.find_last_of('/');
        file_name = file_name.substr(path+1);
        size_t pos = file_name.find('.');
        if(pos!=-1)
            file_name = file_name.substr(0,pos);

        file_name = strTrans(file_name);

        return ;
    }

    //UBFUZZ模式下定位处于if或for内部的目标stmt
    void GetSubExpr(clang::Stmt *&stmt,clang::Stmt *&ori_stmt, std::string type,bool &fir,clang::SourceManager& SM,int &UBFUZZ_line){
        while(type == "ForStmt" || type == "IfStmt"){
            fir=false;
            if(type=="ForStmt"){
                clang::ForStmt *FS = llvm::dyn_cast<clang::ForStmt>(stmt);
                clang::Stmt *sbody=FS->getBody();
                ;//std::cout<<sbody->getStmtClassName()<<std::endl;

                for (auto &s_stmt : sbody->children() ){
                    ;//std::cout<<Tool::get_stmt_string(s_stmt);
                    ;//std::cout<<"STMT:"<<SM.getSpellingLineNumber(s_stmt->getEndLoc())<<std::endl;
                    if(SM.getSpellingLineNumber(s_stmt->getEndLoc()) >= UBFUZZ_line){
                        fir=true;
                        stmt=s_stmt;
                        ori_stmt=stmt;
                        type=stmt->getStmtClassName();
                        ;//std::cout<<"type:"<<type<<std::endl;
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
                ;//std::cout<<"STMT:"<<SM.getSpellingLineNumber(s_stmt->getEndLoc())<<std::endl;
                    if(SM.getSpellingLineNumber(s_stmt->getEndLoc()) >= UBFUZZ_line){
                        fir=true;
                        stmt=s_stmt;
                        type=stmt->getStmtClassName();
                        ;//std::cout<<"type:"<<type<<std::endl;
                        found=true;
                        break;
                    }
                }
                //查询then分支
                if(!found){
                ibody=IS->getThen();
                for (auto &s_stmt : ibody->children() ){
                    ;//std::cout<<"STMT:"<<SM.getSpellingLineNumber(s_stmt->getEndLoc())<<std::endl;
                    if(SM.getSpellingLineNumber(s_stmt->getEndLoc()) >= UBFUZZ_line){
                        fir=true;
                        stmt=s_stmt;
                        ori_stmt=stmt;
                        type=stmt->getStmtClassName();
                        ;//std::cout<<"type:"<<type<<std::endl;
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
                    ;//std::cout<<Tool::get_stmt_string(s_stmt);
                    ;//std::cout<<"STMT:"<<SM.getSpellingLineNumber(s_stmt->getEndLoc())<<std::endl;
                    if(SM.getSpellingLineNumber(s_stmt->getEndLoc()) >= UBFUZZ_line){
                        fir=true;
                        stmt=s_stmt;
                        ori_stmt=stmt;
                        type=stmt->getStmtClassName();
                        ;//std::cout<<"type:"<<type<<std::endl;
                        break;
                    }
                }
                }
                //查询结束
                }
            ;//std::cout<<Tool::get_stmt_string(stmt)<<std::endl;
        }
    }

    //对Stmt*类型忽略括号和隐式转换
    clang::Stmt* IgnoreParenCastsStmt(clang::Stmt *S) {
        if(S == nullptr){
           //std::cout<<"[IgnoreParenCastsStmt]Encounter nullptr."<<std::endl;
            return nullptr;
        }
        if (auto *E = llvm::dyn_cast_or_null<clang::Expr>(S)){
            return (clang::Stmt*)(E->IgnoreParenCasts());
        }
            
        return S;
    }

    //穷举所有需要拆括号（提取SubExpr）的情况，并用SubExpr替换Expr
    void GetSimplifiedExpr(clang::Expr *&hs,const clang::BinaryOperator *&bop,std::string type){
        hs=hs->IgnoreParenImpCasts();
        bop=llvm::dyn_cast<clang::BinaryOperator>(hs);
        type = hs->getStmtClassName();

        if(type == "ArraySubscriptExpr"){
            clang::ArraySubscriptExpr *phs = llvm::dyn_cast<clang::ArraySubscriptExpr>(hs);
            hs=phs->getIdx()->IgnoreParenImpCasts();
            bop=llvm::dyn_cast<clang::BinaryOperator>(hs);

            type = hs->getStmtClassName();
        }
        
        return ;
    }

    #pragma endregion
    
    #pragma region 获取clang::SourceRange

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

    #pragma endregion
    
    
    #pragma region 插入字符串生成

    //mode[null]: 生成 _copy_ = &_Catcher; 的赋值语句
    void generateNullPtrAssign(std::string v_name,int cur_count, std::string typestr, std::string *insertStr, clang::SourceManager& SM, bool isGlobal){
        std::string file_name = SM.getFileEntryForID(SM.getMainFileID())->getName().str();
        SimplifiedFileName(file_name);

        typestr = strTrans(typestr);
        *insertStr=""+file_name+"_";

        if(!isGlobal)
            *insertStr+=global_current_func_name+"_";

        *insertStr += v_name+"_copy_"+typestr+" = &"+file_name+"_"+global_current_func_name+"_Catcher_"+std::to_string(cur_count)+";\n";

        return ;
    }
    //mode[null]: 生成 Catcher = *_copy_ 的检测语句
    void generateNullPtr(std::string v_name,int cur_count, std::string typestr, std::string *insertStr, clang::SourceManager& SM, bool isGlobal){
        std::string file_name = SM.getFileEntryForID(SM.getMainFileID())->getName().str();
        SimplifiedFileName(file_name);

        typestr = strTrans(typestr);
        *insertStr=""+file_name+"_"+global_current_func_name+"_Catcher_"+std::to_string(count)+"= *"+file_name+"_";
        
        if(!isGlobal)
            *insertStr+=global_current_func_name+"_";

        *insertStr += v_name+"_copy_"+typestr+";\n";

        return ;
    }
    //mode[div/shf]:生成第n对SaniCatcher和SaniTestArr,存入insertStr，以mode调控
    void generateArray(int n, std::string v_name, std::string *insertStr, const std::string mode, clang::SourceManager& SM){
        std::string num=std::to_string(n);

        std::string file_name = SM.getFileEntryForID(SM.getMainFileID())->getName().str();
        SimplifiedFileName(file_name);

        if(mode=="div")
            if(FOR_COND)
                *insertStr=("(("+file_name+"_div_SaniCatcher"+num+" = "+file_name+"_div_SaniTestArr"+num+"[("+v_name+" != 0) - 1]),");
            else
                *insertStr=("("+file_name+"_div_SaniCatcher"+num+" = "+file_name+"_div_SaniTestArr"+num+"[("+v_name+" != 0) - 1]),");
        else if(mode=="shf")
            if(DO_WHILE_MACRO)
                *insertStr=("("+file_name+"_shf_SaniCatcher"+num+" = "+file_name+"_shf_SaniTestArr"+num+"["+v_name+"]);");
            else
                *insertStr=("("+file_name+"_shf_SaniCatcher"+num+" = "+file_name+"_shf_SaniTestArr"+num+"["+v_name+"]),");
        else
            ;//std::cout<<"Error: generateArray encounter invalid mode type."<<std::endl;
        return ;
    }
    //mode[null]: 生成 _copy_ = NULL; 的定义语句(需要stdddef.h库)
    void initNullPtr(std::string v_name,std::string typestr, std::string *insertStr, clang::SourceManager& SM, bool isGlobal){
        std::string file_name = SM.getFileEntryForID(SM.getMainFileID())->getName().str();
        SimplifiedFileName(file_name);

        std::string norm_typestr=strTrans(typestr);
        *insertStr="int* "+file_name+"_";
        
        if(!isGlobal)
            *insertStr+=global_current_func_name+"_";

        *insertStr += v_name+"_copy_"+norm_typestr+"=NULL;\n";

        return ;
    }
    //mode[null]: 生成 int _Catcher_n= n; 的定义语句
    void initNullPtrCatcher(std::string v_name,int cur_count, std::string *insertStr, clang::SourceManager& SM){
        std::string file_name = SM.getFileEntryForID(SM.getMainFileID())->getName().str();
        SimplifiedFileName(file_name);

        *insertStr = "int "+file_name+"_"+global_current_func_name+"_Catcher_"+std::to_string(cur_count)+"="+std::to_string(cur_count)+";\n";

        return ;
    }
    
    //mode[div]: 生成第lst_n~n对SaniCatcher和SaniTestArr的定义字符串
    std::string initSani(int lst_n,int n,clang::SourceManager& SM){

        ;//std::cout<<"Sanicount:"<<n<<std::endl;

        if(lst_n==n) return "";

        std::string file_name = SM.getFileEntryForID(SM.getMainFileID())->getName().str();
        SimplifiedFileName(file_name);

        std::string Arr="int "+file_name+"_div_SaniTestArr"+std::to_string(lst_n)+"[10]";
        for(int i=lst_n+1;i<n;i++){
            Arr+=","+file_name+"_div_SaniTestArr"+std::to_string(i)+"[10]";
        }
        Arr+=";\n";

        std::string Cat="int "+file_name+"_div_SaniCatcher"+std::to_string(lst_n);
        for(int i=lst_n+1;i<n;i++){
            Cat+=","+file_name+"_div_SaniCatcher"+std::to_string(i);
        }
        Cat+=";\n";

        return Arr+Cat;
    }
    //mode[shf]: 生成第cur_count对SaniCatcher和SaniTestArr的定义字符串,位移bits位
    std::string initSani_shf(int cur_count, int bits,clang::SourceManager& SM){
        if(bits==-1) return "";

        std::string file_name = SM.getFileEntryForID(SM.getMainFileID())->getName().str();
        SimplifiedFileName(file_name);

        std::string num=std::to_string(bits);
        std::string Arr="int "+file_name+"_shf_SaniTestArr"+std::to_string(cur_count)+"["+num+"];\n";

        std::string Cat="int "+file_name+"_shf_SaniCatcher"+std::to_string(cur_count)+";\n";

        return Arr+Cat;
    }

    #pragma endregion

    #pragma region Judge遍历

    //UBFUZZ-Shf模式下遍历找到UBFUZZ的位置并插桩
    void SeqASTVisitor::judgeShf(const clang::BinaryOperator *bop,const clang::BinaryOperator* last_bop,std::string* insertStr,clang::SourceManager& SM,int& bits){
        if(bop == nullptr){
            ;//std::cout<<"warnning:encounter nullptr,maybe not a binaryoperator\n";
            return ;
        }
        clang::BinaryOperator::Opcode op = bop->getOpcode();
        clang::Expr *lhs=bop->getLHS();clang::Expr *rhs=bop->getRHS();

        if(Tool::get_stmt_string(lhs).find("+ (MUT_VAR)")!=-1){
            ;//std::cout<<"check lhs:"<<Tool::get_stmt_string(lhs)<<std::endl;
            
            const clang::BinaryOperator *bopl=llvm::dyn_cast<clang::BinaryOperator>(lhs);
            while(!bopl)
            {
                std::string type=lhs->getStmtClassName();
                ;//std::cout<<"lhs type:"<<type<<std::endl;
                if(type=="ParenExpr"){
                    clang::ParenExpr *plhs = llvm::dyn_cast<clang::ParenExpr>(lhs);
                    lhs=plhs->getSubExpr();
                }
                else if(type=="ImplicitCastExpr"){
                    clang::ImplicitCastExpr *plhs = llvm::dyn_cast<clang::ImplicitCastExpr>(lhs);
                    lhs=plhs->getSubExpr();

                    ;//std::cout<<Tool::get_stmt_string(lhs)<<std::endl;
                    ;//std::cout<<"Class:"<<lhs->getStmtClassName()<<std::endl;
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
                    ;//std::cout<<"Debug: Unexpected CallExpr."<<std::endl;
                }
                bopl=llvm::dyn_cast<clang::BinaryOperator>(lhs);
            }
            ;//std::cout<<"SUCC:"<<Tool::get_stmt_string(lhs)<<std::endl;
                judgeShf(bopl,bop,insertStr,SM,bits);
        }
        else if(Tool::get_stmt_string(rhs).find("+ (MUT_VAR)")!=-1){
            ;//std::cout<<"check rhs:"<<Tool::get_stmt_string(rhs)<<std::endl;

            const clang::BinaryOperator *bopr=llvm::dyn_cast<clang::BinaryOperator>(rhs);
            while(!bopr)
            {
                std::string type=rhs->getStmtClassName();
                ;//std::cout<<"rhs type:"<<type<<std::endl;
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
                            ;//std::cout<<"Debug: Unexpected CallExpr."<<std::endl;
                        }
                }


                bopr=llvm::dyn_cast<clang::BinaryOperator>(rhs);
            }
            ;//std::cout<<"SUCC:"<<Tool::get_stmt_string(rhs)<<std::endl;
                judgeShf(bopr,bop,insertStr,SM,bits);
        }
        else{
            ;//std::cout<<"then rhs==mutvar:"<<Tool::get_stmt_string(rhs)<<std::endl;
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
                ;//std::cout<<"type:"<<type<<std::endl;
                bits=-1;
                ;//std::cout<<"Debug: Unexpected situation when generate shf_Array."<<std::endl;
            }

            ;//std::cout<<"Fin_Type:"<<fin_lhs->getType().getAsString()<<std::endl;

            generateArray(1,Tool::get_stmt_string(fin_rhs),insertStr,"shf",SM);

        }
        
        return ;
    }
    //UBFUZZ-Div-Print模式下遍历找到UBFUZZ的位置，插桩并插入CHECK_CODE
    void SeqASTVisitor::judgePrint(const clang::BinaryOperator *bop,clang::SourceManager& SM,const int c,clang::SourceLocation &loc){
        if(bop == nullptr){
            ;//std::cout<<"warnning:encounter nullptr,maybe not a binaryoperator\n";
            return ;
        }

        clang::BinaryOperator::Opcode op = bop->getOpcode();
        clang::Expr *lhs=bop->getLHS();clang::Expr *rhs=bop->getRHS();


        llvm::StringRef opcodeStr = clang::BinaryOperator::getOpcodeStr(op);
        std::cout << "Opcode: " << opcodeStr.str() << std::endl;
        //printf("%d vs %d\n",SM.getSpellingColumnNumber(rhs->getBeginLoc()),c);

        

        if(op==clang::BinaryOperator::Opcode::BO_LAnd && SM.getSpellingColumnNumber(rhs->getBeginLoc())<c && SM.getSpellingColumnNumber(rhs->getEndLoc())>=c){
            loc=SM.getExpansionLoc(rhs->getBeginLoc());
            //printf("update loc: %d\n",SM.getSpellingColumnNumber(loc));
        }

        ;//std::cout<<"check lhs:"<<Tool::get_stmt_string(lhs)<<std::endl;
        if((PosDivideZero(Tool::get_stmt_string(lhs))) && SM.getSpellingColumnNumber(lhs->getEndLoc()) >= c){
            ;//std::cout<<"check lhs:"<<Tool::get_stmt_string(lhs)<<std::endl;
            
            const clang::BinaryOperator *bopl=llvm::dyn_cast<clang::BinaryOperator>(lhs);
            while(!bopl)
            {
                std::string type=lhs->getStmtClassName();
                ;//std::cout<<"lhs type:"<<type<<std::endl;
                if(type=="ParenExpr"){
                    clang::ParenExpr *plhs = llvm::dyn_cast<clang::ParenExpr>(lhs);
                    lhs=plhs->getSubExpr();
                }
                else if(type=="ImplicitCastExpr"){
                    clang::ImplicitCastExpr *plhs = llvm::dyn_cast<clang::ImplicitCastExpr>(lhs);
                    lhs=plhs->getSubExpr();

                    ;//std::cout<<Tool::get_stmt_string(lhs)<<std::endl;
                    ;//std::cout<<"Class:"<<lhs->getStmtClassName()<<std::endl;
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
                                ;//std::cout<<Tool::get_stmt_string(lhs)<<std::endl;
                                break;
                           }
                        }
                }
                bopl=llvm::dyn_cast<clang::BinaryOperator>(lhs);
            }
            ;//std::cout<<"SUCC:"<<Tool::get_stmt_string(lhs)<<std::endl;
                judgePrint(bopl,SM,c,loc);
        }

        ;//std::cout<<"check rhs:"<<Tool::get_stmt_string(rhs)<<std::endl;
        if((PosDivideZero(Tool::get_stmt_string(rhs))) && SM.getSpellingColumnNumber(rhs->getBeginLoc()) < c){
            ;//std::cout<<"check rhs:"<<Tool::get_stmt_string(rhs)<<std::endl;

            const clang::BinaryOperator *bopr=llvm::dyn_cast<clang::BinaryOperator>(rhs);
            while(!bopr)
            {
                std::string type=rhs->getStmtClassName();
                ;//std::cout<<"rhs type:"<<type<<std::endl;
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
                                ;//std::cout<<Tool::get_stmt_string(rhs)<<std::endl;
                                break;
                           }
                        }
                }

                bopr=llvm::dyn_cast<clang::BinaryOperator>(rhs);
            }
            ;//std::cout<<"SUCC:"<<Tool::get_stmt_string(rhs)<<std::endl;
                judgePrint(bopr,SM,c,loc);
            
        }
        return ;

    }
    //UBFUZZ-Div模式下遍历找到UBFUZZ的位置并插桩
    void SeqASTVisitor::judgeDiv(const clang::BinaryOperator *bop,std::string* insertStr,int &count,clang::SourceManager& SM,const int c,clang::SourceLocation &loc){
        if(bop == nullptr){
            ;//std::cout<<"warnning:encounter nullptr,maybe not a binaryoperator\n";
            return ;
        }

        clang::BinaryOperator::Opcode op = bop->getOpcode();
        clang::Expr *lhs=bop->getLHS();clang::Expr *rhs=bop->getRHS();
        
        if((op == clang::BinaryOperator::Opcode::BO_Div || op == clang::BinaryOperator::Opcode::BO_Rem) && 
        (SM.getSpellingColumnNumber(rhs->getEndLoc())>c && SM.getSpellingColumnNumber(rhs->getBeginLoc())<c ))
        {
            ;//std::cout<<"check"<<SM.getSpellingColumnNumber(rhs->getBeginLoc())<<std::endl;
            ;//std::cout<<"check"<<SM.getSpellingColumnNumber(rhs->getEndLoc())<<std::endl;
            count=1;
            generateArray(0,Tool::get_stmt_string(rhs),insertStr,"div",SM);

            loc=SM.getExpansionLoc(lhs->getBeginLoc());
        }
        
        ;//std::cout<<"check lhs:"<<Tool::get_stmt_string(lhs)<<std::endl;
        if((PosDivideZero(Tool::get_stmt_string(lhs))) && SM.getSpellingColumnNumber(lhs->getEndLoc()) >= c){
            ;//std::cout<<"check lhs:"<<Tool::get_stmt_string(lhs)<<std::endl;
            
            const clang::BinaryOperator *bopl=llvm::dyn_cast<clang::BinaryOperator>(lhs);
            while(!bopl)
            {
                std::string type=lhs->getStmtClassName();
                ;//std::cout<<"lhs type:"<<type<<std::endl;

                if(type=="ParenExpr"){
                    clang::ParenExpr *plhs = llvm::dyn_cast<clang::ParenExpr>(lhs);
                    lhs=plhs->getSubExpr();
                }
                else if(type=="ImplicitCastExpr"){
                    clang::ImplicitCastExpr *plhs = llvm::dyn_cast<clang::ImplicitCastExpr>(lhs);
                    lhs=plhs->getSubExpr();

                    ;//std::cout<<Tool::get_stmt_string(lhs)<<std::endl;
                    ;//std::cout<<"Class:"<<lhs->getStmtClassName()<<std::endl;
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
                                ;//std::cout<<Tool::get_stmt_string(lhs)<<std::endl;
                                break;
                           }
                        }
                }


                bopl=llvm::dyn_cast<clang::BinaryOperator>(lhs);
            }
            ;//std::cout<<"SUCC:"<<Tool::get_stmt_string(lhs)<<std::endl;
                judgeDiv(bopl,insertStr,count,SM,c,loc);
        }

        ;//std::cout<<"check rhs:"<<Tool::get_stmt_string(rhs)<<std::endl;
        if((PosDivideZero(Tool::get_stmt_string(rhs))) && SM.getSpellingColumnNumber(rhs->getBeginLoc()) <= c){
            ;//std::cout<<"check rhs:"<<Tool::get_stmt_string(rhs)<<std::endl;

            const clang::BinaryOperator *bopr=llvm::dyn_cast<clang::BinaryOperator>(rhs);
            while(!bopr)
            {
                std::string type=rhs->getStmtClassName();
                ;//std::cout<<"rhs type:"<<type<<std::endl;

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
                                ;//std::cout<<Tool::get_stmt_string(rhs)<<std::endl;
                                break;
                           }
                        }
                }


                bopr=llvm::dyn_cast<clang::BinaryOperator>(rhs);
            }
            ;//std::cout<<"SUCC:"<<Tool::get_stmt_string(rhs)<<std::endl;
                judgeDiv(bopr,insertStr,count,SM,c,loc);
            
        }
        return ;
    }
    //mut插入模式下（无UBFUZZ），对bop下的AST中的所有可能的/与%进行查询和插入
    void SeqASTVisitor::judgeDivWithoutUBFuzz(const clang::BinaryOperator *bop,std::string* insertStr,int &count,clang::SourceManager& SM,clang::Rewriter &_rewriter){
        if(bop == nullptr){ return ;}

        clang::BinaryOperator::Opcode op = bop->getOpcode();
        clang::Expr *lhs=bop->getLHS();clang::Expr *rhs=bop->getRHS();

        if(op == clang::BinaryOperator::Opcode::BO_Div || op == clang::BinaryOperator::Opcode::BO_Rem){

            auto SR = get_sourcerange(IgnoreParenCastsStmt(rhs));

            if(clang::IntegerLiteral* in = llvm::dyn_cast<clang::IntegerLiteral>(rhs)){
                if(in->getValue().getZExtValue() == 0){
                    generateArray(count++,Tool::get_stmt_string(rhs),insertStr,"div",SM);
                    if(FOR_COND){
                        _rewriter.InsertText(SM.getSpellingLoc(SR.getBegin()),"(",true,true);
                        //_rewriter.InsertTextBefore(SM.getExpansionLoc(lhs->getBeginLoc()),"(");
                    }
                        
                    _rewriter.InsertTextBefore(SM.getExpansionLoc(lhs->getBeginLoc()),"("+*insertStr);

                    const clang::LangOptions& LO = _ctx->getLangOpts();
                    auto afterEnd = clang::Lexer::getLocForEndOfToken(SR.getEnd(), 0, SM, LO);

                    if(FOR_COND)
                        _rewriter.InsertText(afterEnd,"))",true,true); 
                    else 
                        _rewriter.InsertText(afterEnd,")",true,true); 
                       
                }
            }
            else{
                generateArray(count++,Tool::get_stmt_string(rhs),insertStr,"div",SM);

                // if(FOR_COND){
                //     _rewriter.InsertText(SM.getSpellingLoc(SR.getBegin()),"(",true,true);
                // }

                _rewriter.InsertTextBefore(SM.getExpansionLoc(lhs->getBeginLoc()),"("+*insertStr);

                const clang::LangOptions& LO = _ctx->getLangOpts();
                auto afterEnd = clang::Lexer::getLocForEndOfToken(SR.getEnd(), 0, SM, LO);

                if(FOR_COND)
                    _rewriter.InsertText(afterEnd,"))",true,true); 
                else
                    _rewriter.InsertText(afterEnd,")",true,true); 
                
            }
            
        }

        if(PosDivideZero(Tool::get_stmt_string(lhs))){
            const clang::BinaryOperator *bopl=llvm::dyn_cast<clang::BinaryOperator>(lhs);
            std::string type;

            GetSimplifiedExpr(lhs,bopl,type);
            ;//std::cout<<"SUCC:"<<Tool::get_stmt_string(lhs)<<std::endl;
            if(bopl){
                std::string* ninsertStr=new std::string;
                *ninsertStr="";
                judgeDivWithoutUBFuzz(bopl,ninsertStr,count,SM,_rewriter);

                delete ninsertStr;
            } 
        }

        ;//std::cout<<"check rhs:"<<Tool::get_stmt_string(rhs)<<std::endl;
        if(PosDivideZero(Tool::get_stmt_string(rhs))){
            const clang::BinaryOperator *bopr=llvm::dyn_cast<clang::BinaryOperator>(rhs);
            std::string type;

            GetSimplifiedExpr(rhs,bopr,type);
            ;//std::cout<<"SUCC:"<<Tool::get_stmt_string(rhs)<<std::endl;
            if(bopr){
                std::string* ninsertStr=new std::string;
                *ninsertStr="";
                judgeDivWithoutUBFuzz(bopr,ninsertStr,count,SM,_rewriter);

                delete ninsertStr;
            }
            else
            {
                ;//std::cout<<"what.";
            }
                
        }
        return ;
    }
    //mut插入模式下（无UBFUZZ），对某CallExpr bop下的AST中的所有可能的/与%进行查询和插入,位置固定在在CallExprHead防止误判为参数
    void SeqASTVisitor::judgeDivWithoutUBFuzz(const clang::BinaryOperator *bop,std::string* insertStr,int &count,clang::SourceManager& SM,clang::Rewriter &_rewriter,clang::SourceLocation& CallExprHead){
        ;//std::cout<<"judge_insert_CallExpr:"<<std::endl;
        if(bop == nullptr){
            ;//std::cout<<"warnning:encounter nullptr,maybe not a binaryoperator\n";
            return ;
        }
        
        clang::BinaryOperator::Opcode op = bop->getOpcode();
        clang::Expr *lhs=bop->getLHS();clang::Expr *rhs=bop->getRHS();

        if(op == clang::BinaryOperator::Opcode::BO_Div || op == clang::BinaryOperator::Opcode::BO_Rem)
        {
            if(clang::IntegerLiteral* in = llvm::dyn_cast<clang::IntegerLiteral>(rhs)){
                if(in->getValue().getZExtValue() == 0){
                    generateArray(count++,Tool::get_stmt_string(rhs),insertStr,"div",SM);

                    _rewriter.InsertTextBefore(CallExprHead,*insertStr);
                }
            }
            else{
                
                if(!rhs->getBeginLoc().isMacroID()){

                    generateArray(count++,Tool::get_stmt_string(rhs),insertStr,"div",SM);

                    ;//std::cout<<"count++"<<std::endl;

                    _rewriter.InsertTextBefore(CallExprHead,*insertStr);

                }
            }
        }
        else
            ;//std::cout<<"\topcode not match\n";

        ;//std::cout<<"check lhs:"<<Tool::get_stmt_string(lhs)<<std::endl;
        if(PosDivideZero(Tool::get_stmt_string(lhs))){
            const clang::BinaryOperator *bopl=llvm::dyn_cast<clang::BinaryOperator>(lhs);//TODO: IgnoreParenCAsts
            std::string type;

            GetSimplifiedExpr(lhs,bopl,type);
            ;//std::cout<<"SUCC:"<<Tool::get_stmt_string(lhs)<<std::endl;
            if(bopl){
                std::string* ninsertStr=new std::string;
                *ninsertStr="";
                judgeDivWithoutUBFuzz(bopl,ninsertStr,count,SM,_rewriter,CallExprHead);

                delete ninsertStr;
            }
                
        }

        ;//std::cout<<"check rhs:"<<Tool::get_stmt_string(rhs)<<std::endl;
        if(PosDivideZero(Tool::get_stmt_string(rhs))){
            const clang::BinaryOperator *bopr=llvm::dyn_cast<clang::BinaryOperator>(rhs);
            std::string type;

            GetSimplifiedExpr(rhs,bopr,type);
            ;//std::cout<<"SUCC:"<<Tool::get_stmt_string(rhs)<<std::endl;
            if(bopr){
                std::string* ninsertStr=new std::string;
                *ninsertStr="";
                judgeDivWithoutUBFuzz(bopr,ninsertStr,count,SM,_rewriter,CallExprHead);

                delete ninsertStr;
            }
            else
            {
                ;//std::cout<<"what.";
            }
                
        }
        return ;
    }
    //mut插入模式下（无UBFUZZ），对bop下的AST中的所有可能的<<与>>进行查询和插入
    void SeqASTVisitor::judgeShfWithoutUBFuzz(const clang::BinaryOperator *bop,std::string* insertStr,int &count,clang::SourceManager& SM,clang::Rewriter &_rewriter,clang::SourceLocation& DefHead){
        if(bop == nullptr){
            ;//std::cout<<"warnning:encounter nullptr,maybe not a binaryoperator\n";
            return ;
        }
        clang::BinaryOperator::Opcode op = bop->getOpcode();
        clang::Expr *lhs=bop->getLHS();clang::Expr *rhs=bop->getRHS();

        if(op == clang::BinaryOperator::Opcode::BO_Shl || op == clang::BinaryOperator::Opcode::BO_Shr
        || op == clang::BinaryOperator::Opcode::BO_ShlAssign || op == clang::BinaryOperator::Opcode::BO_ShrAssign){
            
            if(!rhs->getBeginLoc().isMacroID()){
                generateArray(count,Tool::get_stmt_string(rhs),insertStr,"shf",SM);

                //DeclStmt: After
                _rewriter.InsertTextAfter(SM.getExpansionLoc(lhs->getBeginLoc()),*insertStr);
                
                clang::QualType type = lhs->getType();
                const clang::Type *ty = type.getTypePtr();
                unsigned bitWidth = _ctx->getTypeSize(ty);


                _rewriter.InsertTextBefore(DefHead,initSani_shf(count++,bitWidth,SM));
                ;//std::cout<<"count++"<<std::endl;
            }

            
        }
        else
            ;//std::cout<<"\topcode not match\n";

        if(PosShfOverflow(Tool::get_stmt_string(lhs))){
            ;//std::cout<<"check lhs:"<<Tool::get_stmt_string(lhs)<<std::endl;
            
            const clang::BinaryOperator *bopl=llvm::dyn_cast<clang::BinaryOperator>(lhs->IgnoreCasts());
            std::string type;

            GetSimplifiedExpr(lhs,bopl,type);

            if(bopl){
                std::string* ninsertStr=new std::string;
                *ninsertStr="";
                judgeShfWithoutUBFuzz(bopl,ninsertStr,count,SM,_rewriter,DefHead);

                delete ninsertStr;
            }
            
        }
        
        if(PosShfOverflow(Tool::get_stmt_string(rhs))){
            ;//std::cout<<"check rhs:"<<Tool::get_stmt_string(rhs)<<std::endl;

            
            const clang::BinaryOperator *bopr=llvm::dyn_cast<clang::BinaryOperator>(rhs->IgnoreParenCasts());
            std::string type;

            if(bopr)
            {
                // if(bop -> isAssignmentOp()){
                //     _rewriter.InsertTextBefore(SM.getExpansionLoc(rhs->getBeginLoc()), "(");

                //     auto end = clang::Lexer::getLocForEndOfToken(
                //                 SM.getExpansionLoc(rhs->getEndLoc()),
                //                 0, SM, _rewriter.getLangOpts()
                //             );
                //     _rewriter.InsertTextBefore(end, ")");
                // }

                std::string* ninsertStr=new std::string;
                *ninsertStr="";
                judgeShfWithoutUBFuzz(bopr,ninsertStr,count,SM,_rewriter,DefHead);

                delete ninsertStr;
            }
        }
        
        return ;

    }
    //mut插入模式下（无UBFUZZ），对某CallExpr bop下的AST中的所有可能的<<与>>进行查询和插入,位置固定在在CallExprHead防止误判为参数
    void SeqASTVisitor::judgeShfWithoutUBFuzz(const clang::BinaryOperator *bop,std::string* insertStr,int &count,clang::SourceManager& SM,clang::Rewriter &_rewriter,clang::SourceLocation& DefHead,clang::SourceLocation& CallExprHead){
        ;//std::cout<<"judge_insert_CallExpr:"<<std::endl;
        if(bop == nullptr){
            ;//std::cout<<"warnning:encounter nullptr,maybe not a binaryoperator\n";
            return ;
        }
        
        clang::BinaryOperator::Opcode op = bop->getOpcode();
        clang::Expr *lhs=bop->getLHS();clang::Expr *rhs=bop->getRHS();

        if(op == clang::BinaryOperator::Opcode::BO_Shl || op == clang::BinaryOperator::Opcode::BO_Shr
        || op == clang::BinaryOperator::Opcode::BO_ShlAssign || op == clang::BinaryOperator::Opcode::BO_ShrAssign){

            if(!rhs->getBeginLoc().isMacroID()){
                generateArray(count,Tool::get_stmt_string(rhs),insertStr,"shf",SM);

                _rewriter.InsertTextBefore(CallExprHead,*insertStr);

                clang::QualType type = lhs->getType();
                const clang::Type *ty = type.getTypePtr();
                unsigned bitWidth = _ctx->getTypeSize(ty);

                _rewriter.InsertTextBefore(DefHead,initSani_shf(count++,bitWidth,SM));
                ;//std::cout<<"count++"<<std::endl;
            }

        }
        else
            ;//std::cout<<"\topcode not match\n";

        ;//std::cout<<"check lhs:"<<Tool::get_stmt_string(lhs)<<std::endl;
        if(PosShfOverflow(Tool::get_stmt_string(lhs))){
            const clang::BinaryOperator *bopl=llvm::dyn_cast<clang::BinaryOperator>(lhs);
            std::string type;

            GetSimplifiedExpr(lhs,bopl,type);
            ;//std::cout<<"SUCC:"<<Tool::get_stmt_string(lhs)<<std::endl;
            if(bopl){
                std::string* ninsertStr=new std::string;
                *ninsertStr="";
                judgeShfWithoutUBFuzz(bopl,ninsertStr,count,SM,_rewriter,DefHead,CallExprHead);

                delete ninsertStr;
            }
                
        }

        ;//std::cout<<"check rhs:"<<Tool::get_stmt_string(rhs)<<std::endl;
        if(PosShfOverflow(Tool::get_stmt_string(rhs))){
            const clang::BinaryOperator *bopr=llvm::dyn_cast<clang::BinaryOperator>(rhs);
            std::string type;

            GetSimplifiedExpr(rhs,bopr,type);
            ;//std::cout<<"SUCC:"<<Tool::get_stmt_string(rhs)<<std::endl;
            if(bopr){
                std::string* ninsertStr=new std::string;
                *ninsertStr="";
                judgeShfWithoutUBFuzz(bopr,ninsertStr,count,SM,_rewriter,DefHead,CallExprHead);

                delete ninsertStr;
            }
                
        }
        return ;
    }

    //TODO:注释
    void SeqASTVisitor::JudgeAndInsert(clang::Stmt* &stmt,const clang::BinaryOperator *bop,clang::Rewriter &_rewriter,int &count,clang::SourceManager& SM,std::string type,std::string mode,clang::SourceLocation& DefHead){
        ;//std::cout<<type<<std::endl;

        if(stmt->getBeginLoc().isMacroID()){
            return;
        }

        stmt = IgnoreParenCastsStmt(stmt);
        bop = nullptr;
        
        if(type=="DeclStmt"){
            clang::DeclStmt* declstmt = llvm::dyn_cast<clang::DeclStmt>(stmt);
            for(auto phs = declstmt->decl_begin(); phs != declstmt->decl_end(); ++phs){
                if (clang::VarDecl *varDecl = llvm::dyn_cast<clang::VarDecl>(*phs)) {
                    if(varDecl->hasInit()){
                        clang::QualType QT = varDecl->getType();
                        if (QT.isConstQualified()) {
                            continue;// skip const
                        }

                        clang::Expr * expr = varDecl->getInit();
                        if(llvm::isa<clang::BinaryOperator>(expr)){

                            // clang::SourceLocation end = SM.getExpansionLoc(expr->getEndLoc());

                            // //has bug with marcro
                            // _rewriter.InsertTextAfter(end,")");
                            // _rewriter.InsertTextBefore(SM.getExpansionLoc(expr->getBeginLoc()),"(");

                            if(!(bop = llvm::dyn_cast<clang::BinaryOperator>(expr)))
                                GetSimplifiedExpr(expr,bop,expr->getStmtClassName());

                            if(bop){
                                clang::Expr* rhs=bop->getRHS();
                                clang::Expr* lhs=bop->getLHS();
                                if(rhs){
                                    _rewriter.InsertTextBefore(SM.getExpansionLoc(lhs->getBeginLoc()),"(");

                                    clang::SourceLocation afterToken =
                                    clang::Lexer::getLocForEndOfToken(SM.getExpansionLoc(rhs->getEndLoc()), 0, SM, _rewriter.getLangOpts());

                                    _rewriter.InsertTextBefore(afterToken, ")");
                                }
                                    

                            }
                            std::string* insertStr=new std::string;
                            *insertStr="";

                            if(mode == "div")
                                judgeDivWithoutUBFuzz(bop,insertStr,count,SM,_rewriter);
                            else if(mode == "shf")
                                judgeShfWithoutUBFuzz(bop,insertStr,count,SM,_rewriter,DefHead);
                            else
                                ;//std::cout<<"Error:Invalid J&I mode."<<std::endl;

                            delete insertStr;
                        }

                        
                    }
                }
            }
        }
        else if(type=="CallExpr"){
            clang::CallExpr* callexpr = llvm::dyn_cast<clang::CallExpr>(stmt);
            int numArgs = callexpr->getNumArgs();

            for(int i = 0; i < numArgs; i++){
                clang::Expr* arg = callexpr->getArg(i);
            //暂时无法应对call嵌套call的情况
            ;//std::cout<<"type is :"<<arg->getStmtClassName()<<std::endl;
                    if(!(bop = llvm::dyn_cast<clang::BinaryOperator>(arg)))
                        GetSimplifiedExpr(arg,bop,stmt->getStmtClassName());
                    
                    std::string* insertStr=new std::string;
                    *insertStr="";
                    clang::SourceLocation insertLoc = SM.getExpansionLoc(callexpr->getBeginLoc());

                    if(mode == "div")
                        judgeDivWithoutUBFuzz(bop,insertStr,count,SM,_rewriter,insertLoc);
                    else if(mode == "shf")
                        judgeShfWithoutUBFuzz(bop,insertStr,count,SM,_rewriter,DefHead,insertLoc);
                    else
                        ;//std::cout<<"Error:Invalid J&I mode."<<std::endl;

                    delete insertStr;
            }
        }
        else if(type=="ReturnStmt"){
            clang::ReturnStmt* r_stmt = llvm::dyn_cast<clang::ReturnStmt>(stmt);
            clang::Expr* expr = r_stmt->getRetValue();

            if(bop = llvm::dyn_cast<clang::BinaryOperator>(expr)){
                std::string* insertStr = new std::string;
                *insertStr  = "";

                if(mode == "div")
                    judgeDivWithoutUBFuzz(bop,insertStr,count,SM,_rewriter);
                else if(mode == "shf")
                    judgeShfWithoutUBFuzz(bop,insertStr,count,SM,_rewriter,DefHead);
                else
                    ;//std::cout<<"Error:Invalid J&I mode."<<std::endl;

                delete insertStr;
            }

        }
        else{
            ;//std::cout<<"judge_insert:"<<Tool::get_stmt_string(stmt)<<std::endl;

            clang::Expr* expr = llvm::dyn_cast<clang::Expr>(stmt);


            if(!(bop = llvm::dyn_cast<clang::BinaryOperator>(stmt)))
                GetSimplifiedExpr(expr,bop,stmt->getStmtClassName());

            std::string* insertStr=new std::string;
            *insertStr="";
            
            if(mode == "div")
                judgeDivWithoutUBFuzz(bop,insertStr,count,SM,_rewriter);
            else if(mode == "shf")
                judgeShfWithoutUBFuzz(bop,insertStr,count,SM,_rewriter,DefHead);
            else
                ;//std::cout<<"Error:Invalid J&I mode."<<std::endl;

            delete insertStr;
        }
    }

    bool SimplifyDeclRef(clang::Expr* rhs, clang::DeclRefExpr* &declexpr){
        bool decl_ref_flag = false;
        bool member_flag = false;
        bool array_flag =false;

        clang::MemberExpr* struct_member=nullptr;
        clang::ArraySubscriptExpr* array_expr=nullptr;

        do{
            member_flag=array_flag=false;

            if(declexpr = llvm::dyn_cast<clang::DeclRefExpr>(rhs)){
                decl_ref_flag = true;
                ;//std::cout<<"[decl_ref] activated"<<std::endl;
            }
                                
            else if(struct_member = llvm::dyn_cast<clang::MemberExpr>(rhs)){
                member_flag = true;
                rhs=struct_member->getBase()->IgnoreParenCasts();
                ;//std::cout<<"[member_expr] activated"<<std::endl;
            }
                                
            else if(array_expr = llvm::dyn_cast<clang::ArraySubscriptExpr>(rhs)){
                array_flag = true;
                rhs=array_expr->getBase()->IgnoreParenCasts();
                ;//std::cout<<"[array_expr] activated"<<std::endl;
            }
            else{
                ;//std::cout<<"trigered[break]"<<std::endl;
                break;
            }

        }while(member_flag || array_flag);
                            
        return decl_ref_flag && (!member_flag) && !(array_flag);
    }

    //BinaryOperator CallExpr
    //TODO
    void SeqASTVisitor::JudgeAndPtrTrack(clang::Stmt* stmt,int cur_count, clang::SourceManager& SM,clang::SourceLocation& DefHead,clang::SourceLocation& insertLoc){

        stmt = IgnoreParenCastsStmt(stmt);
        
        std::string type = stmt->getStmtClassName();
       ;//std::cout<<Tool::get_stmt_string(stmt)<<"---type is :"<<type<<std::endl;

        std::string varName;
        std::string typestr;
        
        //int a; && int a=1; int a = b+c
        if(clang::DeclStmt* declstmt = llvm::dyn_cast<clang::DeclStmt>(stmt)){
            for(auto it = declstmt->decl_begin(); it != declstmt->decl_end(); ++it){
                if(auto var = llvm::dyn_cast<clang::VarDecl>(*it)){
                   ;//std::cout<<Tool::get_stmt_string(stmt)<<std::endl;
                    if(var->hasInit() || var->getType()->isPointerType() || var->getType()->isReferenceType()){
                        clang::Expr* expr=var->getInit();
                        if(expr != nullptr){
                            std::string type = expr->getStmtClassName();
                                                    
                            if(NeedLoopPtrTrack(type)){
                                clang::Stmt* expr_stmt = expr->getExprStmt();
                                JudgeAndPtrTrack(expr_stmt, cur_count, SM, DefHead, insertLoc);
                            }
                        }
                        
                        
                        w_NameList.insert(var->getNameAsString());
                    }
                    else{
                        std::string* insertStr = new std::string;
                        *insertStr = "";

                        varName = var->getNameAsString();
                        typestr = var->getType().getAsString();

                        // if(!isIntegerVar(var)){
                        //    ;//std::cout<<"[SeqASTVisitor] Not an integer variable: "<<varName<<std::endl;
                        //     delete insertStr;
                        //     continue;
                        // }

                        //if(typestr == "va_list"){}

                        if(var->getDeclContext()->isTranslationUnit()) 
                            initNullPtr(varName,typestr,insertStr,SM,true);
                        else
                            initNullPtr(varName,typestr,insertStr,SM,false);
       
                        _rewriter.InsertTextAfter(insertLoc,*insertStr);
                        delete insertStr;
                    }
                }
            }
        }
        //a = 1; && a = malloc(); && i=0,j=0,k=0;
        else if(clang::BinaryOperator* bop = llvm::dyn_cast<clang::BinaryOperator>(stmt)){
            if(bop->isAssignmentOp()){
               ;//std::cout<<"check: is assign."<<std::endl;

                if(!isMallocAssignment(bop)){
                   ;//std::cout<<"check: is not a malloc."<<std::endl;
                    
                    clang::Expr* rhs = bop->getRHS();
                    clang::DeclRefExpr* declexpr=nullptr;
                    
                    if(rhs){
                        rhs = rhs -> IgnoreParenCasts();

                        if(llvm::isa<clang::MemberExpr>(rhs) || llvm::isa<clang::ArraySubscriptExpr>(rhs) || llvm::isa<clang::DeclRefExpr>(rhs)){
                            if(SimplifyDeclRef(rhs,declexpr)){
                                if (!llvm::isa<clang::FunctionDecl>(declexpr->getDecl())){
                                    clang::ValueDecl* value_decl = declexpr->getDecl();
                                    varName = value_decl->getNameAsString();
                                    typestr = value_decl->getType().getAsString();

                                    if(NotInWNameList(varName) && NotGlobalNorEnum(value_decl) && isIntegerVar(value_decl)){
                                       ;//std::cout<<"["<<varName<<"]not in WNameList"<<std::endl;
                                        std::string* insertStr = new std::string;
                                        *insertStr = "";

                                        if(value_decl->getDeclContext()->isTranslationUnit()) 
                                            generateNullPtr(varName,cur_count,typestr,insertStr,SM,true);
                                        else
                                            generateNullPtr(varName,cur_count,typestr,insertStr,SM,false);

                                        _rewriter.InsertTextAfter(insertLoc,*insertStr);

                                        delete insertStr;
                                    }
                                }
                            }
                            else if(declexpr!=nullptr){
                                if(llvm::isa<clang::FunctionDecl>(declexpr->getDecl()))
                                   ;//std::cout<<"[decl_ref] is a function."<<std::endl;
                            }
                            else{
                               ;//std::cout<<"[decl_ref] is not a decl_ref."<<std::endl;
                            }

                        }
                        else{
                            clang::Stmt* rhs_stmt = IgnoreParenCastsStmt(rhs);
                            JudgeAndPtrTrack(rhs_stmt, cur_count, SM, DefHead, insertLoc);
                        }
                            
                    }
                    
                    clang::Expr* lhs = bop->getLHS();
                    if(lhs){
                        lhs=lhs -> IgnoreParenCasts();
                        clang::DeclRefExpr* declexpr=nullptr;

                        if(SimplifyDeclRef(lhs,declexpr)){
                            // if (llvm::isa<clang::FunctionDecl>(declexpr->getDecl())) {
                            //     return;
                            // }
                            clang::ValueDecl* value_decl = declexpr->getDecl();
                            varName = value_decl->getNameAsString();
                            typestr = value_decl->getType().getAsString();
                        
                            if(NotInWNameList(varName) && NotGlobalNorEnum(value_decl) && isIntegerVar(value_decl)){
                                std::string* insertStr = new std::string;
                                *insertStr = "";

                                if(value_decl->getDeclContext()->isTranslationUnit()) 
                                    generateNullPtrAssign(varName,cur_count,typestr,insertStr,SM,true);
                                else
                                    generateNullPtrAssign(varName,cur_count,typestr,insertStr,SM,false);

                                _rewriter.InsertTextAfter(insertLoc,*insertStr);

                                delete insertStr;
                            }
                        }
                        // else{
                        //     clang::Stmt* lhs_stmt = IgnoreParenCastsStmt(lhs);
                        //     JudgeAndPtrTrack(lhs_stmt, cur_count, SM, DefHead, insertLoc);
                        // }
                    }
                }
            }
            else if(bop->getOpcode() == clang::BO_Comma){
                clang::Expr* lhs = bop->getLHS();
                if(lhs){
                    if(clang::BinaryOperator* lhs_bop = llvm::dyn_cast<clang::BinaryOperator>(lhs)){
                        clang::Stmt* lhs_stmt = IgnoreParenCastsStmt(lhs);
                        JudgeAndPtrTrack(lhs_stmt,cur_count,SM,DefHead,insertLoc);
                    }
                }

                clang::Expr* rhs = bop->getRHS();
                if(rhs){
                    if(clang::BinaryOperator* rhs_bop = llvm::dyn_cast<clang::BinaryOperator>(rhs)){
                        clang::Stmt* rhs_stmt = IgnoreParenCastsStmt(rhs);
                        JudgeAndPtrTrack(rhs_stmt,cur_count,SM,DefHead,insertLoc);
                    }
                }

            }
            else{//(a+b)
               ;//std::cout<<"check: is not assign."<<std::endl;
                    clang::Expr* rhs = bop->getRHS();
                    if(rhs){
                        //TODO: if(!auto rhs_op = dyn_cast<clang::BinaryOperator>(rhs)){}

                        rhs = rhs -> IgnoreParenCasts();
                        clang::DeclRefExpr* declexpr=nullptr;

                        if(SimplifyDeclRef(rhs,declexpr)){
                            clang::ValueDecl* value_decl = declexpr->getDecl();
                            varName = value_decl->getNameAsString();
                            typestr = value_decl->getType().getAsString();
                        
                            if(NotInWNameList(varName) && NotGlobalNorEnum(value_decl) && isIntegerVar(value_decl)){
                                std::string* insertStr = new std::string;
                                *insertStr = "";

                                if(value_decl->getDeclContext()->isTranslationUnit()) 
                                    generateNullPtr(varName,cur_count,typestr,insertStr,SM,true);
                                else
                                    generateNullPtr(varName,cur_count,typestr,insertStr,SM,false);

                                _rewriter.InsertTextAfter(insertLoc,*insertStr);

                                delete insertStr;
                            }
                        }
                        else{
                            clang::Stmt* rhs_stmt = IgnoreParenCastsStmt(rhs);
                            JudgeAndPtrTrack(rhs_stmt, cur_count, SM, DefHead, insertLoc);
                        }
                    }
                    
                    
                    clang::Expr* lhs = bop->getLHS();
                    if(lhs){
                        lhs = lhs -> IgnoreParenCasts();
                        clang::DeclRefExpr* declexpr=nullptr;

                        if(SimplifyDeclRef(lhs,declexpr)){
                            clang::ValueDecl* value_decl = declexpr->getDecl();
                            varName = value_decl->getNameAsString();
                            typestr = value_decl->getType().getAsString();
                        
                            if(NotInWNameList(varName) && NotGlobalNorEnum(value_decl) && isIntegerVar(value_decl)){
                                std::string* insertStr = new std::string;
                                *insertStr = "";

                                if(value_decl->getDeclContext()->isTranslationUnit()) 
                                    generateNullPtr(varName,cur_count,typestr,insertStr,SM,true);
                                else
                                    generateNullPtr(varName,cur_count,typestr,insertStr,SM,false);

                                _rewriter.InsertTextAfter(insertLoc,*insertStr);

                                delete insertStr;
                            }
                        }
                        else{
                            clang::Stmt* lhs_stmt = IgnoreParenCastsStmt(lhs);
                            JudgeAndPtrTrack(lhs_stmt, cur_count, SM, DefHead, insertLoc);
                        }
                    }
            }
        }
        //a++;
        else if(clang::UnaryOperator* uexpr = llvm::dyn_cast<clang::UnaryOperator>(stmt)){
            clang::Expr* a = uexpr->getSubExpr();
            if(a){
                a=a->IgnoreParenCasts();
                clang::DeclRefExpr* declexpr=nullptr;
               ;//std::cout<<"[UnaryOperator] activated"<<std::endl;
                        
                if(SimplifyDeclRef(a,declexpr)){
                    clang::ValueDecl* value_decl = declexpr->getDecl();
                    varName = value_decl->getNameAsString();
                    typestr = value_decl->getType().getAsString();
                
                    if(NotInWNameList(varName) && NotGlobalNorEnum(value_decl) && isIntegerVar(value_decl)){
                        std::string* insertStr = new std::string;
                        *insertStr = "";

                        if(value_decl->getDeclContext()->isTranslationUnit()) 
                            generateNullPtr(varName,cur_count,typestr,insertStr,SM,true);
                        else
                            generateNullPtr(varName,cur_count,typestr,insertStr,SM,false);

                        _rewriter.InsertTextAfter(insertLoc,*insertStr);

                        delete insertStr;
                    }
                }
            }
            clang::Stmt* a_stmt = IgnoreParenCastsStmt(a);

            JudgeAndPtrTrack(a_stmt,cur_count,SM,DefHead,insertLoc);
        }
        //print(a);
        else if(clang::CallExpr* callexpr = llvm::dyn_cast<clang::CallExpr>(stmt)){
            int numArgs = callexpr->getNumArgs();

            for(int i = 0; i < numArgs; i++){
                clang::Expr* arg = callexpr->getArg(i);
                if(arg){
                    arg=arg->IgnoreParenCasts();
                   ;//std::cout<<"arg"<<i<<": "<<arg->getStmtClassName()<<std::endl;
                }

                clang::DeclRefExpr* declexpr=nullptr;
                if(SimplifyDeclRef(arg,declexpr)){
                    if (llvm::isa<clang::FunctionDecl>(declexpr->getDecl())) {
                        continue;
                    }

                    clang::ValueDecl* value_decl = declexpr->getDecl();
                   ;//std::cout<<"check valuedecl exist"<<std::endl;

                    varName = value_decl->getNameAsString();
                    typestr = value_decl->getType().getAsString();

                    if(value_decl->getType()->isPointerType())
                        continue;
                
                    if(NotInWNameList(varName) && NotGlobalNorEnum(value_decl) && isIntegerVar(value_decl)){
                        std::string* insertStr = new std::string;
                        *insertStr = "";

                        if(value_decl->getDeclContext()->isTranslationUnit()) 
                            generateNullPtr(varName,cur_count,typestr,insertStr,SM,true);
                        else
                            generateNullPtr(varName,cur_count,typestr,insertStr,SM,false);

                        // const clang::LangOptions& LangOpts = _ctx->getLangOpts();
                        // clang::SourceLocation end = clang::Lexer::getLocForEndOfToken(callexpr->getEndLoc(), 0, SM, LangOpts);
                        _rewriter.InsertTextAfter(insertLoc,*insertStr);

                        delete insertStr;
                    }
                   ;//std::cout<<"check w_namelist finished"<<std::endl;
                }
                else{
                    std::string type=arg->getStmtClassName();
                    if(NeedLoopPtrTrack(type)){
                        clang::Stmt* arg_stmt = arg->getExprStmt();
                        JudgeAndPtrTrack(arg_stmt, cur_count, SM, DefHead, insertLoc);
                    }
                }

            }
           ;//std::cout<<"check judge finished"<<std::endl;
        }
        else if(clang::ConditionalOperator* cond = llvm::dyn_cast<clang::ConditionalOperator>(stmt)){
           ;//std::cout<<"[ConditionalOperator] activated"<<std::endl;

            clang::Expr* Cond = cond->getCond();
            if(Cond){
                Cond = Cond -> IgnoreParenCasts();
                clang::DeclRefExpr* declexpr=nullptr;

                if(SimplifyDeclRef(Cond,declexpr)){
                    clang::ValueDecl* value_decl = declexpr->getDecl();
                    varName = value_decl->getNameAsString();
                    typestr = value_decl->getType().getAsString();
                
                    if(NotInWNameList(varName) && NotGlobalNorEnum(value_decl) && isIntegerVar(value_decl)){
                        std::string* insertStr = new std::string;
                        *insertStr = "";

                        if(value_decl->getDeclContext()->isTranslationUnit()) 
                            generateNullPtr(varName,cur_count,typestr,insertStr,SM,true);
                        else
                            generateNullPtr(varName,cur_count,typestr,insertStr,SM,false);

                        _rewriter.InsertTextAfter(insertLoc,*insertStr);

                        delete insertStr;
                    }
                }
                else {
                    clang::Stmt* Cond_stmt = IgnoreParenCastsStmt(Cond);

                    JudgeAndPtrTrack(Cond_stmt,cur_count,SM,DefHead,insertLoc);
                }
                   
            }

            clang::Expr* trueExpr = cond->getTrueExpr();
            if(trueExpr){
                trueExpr = trueExpr -> IgnoreParenCasts();
                clang::DeclRefExpr* declexpr=nullptr;

                if(SimplifyDeclRef(trueExpr,declexpr)){
                    clang::ValueDecl* value_decl = declexpr->getDecl();
                    varName = value_decl->getNameAsString();
                    typestr = value_decl->getType().getAsString();
                
                    if(NotInWNameList(varName) && NotGlobalNorEnum(value_decl) && isIntegerVar(value_decl)){
                        std::string* insertStr = new std::string;
                        *insertStr = "";

                        if(value_decl->getDeclContext()->isTranslationUnit()) 
                            generateNullPtr(varName,cur_count,typestr,insertStr,SM,true);
                        else
                            generateNullPtr(varName,cur_count,typestr,insertStr,SM,false);

                        _rewriter.InsertTextAfter(insertLoc,*insertStr);

                        delete insertStr;
                    }
                }
                else {
                    clang::Stmt* true_stmt = IgnoreParenCastsStmt(trueExpr);

                    JudgeAndPtrTrack(true_stmt,cur_count,SM,DefHead,insertLoc);
                }
                    
            }

            clang::Expr* falseExpr = cond->getFalseExpr();
            if(falseExpr){
                falseExpr = falseExpr -> IgnoreParenCasts();
                clang::DeclRefExpr* declexpr=nullptr;

                if(SimplifyDeclRef(falseExpr,declexpr)){
                    clang::ValueDecl* value_decl = declexpr->getDecl();
                    varName = value_decl->getNameAsString();
                    typestr = value_decl->getType().getAsString();
                
                    if(NotInWNameList(varName) && NotGlobalNorEnum(value_decl) && isIntegerVar(value_decl) && !value_decl->getType()->isReferenceType()){
                        std::string* insertStr = new std::string;
                        *insertStr = "";

                        if(value_decl->getDeclContext()->isTranslationUnit()) 
                            generateNullPtr(varName,cur_count,typestr,insertStr,SM,true);
                        else
                            generateNullPtr(varName,cur_count,typestr,insertStr,SM,false);

                        _rewriter.InsertTextAfter(insertLoc,*insertStr);

                        delete insertStr;
                    }
                }
                else {
                    clang::Stmt* false_stmt = IgnoreParenCastsStmt(falseExpr);
  
                    JudgeAndPtrTrack(false_stmt,cur_count,SM,DefHead,insertLoc);
                }
                    
            }
        }
        else{
           ;//std::cout<<"[unknown] activated"<<std::endl;
        }
    }

    void SeqASTVisitor::JudgeByMode(clang::Stmt* &stmt,const clang::BinaryOperator *bop,clang::Rewriter &_rewriter,int &count,clang::SourceManager& SM,std::string type,std::string stmt_string,std::string mode,clang::SourceLocation& DefHead){
        if(NeedLoopChildren(type))
            LoopChildren(stmt,bop,_rewriter,count,SM,type,stmt_string,mode,DefHead);
        else if(mode == "div"){
            if(PosDivideZero(stmt_string))
                JudgeAndInsert(stmt,bop,_rewriter,count,SM,type,mode,DefHead);
        }
        else if(mode == "shf"){
            if(PosShfOverflow(stmt_string))
                JudgeAndInsert(stmt,bop,_rewriter,count,SM,type,mode,DefHead);
        }
        else if(mode == "null"){
            clang::SourceLocation insertLoc = DefHead;
            JudgeAndPtrTrack(stmt,count,SM,DefHead,insertLoc);
        }  
    }
    
    //对于需要循环遍历其子语句的Stmt（NeedLoopChildren）遍历其子语句
    void SeqASTVisitor::LoopChildren(clang::Stmt*& stmt,const clang::BinaryOperator *bop,clang::Rewriter &_rewriter, int &count, clang::SourceManager& SM, std::string type,std::string stmt_string,std::string mode,clang::SourceLocation& DefHead){
       ;//std::cout<<"type:["<<type<<"] need to Loop children"<<std::endl;

        bool loop_record = OUTLOOP;
        clang::SourceLocation insertLoc;

        if(type=="ForStmt"){
            clang::ForStmt *FS = llvm::dyn_cast<clang::ForStmt>(stmt);
            clang::Stmt* s_stmt;

            if(!OUTLOOP){
                OUTLOOP = true;
            }

            FOR_COND = true;
            //for三条件需要特殊格式
            for(int i=0;i<3;i++){
                if(i==0)
                    s_stmt=FS->getInit();
                else if(i==1)
                    s_stmt=FS->getCond();
                else
                    s_stmt=FS->getInc();
                
                if(!s_stmt)
                    continue;
                
                stmt_string=Tool::get_stmt_string(s_stmt);

                type = s_stmt->getStmtClassName();
               ;//std::cout<<"------check cond:"<<stmt_string<<"--------"<<std::endl;

                if(mode == "null")
                    DefHead = SM.getExpansionLoc(FS->getBeginLoc());
                    
                JudgeByMode(s_stmt,bop,_rewriter,count,SM,type,stmt_string,mode,DefHead);      
            }
            FOR_COND = false;

            clang::Stmt *sbody=FS->getBody();
            if(clang::CompoundStmt *CS = llvm::dyn_cast<clang::CompoundStmt>(sbody))
                for (clang::Stmt* s_stmt : CS->body() ){
                    stmt_string=Tool::get_stmt_string(s_stmt);

                    type = s_stmt->getStmtClassName();
                    //std::cout<<"------check body:"<<stmt_string<<"--------"<<std::endl;

                    // if(mode == "null"){
                    //     const clang::LangOptions& LangOpts = _ctx->getLangOpts();
                    //     clang::SourceLocation afterLBrace = clang::Lexer::getLocForEndOfToken(
                    //         SM.getExpansionLoc(CS->getLBracLoc()), 0, SM, LangOpts
                    //     );
                    //     DefHead = SM.getExpansionLoc(afterLBrace);
                    // }
                    if(mode == "null")
                        DefHead = SM.getExpansionLoc(s_stmt->getBeginLoc());

                    JudgeByMode(s_stmt,bop,_rewriter,count,SM,type,stmt_string,mode,DefHead);
                                    
                }
            else{
                if(!llvm::isa<clang::IfStmt>(sbody) && !llvm::isa<clang::ForStmt>(sbody) && !llvm::isa<clang::NullStmt>(sbody)){
                    clang::SourceLocation startLoc = sbody->getBeginLoc();
                    _rewriter.InsertTextBefore(startLoc, "{");

                    clang::SourceLocation endLoc = sbody->getEndLoc();
                    clang::SourceLocation afterSemi = clang::Lexer::findLocationAfterToken(
                    endLoc, clang::tok::semi, SM, _rewriter.getLangOpts(), /*SkipTrailingWhitespace=*/true);

                    _rewriter.InsertTextBefore(afterSemi, "}\n");
                }
                

                // endLoc = clang::Lexer::getLocForEndOfToken(endLoc, 0, 
                //                                    _rewriter.getSourceMgr(), 
                //                                    _rewriter.getLangOpts());
                // _rewriter.InsertTextAfter(endLoc, "\n}");
                
                stmt_string=Tool::get_stmt_string(sbody);

                type = sbody->getStmtClassName();

                if(NeedLoopChildren(type))
                    LoopChildren(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),stmt_string,mode,DefHead);
                
                else if(mode == "div"){
                    if(PosDivideZero(stmt_string))
                        JudgeAndInsert(sbody,bop,_rewriter,count,SM,sbody->getStmtClassName(),mode,DefHead);
                }
                else if(mode == "shf"){
                    if(PosShfOverflow(stmt_string))
                        JudgeAndInsert(sbody,bop,_rewriter,count,SM,sbody->getStmtClassName(),mode,DefHead);
                }
                else if(mode == "null"){
                    insertLoc = SM.getExpansionLoc(sbody->getBeginLoc());
                    JudgeAndPtrTrack(sbody,count,SM,DefHead,insertLoc);
                }
            }
            
        }
        else if(type == "WhileStmt"){
            clang::WhileStmt *WS = llvm::dyn_cast<clang::WhileStmt>(stmt);
            clang::Stmt *scond=WS->getCond();

            if(mode == "null"){
                DefHead = SM.getExpansionLoc(WS->getBeginLoc());
            }

            if(scond){
                
                stmt_string=Tool::get_stmt_string(scond);

                type = scond->getStmtClassName();

                if(NeedLoopChildren(type))
                    LoopChildren(scond,bop,_rewriter,count,SM,scond->getStmtClassName(),stmt_string,mode,DefHead);

                else if(mode == "div"){
                    if(PosDivideZero(stmt_string))
                        JudgeAndInsert(scond,bop,_rewriter,count,SM,scond->getStmtClassName(),mode,DefHead);
                }
                else if(mode == "shf"){
                    if(PosShfOverflow(stmt_string))
                        JudgeAndInsert(scond,bop,_rewriter,count,SM,scond->getStmtClassName(),mode,DefHead);
                }
                else if(mode == "null"){
                    insertLoc = SM.getExpansionLoc(WS->getBeginLoc());
                    JudgeAndPtrTrack(scond,count,SM,DefHead,insertLoc);
                }
                                
            }

            clang::Stmt *sbody=WS->getBody();
            type = sbody->getStmtClassName();
            if(clang::CompoundStmt* while_comp = llvm::dyn_cast<clang::CompoundStmt>(sbody))
                for (auto &s_stmt : while_comp->children() ){

                stmt_string=Tool::get_stmt_string(s_stmt);
                type = s_stmt->getStmtClassName();
                
                if(mode == "null")
                    DefHead = SM.getExpansionLoc(while_comp->getLBracLoc());

                if(NeedLoopChildren(type))
                    LoopChildren(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),stmt_string,mode,DefHead);

                else if(mode == "div"){
                    if(PosDivideZero(stmt_string))
                        JudgeAndInsert(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),mode,DefHead);
                }
                else if(mode == "shf"){
                    if(PosShfOverflow(stmt_string))
                        JudgeAndInsert(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),mode,DefHead);
                }
                else if(mode == "null"){
                    insertLoc=SM.getExpansionLoc(s_stmt->getBeginLoc());
                    JudgeAndPtrTrack(s_stmt,count,SM,DefHead,insertLoc);
                }
                                
            }
            else if(NeedLoopChildren(type)){
                LoopChildren(sbody,bop,_rewriter,count,SM,type,stmt_string,mode,DefHead);
            }
        }
        else if(type == "DoStmt"){
            clang::DoStmt *WS = llvm::dyn_cast<clang::DoStmt>(stmt);
            clang::Stmt *scond=WS->getCond();

            if(mode == "null"){
                DefHead = SM.getExpansionLoc(WS->getBeginLoc());
            }

            if(scond){
                stmt_string=Tool::get_stmt_string(scond);

                type = scond->getStmtClassName();

                if(NeedLoopChildren(type))
                    LoopChildren(scond,bop,_rewriter,count,SM,scond->getStmtClassName(),stmt_string,mode,DefHead);
                else if(mode == "div"){
                    if(PosDivideZero(stmt_string))
                        JudgeAndInsert(scond,bop,_rewriter,count,SM,scond->getStmtClassName(),mode,DefHead);
                }
                else if(mode == "shf"){
                    if(PosShfOverflow(stmt_string))
                        JudgeAndInsert(scond,bop,_rewriter,count,SM,scond->getStmtClassName(),mode,DefHead);
                }
                else if(mode == "null"){
                    insertLoc = SM.getExpansionLoc(WS->getBeginLoc());
                    JudgeAndPtrTrack(scond,count,SM,DefHead,insertLoc);
                }
                                
            }

            clang::Stmt *sbody=WS->getBody();
            type = sbody->getStmtClassName();
            if(clang::CompoundStmt* while_comp = llvm::dyn_cast<clang::CompoundStmt>(sbody))
                for (auto &s_stmt : while_comp->children() ){
                stmt_string=Tool::get_stmt_string(s_stmt);

                type = s_stmt->getStmtClassName();

                if(mode == "null")
                    DefHead = SM.getExpansionLoc(while_comp->getLBracLoc());

                if(NeedLoopChildren(type))
                    LoopChildren(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),stmt_string,mode,DefHead);

                else if(mode == "div"){
                    if(PosDivideZero(stmt_string))
                        JudgeAndInsert(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),mode,DefHead);
                }
                else if(mode == "shf"){
                    if(PosShfOverflow(stmt_string))
                        JudgeAndInsert(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),mode,DefHead);
                }
                else if(mode == "null"){
                    insertLoc=SM.getExpansionLoc(s_stmt->getBeginLoc());
                    JudgeAndPtrTrack(s_stmt,count,SM,DefHead,insertLoc);
                }
                                
            }
            else if(NeedLoopChildren(type)){
                LoopChildren(sbody,bop,_rewriter,count,SM,type,stmt_string,mode,DefHead);
            }
        }
        else if(type == "IfStmt"){
            //查询条件
            clang::IfStmt *IS = llvm::dyn_cast<clang::IfStmt>(stmt);
                            
            clang::Stmt *icond=IS->getCond();
            //for (auto &s_stmt : ibody->children() ){
                stmt_string=Tool::get_stmt_string(icond);
               ;//std::cout<<"------check cond:"<<stmt_string<<"--------"<<std::endl;

            if(mode == "null"){
                DefHead = SM.getExpansionLoc(IS->getBeginLoc());
            }

                type = icond->getStmtClassName();

                if(NeedLoopChildren(type))
                LoopChildren(icond,bop,_rewriter,count,SM,icond->getStmtClassName(),stmt_string,mode,DefHead);

                else if(mode == "div"){
                    if(PosDivideZero(stmt_string))
                        JudgeAndInsert(icond,bop,_rewriter,count,SM,icond->getStmtClassName(),mode,DefHead);
                }
                else if(mode == "shf"){
                    if(PosShfOverflow(stmt_string))
                        JudgeAndInsert(icond,bop,_rewriter,count,SM,icond->getStmtClassName(),mode,DefHead);
                }
                else if(mode == "null"){
                    insertLoc = SM.getExpansionLoc(IS->getBeginLoc());
                    JudgeAndPtrTrack(icond,count,SM,DefHead,insertLoc);
                }
            
            //}
            clang::Stmt *ibody=IS->getThen();
            if(clang::CompoundStmt* sbody = llvm::dyn_cast<clang::CompoundStmt>(ibody)){
                for (auto &s_stmt : sbody->children() ){
                    stmt_string=Tool::get_stmt_string(s_stmt);
                   ;//std::cout<<"------check then_body:"<<stmt_string<<"--------"<<std::endl;

                    type = s_stmt->getStmtClassName();

                    if(mode == "null")
                        DefHead = SM.getExpansionLoc(sbody->getLBracLoc());

                    if(NeedLoopChildren(type))
                            LoopChildren(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),stmt_string,mode,DefHead);

                        else if(mode == "div"){
                            if(PosDivideZero(stmt_string))
                                JudgeAndInsert(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),mode,DefHead);
                        }
                        else if(mode == "shf"){
                            if(PosShfOverflow(stmt_string))
                                JudgeAndInsert(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),mode,DefHead);
                        }
                        else if(mode == "null"){
                            insertLoc=SM.getExpansionLoc(s_stmt->getBeginLoc());
                            JudgeAndPtrTrack(s_stmt,count,SM,DefHead,insertLoc);
                        }    
                }
            }
            else{
                if(!llvm::isa<clang::IfStmt>(ibody) && !llvm::isa<clang::ForStmt>(ibody) && !llvm::isa<clang::NullStmt>(ibody)){
                    clang::SourceLocation startLoc = ibody->getBeginLoc();
                    _rewriter.InsertTextBefore(startLoc, "{");

                    clang::SourceLocation endLoc = ibody->getEndLoc();

                    clang::SourceLocation afterSemi = clang::Lexer::findLocationAfterToken(
                    endLoc, clang::tok::semi, SM, _rewriter.getLangOpts(), /*SkipTrailingWhitespace=*/true);

                    _rewriter.InsertTextBefore(afterSemi, "}\n");

                    if(mode == "null")
                        DefHead = startLoc;
                }

                // endLoc = clang::Lexer::getLocForEndOfToken(endLoc, 0, 
                //                                    _rewriter.getSourceMgr(), 
                //                                    _rewriter.getLangOpts());
                // _rewriter.InsertTextAfter(endLoc, "\n}");


                // stmt_string=Tool::get_stmt_string(ibody);

               ;//std::cout<<stmt_string<<":---need insert{}"<<std::endl;
                // _rewriter.InsertTextBefore(ibody->getBeginLoc(),"{");

                // const clang::LangOptions& LangOpts = _ctx->getLangOpts();
                // clang::SourceLocation end = clang::Lexer::getLocForEndOfToken(ibody->getEndLoc(), 0, SM, LangOpts);
                // _rewriter.InsertText(end,"}");

                    type = ibody->getStmtClassName();

                    if(NeedLoopChildren(type))
                            LoopChildren(ibody,bop,_rewriter,count,SM,ibody->getStmtClassName(),stmt_string,mode,DefHead);

                        else if(mode == "div"){
                            if(PosDivideZero(stmt_string))
                                JudgeAndInsert(ibody,bop,_rewriter,count,SM,ibody->getStmtClassName(),mode,DefHead);
                        }
                        else if(mode == "shf"){
                            if(PosShfOverflow(stmt_string))
                                JudgeAndInsert(ibody,bop,_rewriter,count,SM,ibody->getStmtClassName(),mode,DefHead);
                        }
                        else if(mode == "null"){
                            insertLoc=SM.getExpansionLoc(ibody->getBeginLoc());
                            JudgeAndPtrTrack(ibody,count,SM,DefHead,insertLoc);
                        }  
            }
                
                                
            ibody = IS->getElse();
            if(ibody)
                if(clang::CompoundStmt* sbody = llvm::dyn_cast<clang::CompoundStmt>(ibody)){
                    for (auto &s_stmt : sbody->children() ){
                        stmt_string=Tool::get_stmt_string(s_stmt);
                       ;//std::cout<<"------check else_body:"<<stmt_string<<"--------"<<std::endl;

                        type = s_stmt->getStmtClassName();

                        if(mode == "null")
                            DefHead = SM.getExpansionLoc(sbody->getLBracLoc());

                        if(NeedLoopChildren(type))
                                LoopChildren(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),stmt_string,mode,DefHead);

                            else if(mode == "div"){
                                if(PosDivideZero(stmt_string))
                                    JudgeAndInsert(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),mode,DefHead);
                            }
                            else if(mode == "shf"){
                                if(PosShfOverflow(stmt_string))
                                    JudgeAndInsert(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),mode,DefHead);
                            }
                            else if(mode == "null"){
                                insertLoc=SM.getExpansionLoc(s_stmt->getBeginLoc());
                                JudgeAndPtrTrack(s_stmt,count,SM,DefHead,insertLoc);
                            }    
                    }
                }
                else{
                    if(!llvm::isa<clang::IfStmt>(ibody) && !llvm::isa<clang::ForStmt>(ibody) && !llvm::isa<clang::NullStmt>(ibody)){
                        clang::SourceLocation startLoc = ibody->getBeginLoc();
                        _rewriter.InsertTextBefore(startLoc, "{");

                        clang::SourceLocation endLoc = ibody->getEndLoc();

                        clang::SourceLocation afterSemi = clang::Lexer::findLocationAfterToken(
                        endLoc, clang::tok::semi, SM, _rewriter.getLangOpts(), /*SkipTrailingWhitespace=*/true);

                        _rewriter.InsertTextBefore(afterSemi, "}\n");

                        if (mode == "null")
                            DefHead = startLoc;

                    }

                    stmt_string=Tool::get_stmt_string(ibody);
                        type = ibody->getStmtClassName();

                        if(NeedLoopChildren(type))
                                LoopChildren(ibody,bop,_rewriter,count,SM,ibody->getStmtClassName(),stmt_string,mode,DefHead);

                            else if(mode == "div"){
                                if(PosDivideZero(stmt_string))
                                    JudgeAndInsert(ibody,bop,_rewriter,count,SM,ibody->getStmtClassName(),mode,DefHead);
                            }
                            else if(mode == "shf"){
                                if(PosShfOverflow(stmt_string))
                                    JudgeAndInsert(ibody,bop,_rewriter,count,SM,ibody->getStmtClassName(),mode,DefHead);
                            }
                            else if(mode == "null"){
                                insertLoc=SM.getExpansionLoc(ibody->getBeginLoc());
                                JudgeAndPtrTrack(ibody,count,SM,DefHead,insertLoc);
                            }  
                }
        }
        else if(type == "CompoundStmt")
        {
            clang::CompoundStmt *CS = llvm::dyn_cast<clang::CompoundStmt>(stmt);
            for (auto &s_stmt : CS->body()) 
            {
               ;//std::cout<<Tool::get_stmt_string(s_stmt)<<std::endl;

                stmt_string=Tool::get_stmt_string(s_stmt);
               ;//std::cout<<"CompountStmt::"<<stmt_string<<std::endl;

                type = s_stmt->getStmtClassName();

                if(mode == "null")
                    DefHead = SM.getExpansionLoc(CS->getLBracLoc());

                if(NeedLoopChildren(type))
                        LoopChildren(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),stmt_string,mode,DefHead);

                    else if(mode == "div"){
                        if(PosDivideZero(stmt_string))
                            JudgeAndInsert(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),mode,DefHead);
                    }
                    else if(mode == "shf"){
                        if(PosShfOverflow(stmt_string))
                            JudgeAndInsert(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),mode,DefHead);
                    }
                    else if(mode == "null"){
                        insertLoc=SM.getExpansionLoc(s_stmt->getBeginLoc());
                        JudgeAndPtrTrack(s_stmt,count,SM,DefHead,insertLoc);
                    }
            }
        }
        else if(type == "SwitchStmt"){
            clang::SwitchStmt *SS = llvm::dyn_cast<clang::SwitchStmt>(stmt);
            clang::Stmt *s_stmt=SS->getCond();

            ;//std::cout<<"switch in."<<std::endl;
            if(mode == "null"){
                DefHead = SM.getExpansionLoc(SS->getBeginLoc());
            }

            if(s_stmt){
               ;//std::cout<<"s_stmt yes."<<std::endl;
                stmt_string=Tool::get_stmt_string(s_stmt);

                type = s_stmt->getStmtClassName();

                if(NeedLoopChildren(type))
                        LoopChildren(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),stmt_string,mode,DefHead);

                    else if(mode == "div"){
                        if(PosDivideZero(stmt_string))
                            JudgeAndInsert(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),mode,DefHead);
                    }
                    else if(mode == "shf"){
                        if(PosShfOverflow(stmt_string))
                            JudgeAndInsert(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),mode,DefHead);
                    }
                    else if(mode == "null"){
                        insertLoc=SM.getExpansionLoc(SS->getBeginLoc());
                        JudgeAndPtrTrack(s_stmt,count,SM,DefHead,insertLoc);
                    }
            }
            
                                   
            clang::Stmt *sbody=  SS->getBody();

           ;//std::cout<<"sbody"<<std::endl;

            for(clang::Stmt* subStmt : sbody->children()){
                

                while(clang::CaseStmt* case_stmt=llvm::dyn_cast<clang::CaseStmt>(subStmt)){
                    subStmt=case_stmt->getSubStmt();
                }

                if(clang::DefaultStmt* default_stmt=llvm::dyn_cast<clang::DefaultStmt>(subStmt)){
                    subStmt=default_stmt->getSubStmt();
                }

                type = subStmt->getStmtClassName();
                stmt_string=Tool::get_stmt_string(subStmt);

                if(mode == "null")
                    DefHead = SM.getExpansionLoc(subStmt->getBeginLoc());
                
                //std::cout<<"=======================switch sub:======================="<<stmt_string<<std::endl;

                if(NeedLoopChildren(type))
                    LoopChildren(subStmt,bop,_rewriter,count,SM,subStmt->getStmtClassName(),stmt_string,mode,DefHead);

                else if(mode == "div"){
                    if(PosDivideZero(stmt_string))
                        JudgeAndInsert(subStmt,bop,_rewriter,count,SM,subStmt->getStmtClassName(),mode,DefHead);
                }
                else if(mode == "shf"){
                    if(PosShfOverflow(stmt_string))
                        JudgeAndInsert(subStmt,bop,_rewriter,count,SM,subStmt->getStmtClassName(),mode,DefHead);
                }
                else if(mode == "null"){
                    insertLoc = SM.getExpansionLoc(subStmt->getBeginLoc());
                    JudgeAndPtrTrack(subStmt,count,SM,DefHead,insertLoc);
                }
            }
        }
        else if(type == "LabelStmt"){
            clang::LabelStmt *LS = llvm::dyn_cast<clang::LabelStmt>(stmt);
            auto s_stmt = LS->getSubStmt();

           ;//std::cout<<Tool::get_stmt_string(s_stmt)<<std::endl;

            stmt_string=Tool::get_stmt_string(s_stmt);

            type = s_stmt->getStmtClassName();

            if(mode == "null"){
                DefHead = SM.getExpansionLoc(s_stmt->getBeginLoc());
            }

            if(NeedLoopChildren(type))
                        LoopChildren(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),stmt_string,mode,DefHead);

                    else if(mode == "div"){
                        if(PosDivideZero(stmt_string))
                            JudgeAndInsert(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),mode,DefHead);
                    }
                    else if(mode == "shf"){
                        if(PosShfOverflow(stmt_string))
                            JudgeAndInsert(s_stmt,bop,_rewriter,count,SM,s_stmt->getStmtClassName(),mode,DefHead);
                    }
                    else if(mode == "null"){
                        insertLoc = SM.getExpansionLoc(s_stmt->getBeginLoc());
                        JudgeAndPtrTrack(s_stmt,count,SM,DefHead,insertLoc);
                    }
        }

        if(!loop_record)
            OUTLOOP = false;
    }

    #pragma endregion

    #pragma region find something
    //mode[UBFUZZ]: 定位UBFUZZ位置并存入line和column
    void find_UB(int &l,int &c,const std::string& fname){
        std::ifstream file(fname);
        if(!file){
           ;//std::cout<<"file not exists--in find_UB"<<std::endl;
            return ;
        }
        l=0;
        std::string cur;
        while(std::getline(file,cur)){
            l++;
            if(cur.find("/*UBFUZZ*/") != -1){
                c=cur.find("/*UBFUZZ*/");
               ;//std::cout<<"line:"<<l<<" column:"<<c<<std::endl;
                return ;
            }
        }
        l=-1;
        return;
    }
    //mode[UBFUZZ][shf]: 定位UBFUZZ(INTOP)位置(next line)
    void find_INTOP(std::pair<int,int> &intopl, std::pair<int,int> &intopr,const std::string& fname,int UB_line){
        std::ifstream file(fname);
        if(!file){
           ;//std::cout<<"file not exists--in find_INTOP"<<std::endl;
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
            }
            else{
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
    //TODO:需要改名，因为找并补充了俩前置库(stddef.h|stdlib.h)
    void find_stdlib(const std::string& fname,clang::Rewriter &_rewriter,clang::FileID fid){
        std::ifstream file(fname);
        if(!file){
            ;//std::cout<<"file not exists--in find_UB"<<std::endl;
            return ;
        }

        std::string cur;

        clang::SourceManager &SM = _rewriter.getSourceMgr();
        clang::SourceLocation insertLoc = SM.getLocForStartOfFile(fid);

        bool has_stdlib = false;
        bool has_stddef = false;

        int cur_line = 0;
        int last_include_line = -1;

        //if (SM.isInMainFile(insertLoc)) {
            while(std::getline(file,cur)){
                cur_line++;
                if(cur.find("#include") != std::string::npos){
                    last_include_line = cur_line;
                }

                if(cur.find("stdlib.h") != std::string::npos){
                    has_stdlib = true;
                }
                if(cur.find("stddef.h") != std::string::npos){
                    has_stddef = true;
                }
            }
        //}
 
        if(last_include_line != -1){
            clang::SourceLocation lineStart = SM.translateLineCol(fid,last_include_line,0);
            insertLoc = clang::Lexer::getLocForEndOfToken(lineStart, 0, SM, clang::LangOptions());
        }

        if(!has_stdlib){
            _rewriter.InsertText(insertLoc,"\n#include <stdlib.h>\n");
        }
        if(!has_stddef){
            _rewriter.InsertText(insertLoc,"\n#include <stddef.h>\n");
        }

        return;
    } 

    #pragma endregion

    #pragma region Visit Decls

    bool SeqASTVisitor::VisitFunctionDecl(clang::FunctionDecl *func_decl){

        if(func_decl == nullptr) return true;
        //std::cout<<"[Visiting]"<<func_decl->getNameAsString()<<std::endl;

        if(ContainSpecialChar(func_decl->getNameAsString())) return true;

        global_w_NameList.insert(func_decl->getNameAsString());

        if (!_ctx->getSourceManager().isInMainFile(func_decl->getLocation())) {
            return true;
        }

        if (isInSystemHeader(func_decl->getLocation()))
            return true;

        //*****打印AST
        //func_decl->dump();
        //*****
        
        clang::SourceManager &SM = _ctx->getSourceManager();

        std::cout << "[File location]: " 
         << SM.getFileLoc(func_decl->getLocation()).printToString(SM) << "\n";

        std::string filePath_abs = SM.getFilename(func_decl->getLocation()).str();
        std::string filePath =filePath_abs;

        clang::FileID fid = SM.getFileID(SM.getFileLoc(func_decl->getBeginLoc()));
        const clang::FileEntry *FE = SM.getFileEntryForID(fid);

        clang::SourceLocation insertLoc = SM.getLocForStartOfFile(fid);

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
        if(getFlag(DIV_BIT) && !getFlag(MUT_BIT) ){
            find_UB(UBFUZZ_line,UBFUZZ_column,filePath);
            if(UBFUZZ_line==-1)
                ;//std::cout<<"Cant find a UBFUZZ sign"<<std::endl;
            else
                ;//std::cout<<"UBFUZZ in line"<<UBFUZZ_line<<std::endl;
        }
        //--------UBFUZZ localized end--------

        std::pair<int,int> intopl_loc=std::make_pair(-1,-1);
        std::pair<int,int> intopr_loc=std::make_pair(-1,-1);
        int bits=-1;

        if(getFlag(NULL_BIT)){
            if(!VisitedFiles.count(FE)){
                find_stdlib(filePath,_rewriter,fid);
                VisitedFiles.insert(FE);
            }
                

            w_NameList.clear();
            //global_w_NameList.insert(func_decl->getNameAsString());
            
            int param_num=func_decl->getNumParams();
            
            for(int i=0;i<param_num;i++){
                std::string param_name=func_decl->getParamDecl(i)->getNameAsString();
                w_NameList.insert(param_name);
            }


            // if(func_decl->getNameAsString() == "main")
            //     return true;

            ;//std::cout<<"\n| Function [" << func_decl->getNameAsString() << "] defined in: " << filePath << "\n";
            int cur_count = count;

            std::string func_name = func_decl->getNameAsString();
            global_current_func_name = func_name;

            clang::SourceLocation defHead = SM.getExpansionLoc(func_decl->getBeginLoc());
            clang::SourceLocation CatcherLoc = defHead;

            std::string* insertCatcher = new std::string;
            *insertCatcher = "";

            ;//std::cout<<"check init catcher init"<<std::endl;

            initNullPtrCatcher("no_need",cur_count,insertCatcher,SM);
            if(SM.isWrittenInMainFile(CatcherLoc))
                _rewriter.InsertTextAfter(CatcherLoc,*insertCatcher);
            else
               ;//std::cout<<"[[Fatal Error]]: Not written in main file"<<std::endl;

            if (func_decl == func_decl->getDefinition()){

                ;//std::cout<<"[FuncDecl]current global:"<<global_current_func_name<<std::endl;
                ;//std::cout<<"check func_decl started"<<std::endl;

                clang::Stmt *body_stmt = func_decl->getBody();
                
                const clang::BinaryOperator *bop = nullptr;

                if(body_stmt == nullptr){
                    return true;
                }

                for (auto &stmt : body_stmt->children()){
                    std::string type = stmt->getStmtClassName();
                    std::string stmt_string = Tool::get_stmt_string(stmt);

                    clang::SourceLocation insertLoc = SM.getExpansionLoc(stmt->getBeginLoc());

                    ;//std::cout<<"check:"<<stmt_string<<std::endl;

                    
                    if(NeedLoopChildren(type)){
                        LoopChildren(stmt,nullptr,_rewriter, cur_count, SM, type, stmt_string, "null", defHead);
                    }
                    else{
                        JudgeAndPtrTrack(stmt,cur_count,SM,defHead,insertLoc);
                    }

                }

            }
            count++;
        }
        else{
            //--------shift redirection--------        
            if(!getFlag(DIV_BIT) && !getFlag(MUT_BIT)){
                find_INTOP(intopl_loc,intopr_loc,filePath,UBFUZZ_line);
                UBFUZZ_line=intopl_loc.first;
            }
            ;//std::cout<<"check:"<<intopl_loc.first<<":"<<intopl_loc.second<<" & "<<intopr_loc.second<<std::endl;
            //--------shift redirection end -------- 

            std::string func_name = func_decl->getNameAsString();
            ;//std::cout<<"\n| Function [" << func_decl->getNameAsString() << "] defined in: " << filePath << "\n";
        
            if (func_decl == func_decl->getDefinition()){

                clang::Stmt *body_stmt = func_decl->getBody();
                //SaniCatcherPairs count
                            
                //遍历这个函数中的语句
                if(getFlag(DIV_BIT) && getFlag(MUT_BIT)){
                    const clang::BinaryOperator *bop = nullptr;
                    //clang::BinaryOperator::Opcode op;
                    //clang::Expr *lhs; clang::Expr *rhs;

                    for (auto &stmt : body_stmt->children())
                    {
                        auto ori_stmt = stmt;
                        clang::SourceLocation insert_loc = SM.getExpansionLoc(ori_stmt->getBeginLoc());

                        std::string stmt_string=Tool::get_stmt_string(stmt);
                        
                        if(PosDivideZero(stmt_string))
                        {
                            if(ori_stmt->getBeginLoc().isMacroID()){
                                continue;
                            }

                            ;//std::cout<<"stmt:"<<stmt_string<<std::endl;
                            std::string type = stmt->getStmtClassName();
                            clang::SourceLocation defHead = SM.getExpansionLoc(func_decl->getBeginLoc());
                            
                            if(NeedLoopChildren(type))
                                LoopChildren(stmt,bop,_rewriter,count,SM,type,stmt_string,"div",defHead);
                            else
                                JudgeAndInsert(stmt,bop,_rewriter,count,SM,stmt->getStmtClassName(),"div",defHead);     
                        }
                        else continue;
                    }
                }
                else if(!getFlag(DIV_BIT) && getFlag(MUT_BIT)){
                    ;//std::cout<<"mode: Shf-Mut"<<std::endl;

                    const clang::BinaryOperator *bop = nullptr;
                    for (auto &stmt : body_stmt->children())
                    {
                        DO_WHILE_MACRO = false;
                        auto ori_stmt = stmt;
                        clang::SourceLocation insert_loc = SM.getExpansionLoc(ori_stmt->getBeginLoc());

                        std::string stmt_string=Tool::get_stmt_string(stmt);
                        
                        if(PosShfOverflow(stmt_string))
                        {
                            if(ori_stmt->getBeginLoc().isMacroID()){
                                //P.S.宏的处理是最让人头疼的部分...
                                // auto expansionRange = SM.getExpansionRange(insert_loc);
                                // auto beginFileLoc = SM.getSpellingLoc(expansionRange.getBegin());
                                // auto endFileLoc   = SM.getSpellingLoc(expansionRange.getEnd());
                                // clang::LangOptions LangOpts = _ctx->getLangOpts();

                                // // 获取源码文本
                                // llvm::StringRef macroText = clang::Lexer::getSourceText(
                                //     clang::CharSourceRange::getTokenRange(beginFileLoc, endFileLoc),
                                //     SM, LangOpts
                                // );

                                // if (macroText.startswith("do") && (macroText.trim().endswith("while (0)"))|| (macroText.trim().endswith("while(0)"))) {
                                //     DO_WHILE_MACRO = true;
                                // }
                                continue;
                            }

                            ;//std::cout<<"stmt:"<<stmt_string<<std::endl;
                            std::string type = stmt->getStmtClassName();
                            clang::SourceLocation defHead = SM.getExpansionLoc(func_decl->getBeginLoc());
                            
                            if(NeedLoopChildren(type))
                                LoopChildren(stmt,bop,_rewriter,count,SM,type,stmt_string,"shf",defHead);
                            else
                                JudgeAndInsert(stmt,bop,_rewriter,count,SM,stmt->getStmtClassName(),"shf",defHead);     
                            
                        }
                        else continue;
                    }
                }
                else{
                    for (auto &stmt : body_stmt->children()){
                        //定位到含有/*UBFUZZ*/的那行Stmt
                        ;//std::cout<<"checkline:"<<SM.getSpellingLineNumber(stmt->getEndLoc());
                        
                        if(SM.getSpellingLineNumber(stmt->getEndLoc()) >= UBFUZZ_line && !fir){
                            auto ori_stmt = stmt;
                            clang::SourceLocation insert_loc=SM.getExpansionLoc(ori_stmt->getBeginLoc());

                            fir=true;
                            
                            std::string type=stmt->getStmtClassName();
                            
                            GetSubExpr(stmt,ori_stmt,type,fir,SM,UBFUZZ_line);
                            
                            // 判断二元运算
                            if(const clang::BinaryOperator *bop = llvm::dyn_cast<clang::BinaryOperator>(stmt)){
                                clang::BinaryOperator::Opcode op = bop->getOpcode();
                                //运算符，如op==clang::BinaryOperator::Opcode::BO_Comma 可以判断运算符是不是逗号
                                clang::Expr *lhs = bop->getLHS(); clang::Expr *rhs = bop->getRHS();

                                std::string* insertStr=new std::string;
                                *insertStr="";
                                    
                                if(getFlag(DIV_BIT)){
                                    //for '&& situation', print the check_code
                                    if(getFlag(OPT_BIT)){
                                        ;//std::cout<<"mode: print"<<std::endl;

                                        clang::SourceLocation loc=ori_stmt->getBeginLoc();
                                        judgePrint(bop,SM,UBFUZZ_column,loc);
                                            
                                        *insertStr=" fprintf(stderr, \"ACT_CHECK_CODE\") &&";
                                        if(loc!=ori_stmt->getBeginLoc())
                                            _rewriter.InsertTextBefore(loc,*insertStr);
                                        else
                                            _rewriter.InsertTextBefore(ori_stmt->getBeginLoc(),"fprintf(stderr, \"ACT_CHECK_CODE\");\n");
                                    }
                                    else{
                                        ;//std::cout<<"mode: div"<<std::endl;

                                        std::string stmt_string=Tool::get_stmt_string(stmt);
                                        if(PosDivideZero(stmt_string))
                                            judgeDiv(bop,insertStr,count,SM,UBFUZZ_column,insert_loc);
                                        
                                        //insertStr->pop_back(); //delete the last character
                                        *insertStr += "/*A_QUITE_UNIQUE_FLAG*/\n";
                                        _rewriter.InsertTextBefore(insert_loc,*insertStr);
                                    }   
                                }
                                else{
                                    ;//std::cout<<"mode: shift"<<std::endl;

                                    std::string stmt_string=Tool::get_stmt_string(stmt);

                                    ;//std::cout<<stmt_string<<"-----"<<std::endl;

                                    //UBFUZZ_column=stmt_string.find("+ (MUT_VAR)");

                                    
                                    judgeShf(bop,nullptr,insertStr,SM,bits);

                                    _rewriter.InsertTextBefore(SM.getExpansionLoc(ori_stmt->getBeginLoc()),*insertStr);

                                }
                                delete insertStr;
                            }
                            //    if(lhs==nullptr || rhs==nullptr) return false;//防止空指针
                            //   std::string lhs_class = lhs->getStmtClassName(),rhs_class = rhs->getStmtClassName();//需要考虑运算符左右子语句可能也是二元运算。所以可能需要设计递归函数
                        }
                    }
                }
                
                ;//std::cout<<"count:"<<count<<std::endl;

                clang::SourceLocation InitLoc = SM.getExpansionLoc(func_decl->getSourceRange().getBegin());

                if(SM.isWrittenInMainFile(InitLoc)){
                    if(getFlag(DIV_BIT))
                        _rewriter.InsertTextBefore(InitLoc,initSani(last_count,count,SM));
                    else if(!getFlag(MUT_BIT))
                        _rewriter.InsertTextBefore(InitLoc,initSani_shf(count,bits,SM));
                }
                else
                    ;//std::cout<<"[[Fatal Error]]: Not written in main file"<<std::endl;
            
                last_count=count;

                if(getFlag(DIV_BIT))
                    if(!VisitedFiles.count(FE)){
                        find_stdlib(filePath,_rewriter,fid);   
                        VisitedFiles.insert(FE);
                    }
                        
            }
        }
       ;//std::cout<<"[FunctionDecl]"<<func_decl->getNameAsString()<<" finished."<<std::endl<<std::endl;
        return true;
    }
    bool SeqASTVisitor::VisitVarDecl(clang::VarDecl *var_decl){
        if(!var_decl) return false;

        if(!getFlag(NULL_BIT))
            return true;

        if(var_decl->hasGlobalStorage()){// && var_decl->isFileVarDecl()

            ;//std::cout<<"[global var]:"<<var_decl->getNameAsString()<<std::endl;

            clang::SourceManager &SM = _ctx->getSourceManager();
            clang::SourceLocation insertLoc = SM.getExpansionLoc(var_decl->getBeginLoc());
            clang::SourceLocation DefHead = SM.getExpansionLoc(var_decl->getBeginLoc());

            int cur_count = count;

            //clang::Expr* expr=var_decl->getInit();
            //std::string type = expr->getStmtClassName();
                        
                // if(type == "BinaryOperator" || type == "CallExpr"){
                //     JudgeAndPtrTrack(expr->getExprStmt(), cur_count, SM, DefHead, insertLoc);
                // }
                        
            global_w_NameList.insert(var_decl->getNameAsString());
            ;//std::cout<<"[indeed global var inserted]"<<var_decl->getNameAsString()<<std::endl;
            

            //count++;
            return true;
        }

        return true;
    }
    bool SeqASTVisitor::VisitEnumDecl(clang::EnumDecl *enum_decl){
        if(!enum_decl) return false;

        if(!getFlag(NULL_BIT))
            return true;

        for (auto *EnumConst : enum_decl->enumerators()) {
            ;//std::cout << "[Enumerator]:" << EnumConst->getNameAsString()<<std::endl;
            global_w_NameList.insert(EnumConst->getNameAsString());
            ;//std::cout<<"[indeed global var inserted]"<<EnumConst->getNameAsString()<<std::endl;

            //ShowGlobalNameList();
            // 获取初始化值（如果是整型）
        }
        return true;
    }

    #pragma endregion
    
};

#pragma once
#include "common.h"

/*******************************************************************************
                                   语法分析器
*******************************************************************************/
#define SYNERROR(code, t) Error::synError(code, t)

/*
        错误修复
*/
#define _(T) || look->tag == T
#define F(C) look->tag == C

#define TYPE_FIRST F(KW_INT) _(KW_CHAR) _(KW_VOID)
//表达式
#define EXPR_FIRST \
    F(LPAREN)      \
    _(NUM) _(CH) _(STR) _(ID) _(NOT) _(SUB) _(LEA) _(MUL) _(INC) _(DEC)
//左值运算
#define LVAL_OPR                                                               \
    F(ASSIGN)                                                                  \
    _(OR)                                                                      \
    _(AND)                                                                     \
    _(GT)                                                                      \
    _(GE) _(LT) _(LE) _(EQU) _(NEQU) _(ADD) _(SUB) _(MUL) _(DIV) _(MOD) _(INC) \
        _(DEC)
//右值运算
#define RVAL_OPR \
    F(OR)        \
    _(AND)       \
    _(GT) _(GE) _(LT) _(LE) _(EQU) _(NEQU) _(ADD) _(SUB) _(MUL) _(DIV) _(MOD)
//语句
#define STATEMENT_FIRST                                             \
    (EXPR_FIRST) _(SEMICON) _(KW_WHILE) _(KW_FOR) _(KW_DO) _(KW_IF) \
        _(KW_SWITCH) _(KW_RETURN) _(KW_BREAK) _(KW_CONTINUE)

class Parser {
    //文法开始
    void program() {
        if (F(END)) {  //分析结束

            return;
        } else {
            segment();
            program();
        }
    }

    void segment() {
        bool ext = match(KW_EXTERN);  //记录声明属性
        Tag t = type();
        def(ext, t);
    }

    Tag type() {
        Tag tmp = KW_INT;  //默认类型
        if (TYPE_FIRST) {
            tmp = look->tag;  //记录类型
            move();           //移进
        } else                //报错
            recovery(F(ID) _(MUL), TYPE_LOST, TYPE_WRONG);
        return tmp;  //记录类型
    }

    //声明与定义
    Var* defdata(bool ext, Tag t) {
        string name = "";
        if (F(ID)) {
            name = (((Id*)look)->name);
            move();
            return varrdef(ext, t, false, name);
        } else if (match(MUL)) {
            if (F(ID)) {
                name = (((Id*)look)->name);
                move();
            } else
                recovery(F(SEMICON) _(COMMA) _(ASSIGN), ID_LOST, ID_WRONG);
            return init(ext, t, true, name);
        } else {
            recovery(F(SEMICON) _(COMMA) _(ASSIGN) _(LBRACK), ID_LOST,
                     ID_WRONG);
            return varrdef(ext, t, false, name);
        }
    }

    void deflist(bool ext, Tag t) {
        if (match(COMMA)) {  //下一个声明
            symtab.addVar(defdata(ext, t));
            deflist(ext, t);
        } else if (match(SEMICON))  //最后一个声明
            return;
        else {                   //出错了
            if (F(ID) _(MUL)) {  //逗号
                recovery(1, COMMA_LOST, COMMA_WRONG);
                symtab.addVar(defdata(ext, t));
                deflist(ext, t);
            } else
                recovery(
                    TYPE_FIRST || STATEMENT_FIRST || F(KW_EXTERN) _(RBRACE),
                    SEMICON_LOST, SEMICON_WRONG);
        }
    }

    Var* varrdef(bool ext, Tag t, bool ptr, string name) {
        if (match(LBRACK)) {
            int len = 0;
            if (F(NUM)) {
                len = ((Num*)look)->val;
                move();
            } else
                recovery(F(RBRACK), NUM_LOST, NUM_WRONG);
            if (!match(RBRACK))
                recovery(F(COMMA) _(SEMICON), RBRACK_LOST, RBRACK_WRONG);
            return new Var(symtab.getScopePath(), ext, t, name,
                           len);  //新的数组
        } else
            return init(ext, t, ptr, name);
    }

    Var* init(bool ext, Tag t, bool ptr, string name) {
        Var* initVal = NULL;
        if (match(ASSIGN)) {
            initVal = expr();
        }
        return new Var(symtab.getScopePath(), ext, t, ptr, name,
                       initVal);  //新的变量或者指针
    }

    void def(bool ext, Tag t) {
        string name = "";
        if (match(MUL)) {  //指针
            if (F(ID)) {
                name = (((Id*)look)->name);
                move();
            } else
                recovery(F(SEMICON) _(COMMA) _(ASSIGN), ID_LOST, ID_WRONG);
            symtab.addVar(init(ext, t, true, name));  //新建一个指针变量
            deflist(ext, t);
        } else {
            if (F(ID)) {  //变量 数组 函数
                name = (((Id*)look)->name);
                move();
            } else
                recovery(F(SEMICON) _(COMMA) _(ASSIGN) _(LPAREN) _(LBRACK),
                         ID_LOST, ID_WRONG);
            idtail(ext, t, false, name);
        }
    }

    void idtail(bool ext, Tag t, bool ptr, string name) {
        if (match(LPAREN)) {  //函数
            symtab.enter();
            vector<Var*> paraList;  //参数列表
            para(paraList);
            if (!match(RPAREN))
                recovery(F(LBRACK) _(SEMICON), RPAREN_LOST, RPAREN_WRONG);
            Fun* fun = new Fun(ext, t, name, paraList);
            funtail(fun);
            symtab.leave();
        } else {
            symtab.addVar(varrdef(ext, t, false, name));
            deflist(ext, t);
        }
    }

    //函数
    Var* paradatatail(Tag t, string name) {
        if (match(LBRACK)) {
            int len = 1;  //参数数组忽略长度
            if (F(NUM)) {
                len = ((Num*)look)->val;
                move();
            }  //可以没有指定长度
            if (!match(RBRACK))
                recovery(F(COMMA) _(RPAREN), RBRACK_LOST, RBRACK_WRONG);
            return new Var(symtab.getScopePath(), false, t, name, len);
        }
        return new Var(symtab.getScopePath(), false, t, false, name);
    }

    Var* paradata(Tag t) {
        string name = "";
        if (match(MUL)) {
            if (F(ID)) {
                name = ((Id*)look)->name;
                move();
            } else
                recovery(F(COMMA) _(RPAREN), ID_LOST, ID_WRONG);
            return new Var(symtab.getScopePath(), false, t, true, name);
        } else if (F(ID)) {
            name = ((Id*)look)->name;
            move();
            return paradatatail(t, name);
        } else {
            recovery(F(COMMA) _(RPAREN) _(LBRACK), ID_LOST, ID_WRONG);
            return new Var(symtab.getScopePath(), false, t, false, name);
        }
    }

    void para(vector<Var*>& list) {
        if (F(RPAREN)) return;
        Tag t = type();
        Var* v = paradata(t);
        symtab.addVar(v);  //保存参数到符号表
        list.push_back(v);
        paralist(list);
    }

    void paralist(vector<Var*>& list) {
        if (match(COMMA)) {  //下一个参数
            Tag t = type();
            Var* v = paradata(t);
            symtab.addVar(v);
            list.push_back(v);
            paralist(list);
        }
    }

    void funtail(Fun* f) {
        if (match(SEMICON)) {  //函数声明
            symtab.decFun(f);
        } else {  //函数定义
            symtab.defFun(f);
            block();
            symtab.endDefFun();  //结束函数定义
        }
    }

    void block() {
        if (!match(LBRACE))
            recovery(TYPE_FIRST || STATEMENT_FIRST || F(RBRACE), LBRACE_LOST,
                     LBRACE_WRONG);
        subprogram();
        if (!match(RBRACE))
            recovery(TYPE_FIRST || STATEMENT_FIRST ||
                         F(KW_EXTERN) _(KW_ELSE) _(KW_CASE) _(KW_DEFAULT),
                     RBRACE_LOST, RBRACE_WRONG);
    }

    void subprogram() {
        if (TYPE_FIRST) {  //局部变量
            localdef();
            subprogram();
        } else if (STATEMENT_FIRST) {  //语句
            statement();
            subprogram();
        }
    }

    void localdef() {
        Tag t = type();
        symtab.addVar(defdata(false, t));
        deflist(false, t);
    }

    //语句
    void statement() {
        switch (look->tag) {
            case KW_WHILE:
                whilestat();
                break;
            case KW_FOR:
                forstat();
                break;
            case KW_DO:
                dowhilestat();
                break;
            case KW_IF:
                ifstat();
                break;
            case KW_SWITCH:
                switchstat();
                break;
            case KW_BREAK:
                ir.genBreak();  //产生break语句
                move();
                if (!match(SEMICON))
                    recovery(TYPE_FIRST || STATEMENT_FIRST || F(RBRACE),
                             SEMICON_LOST, SEMICON_WRONG);
                break;
            case KW_CONTINUE:
                ir.genContinue();  //产生continue语句
                move();
                if (!match(SEMICON))
                    recovery(TYPE_FIRST || STATEMENT_FIRST || F(RBRACE),
                             SEMICON_LOST, SEMICON_WRONG);
                break;
            case KW_RETURN:
                move();
                ir.genReturn(altexpr());  //产生return语句
                if (!match(SEMICON))
                    recovery(TYPE_FIRST || STATEMENT_FIRST || F(RBRACE),
                             SEMICON_LOST, SEMICON_WRONG);
                break;
            default:
                altexpr();
                if (!match(SEMICON))
                    recovery(TYPE_FIRST || STATEMENT_FIRST || F(RBRACE),
                             SEMICON_LOST, SEMICON_WRONG);
        }
    }

    void whilestat() {
        symtab.enter();
        InterInst *_while, *_exit;       //标签
        ir.genWhileHead(_while, _exit);  // while循环头部
        match(KW_WHILE);
        if (!match(LPAREN))
            recovery(EXPR_FIRST || F(RPAREN), LPAREN_LOST, LPAREN_WRONG);
        Var* cond = altexpr();
        ir.genWhileCond(cond, _exit);  // while条件
        if (!match(RPAREN)) recovery(F(LBRACE), RPAREN_LOST, RPAREN_WRONG);
        if (F(LBRACE))
            block();
        else
            statement();
        ir.genWhileTail(_while, _exit);  // while尾部
        symtab.leave();
    }

    void dowhilestat() {
        symtab.enter();
        InterInst *_do, *_exit;         //标签
        ir.genDoWhileHead(_do, _exit);  // do-while头部
        match(KW_DO);
        if (F(LBRACE))
            block();
        else
            statement();
        if (!match(KW_WHILE)) recovery(F(LPAREN), WHILE_LOST, WHILE_WRONG);
        if (!match(LPAREN))
            recovery(EXPR_FIRST || F(RPAREN), LPAREN_LOST, LPAREN_WRONG);
        symtab.leave();
        Var* cond = altexpr();
        if (!match(RPAREN)) recovery(F(SEMICON), RPAREN_LOST, RPAREN_WRONG);
        if (!match(SEMICON))
            recovery(TYPE_FIRST || STATEMENT_FIRST || F(RBRACE), SEMICON_LOST,
                     SEMICON_WRONG);
        ir.genDoWhileTail(cond, _do, _exit);  // do-while尾部
    }

    void forstat() {
        symtab.enter();
        InterInst *_for, *_exit, *_step, *_block;  //标签
        match(KW_FOR);
        if (!match(LPAREN))
            recovery(TYPE_FIRST || EXPR_FIRST || F(SEMICON), LPAREN_LOST,
                     LPAREN_WRONG);
        forinit();                                       //初始语句
        ir.genForHead(_for, _exit);                      // for循环头部
        Var* cond = altexpr();                           //循环条件
        ir.genForCondBegin(cond, _step, _block, _exit);  // for循环条件开始部分
        if (!match(SEMICON)) recovery(EXPR_FIRST, SEMICON_LOST, SEMICON_WRONG);
        altexpr();
        if (!match(RPAREN)) recovery(F(LBRACE), RPAREN_LOST, RPAREN_WRONG);
        ir.genForCondEnd(_for, _block);  // for循环条件结束部分
        if (F(LBRACE))
            block();
        else
            statement();
        ir.genForTail(_step, _exit);  // for循环尾部
        symtab.leave();
    }

    void forinit() {
        if (TYPE_FIRST)
            localdef();
        else {
            altexpr();
            if (!match(SEMICON))
                recovery(EXPR_FIRST, SEMICON_LOST, SEMICON_WRONG);
        }
    }

    void ifstat() {
        symtab.enter();
        InterInst *_else, *_exit;  //标签
        match(KW_IF);
        if (!match(LPAREN)) recovery(EXPR_FIRST, LPAREN_LOST, LPAREN_WRONG);
        Var* cond = expr();
        ir.genIfHead(cond, _else);  // if头部
        if (!match(RPAREN)) recovery(F(LBRACE), RPAREN_LOST, RPAREN_WRONG);
        if (F(LBRACE))
            block();
        else
            statement();
        symtab.leave();

        ir.genElseHead(_else, _exit);  // else头部
        if (F(KW_ELSE)) {              //有else
            elsestat();
        }
        ir.genElseTail(_exit);  // else尾部
        //不对if-else的else部分优化，妨碍冗余删除算法的效果
        // if(F(KW_ELSE)){//有else
        // 	ir.genElseHead(_else,_exit);//else头部
        // 	elsestat();
        // 	ir.genElseTail(_exit);//else尾部
        // }
        // else{//无else
        // 	ir.genIfTail(_else);
        // }
    }

    void elsestat() {
        if (match(KW_ELSE)) {
            symtab.enter();
            if (F(LBRACE))
                block();
            else
                statement();
            symtab.leave();
        }
    }

    void switchstat() {
        symtab.enter();
        InterInst* _exit;         //标签
        ir.genSwitchHead(_exit);  // switch头部
        match(KW_SWITCH);
        if (!match(LPAREN)) recovery(EXPR_FIRST, LPAREN_LOST, LPAREN_WRONG);
        Var* cond = expr();
        if (cond->isRef())
            cond = ir.genAssign(cond);  // switch(*p),switch(a[0])
        if (!match(RPAREN)) recovery(F(LBRACE), RPAREN_LOST, RPAREN_WRONG);
        if (!match(LBRACE))
            recovery(F(KW_CASE) _(KW_DEFAULT), LBRACE_LOST, LBRACE_WRONG);
        casestat(cond);
        if (!match(RBRACE))
            recovery(TYPE_FIRST || STATEMENT_FIRST, RBRACE_LOST, RBRACE_WRONG);
        ir.genSwitchTail(_exit);  // switch尾部
        symtab.leave();
    }

    void casestat(Var* cond) {
        if (match(KW_CASE)) {
            InterInst* _case_exit;  //标签
            Var* lb = caselabel();
            ir.genCaseHead(cond, lb, _case_exit);  // case头部
            if (!match(COLON))
                recovery(TYPE_FIRST || STATEMENT_FIRST, COLON_LOST,
                         COLON_WRONG);
            symtab.enter();
            subprogram();
            symtab.leave();
            ir.genCaseTail(_case_exit);  // case尾部
            casestat(cond);
        } else if (match(KW_DEFAULT)) {  // default默认执行
            if (!match(COLON))
                recovery(TYPE_FIRST || STATEMENT_FIRST, COLON_LOST,
                         COLON_WRONG);
            symtab.enter();
            subprogram();
            symtab.leave();
        }
    }

    Var* caselabel() { return literal(); }

    //表达式
    Var* altexpr() {
        if (EXPR_FIRST) return expr();
        return Var::getVoid();  //返回特殊VOID变量
    }

    Var* expr() { return assexpr(); }

    Var* assexpr() {
        Var* lval = orexpr();
        return asstail(lval);
    }

    Var* asstail(Var* lval) {
        if (match(ASSIGN)) {
            Var* rval = assexpr();
            Var* result = ir.genTwoOp(lval, ASSIGN, rval);
            return asstail(result);
        }
        return lval;
    }

    Var* orexpr() {
        Var* lval = andexpr();
        return ortail(lval);
    }

    Var* ortail(Var* lval) {
        if (match(OR)) {
            Var* rval = andexpr();
            Var* result = ir.genTwoOp(lval, OR, rval);
            return ortail(result);
        }
        return lval;
    }

    Var* andexpr() {
        Var* lval = cmpexpr();
        return andtail(lval);
    }

    Var* andtail(Var* lval) {
        if (match(AND)) {
            Var* rval = cmpexpr();
            Var* result = ir.genTwoOp(lval, AND, rval);
            return andtail(result);
        }
        return lval;
    }

    Var* cmpexpr() {
        Var* lval = aloexpr();
        return cmptail(lval);
    }

    Var* cmptail(Var* lval) {
        if (F(GT) _(GE) _(LT) _(LE) _(EQU) _(NEQU)) {
            Tag opt = cmps();
            Var* rval = aloexpr();
            Var* result = ir.genTwoOp(lval, opt, rval);
            return cmptail(result);
        }
        return lval;
    }

    Tag cmps() {
        Tag opt = look->tag;
        move();
        return opt;
    }

    Var* aloexpr() {
        Var* lval = item();
        return alotail(lval);
    }

    Var* alotail(Var* lval) {
        if (F(ADD) _(SUB)) {
            Tag opt = adds();
            Var* rval = item();
            Var* result = ir.genTwoOp(lval, opt, rval);  //双目运算
            return alotail(result);
        }
        return lval;
    }

    Tag adds() {
        Tag opt = look->tag;
        move();
        return opt;
    }

    Var* item() {
        Var* lval = factor();
        return itemtail(lval);
    }

    Var* itemtail(Var* lval) {
        if (F(MUL) _(DIV) _(MOD)) {
            Tag opt = muls();
            Var* rval = factor();
            Var* result = ir.genTwoOp(lval, opt, rval);  //双目运算
            return itemtail(result);
        }
        return lval;
    }

    Tag muls() {
        Tag opt = look->tag;
        move();
        return opt;
    }

    Var* factor() {
        if (F(NOT) _(SUB) _(LEA) _(MUL) _(INC) _(DEC)) {
            Tag opt = lop();
            Var* v = factor();
            return ir.genOneOpLeft(opt, v);  //单目左操作
        } else
            return val();
    }

    Tag lop() {
        Tag opt = look->tag;
        move();
        return opt;
    }

    Var* val() {
        Var* v = elem();
        if (F(INC) _(DEC)) {
            Tag opt = rop();
            v = ir.genOneOpRight(v, opt);
        }
        return v;
    }

    Tag rop() {
        Tag opt = look->tag;
        move();
        return opt;
    }

    Var* elem() {
        Var* v = NULL;
        if (F(ID)) {  //变量，数组，函数调用
            string name = ((Id*)look)->name;
            move();
            v = idexpr(name);
        } else if (match(LPAREN)) {  //括号表达式
            v = expr();
            if (!match(RPAREN)) {
                recovery(LVAL_OPR, RPAREN_LOST, RPAREN_WRONG);
            }
        } else  //常量
            v = literal();
        return v;
    }

    Var* literal() {
        Var* v = NULL;
        if (F(NUM) _(STR) _(CH)) {
            v = new Var(look);
            if (F(STR))
                symtab.addStr(v);  //字符串常量记录
            else
                symtab.addVar(v);  //其他常量也记录到符号表
            move();
        } else
            recovery(RVAL_OPR, LITERAL_LOST, LITERAL_WRONG);
        return v;
    }

    Var* idexpr(string name) {
        Var* v = NULL;
        if (match(LBRACK)) {
            Var* index = expr();
            if (!match(RBRACK)) recovery(LVAL_OPR, LBRACK_LOST, LBRACK_WRONG);
            Var* array = symtab.getVar(name);  //获取数组
            v = ir.genArray(array, index);     //产生数组运算表达式
        } else if (match(LPAREN)) {
            vector<Var*> args;
            realarg(args);
            if (!match(RPAREN)) recovery(RVAL_OPR, RPAREN_LOST, RPAREN_WRONG);
            Fun* function = symtab.getFun(name, args);  //获取函数
            v = ir.genCall(function, args);  //产生函数调用代码
        } else{
            v = symtab.getVar(name);  //获取变量
        }
        return v;
    }

    void realarg(vector<Var*>& args) {
        if (EXPR_FIRST) {
            args.push_back(arg());  //压入参数
            arglist(args);
        }
    }

    void arglist(vector<Var*>& args) {
        if (match(COMMA)) {
            args.push_back(arg());
            arglist(args);
        }
    }

    Var* arg() {
        //添加一个实际参数
        return expr();
    }

    //词法分析
    Lexer& lexer;  //词法分析器
    Token* look;   //超前查看的字符

    //符号表
    SymTab& symtab;

    //中间代码生成器
    GenIR& ir;

    //语法分析与错误修复
    void move() {
        look = lexer.tokenize();
        if (Args::showToken)
            printf("%s\n", look->toString().c_str());  //输出词法记号——测试
    }

    bool match(Tag need) {
        if (look->tag == need) {
            move();
            return true;
        } else
            return false;
    }

    void recovery(bool cond, SynError lost, SynError wrong) {
        if (cond) /*在给定的Follow集合内*/
            SYNERROR(lost, look);
        else {
            SYNERROR(wrong, look);
            move();
        }
    }

   public:
    Parser(Lexer& lex, SymTab& tab, GenIR& inter)
        : lexer(lex), symtab(tab), ir(inter) {}

    void analyse() {
        move();
        program();
    }
};

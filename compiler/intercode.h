#pragma once
#include "common.h"
#include "set.h"

/*
        四元式类，定义了中间代码的指令的形式
*/
class InterInst {
   private:
    string label;  //标签
    Operator op;   //操作符
    // union{
    Var *result;        //运算结果
    InterInst *target;  //跳转标号
    //};
    // union{
    Var *arg1;  //参数1
    Fun *fun;   //函数
    //};
    Var *arg2;  //参数2

    bool first;  //是否是首指令
    void init() {
        op = OP_NOP;
        this->result = NULL;
        this->target = NULL;
        this->arg1 = NULL;
        this->fun = NULL;
        this->arg2 = NULL;
        first = false;
        isDead = false;
    }

   public:
    //初始化
    Block *block;  //指令所在的基本块指针

    //数据流信息
    vector<double> inVals;   //常量传播in集合
    vector<double> outVals;  //常量传播out集合
    Set e_use;               //使用的表达式集合
    Set e_kill;              //杀死的表达式集合
    RedundInfo info;         //冗余删除数据流信息
    CopyInfo copyInfo;       //复写传播数据流信息
    LiveInfo liveInfo;       //活跃变量数据流信息
    bool isDead;             //标识指令是否是死代码

    //参数不通过拷贝，而通过push入栈
    //指令在的作用域路径
    // vector<int>path;//该字段为ARG指令准备，ARG的参数并不代表ARG的位置！！！尤其是常量！！！
    // int offset;//参数的栈帧偏移

    //构造
    InterInst(Operator op, Var *rs, Var *arg1, Var *arg2 = NULL) {
        init();
        this->op = op;
        this->result = rs;
        this->arg1 = arg1;
        this->arg2 = arg2;
    }

    InterInst(Operator op, Fun *fun, Var *rs = NULL) {
        init();
        this->op = op;
        this->result = rs;
        this->fun = fun;
        this->arg2 = NULL;
    }

    InterInst(Operator op, Var *arg1 = NULL) {
        init();
        this->op = op;
        this->result = NULL;
        this->arg1 = arg1;
        this->arg2 = NULL;
    }

    InterInst() {
        init();
        label = GenIR::genLb();
    }

    InterInst(Operator op, InterInst *tar, Var *arg1 = NULL, Var *arg2 = NULL) {
        init();
        this->op = op;
        this->target = tar;
        this->arg1 = arg1;
        this->arg2 = arg2;
    }

    void replace(Operator op, Var *rs, Var *arg1, Var *arg2 = NULL) {
        this->op = op;
        this->result = rs;
        this->arg1 = arg1;
        this->arg2 = arg2;
    }

    void replace(Operator op, InterInst *tar, Var *arg1 = NULL,
                 Var *arg2 = NULL) {
        this->op = op;
        this->target = tar;
        this->arg1 = arg1;
        this->arg2 = arg2;
    }

    ~InterInst() {}

    //外部调用接口
    void setFirst() { first = true; }

    bool isJcond() { return op >= OP_JT && op <= OP_JNE; }
    bool isJmp() { return op == OP_JMP || op == OP_RET || op == OP_RETV; }

    bool isFirst() { return first; }

    bool isLb() { return label != ""; }

    bool isDec() { return op == OP_DEC; }
    bool isExpr() {
        return (op >= OP_AS && op <= OP_OR ||
                op == OP_GET);  //&&result->isBase();
    }
    bool unknown() { return op == OP_SET || op == OP_PROC || op == OP_CALL; }

    Operator getOp() { return op; }
    void callToProc() {
        this->result = NULL;  //清除返回值
        this->op = OP_PROC;
    }
    
    InterInst *getTarget();   //获取跳转指令的目标指令
    Var *getResult();         //获取返回值
    Var *getArg1();           //获取第一个参数
    Var *getArg2();           //获取第二个参数
    string getLabel();        //获取标签
    Fun *getFun();            //获取函数对象
    void setArg1(Var *arg1);  //设置第一个参数
    void toString() {
        if (label != "") {
            printf("%s:\n", label.c_str());
            return;
        }
        switch (op) {
            // case OP_NOP:printf("nop");break;
            case OP_DEC:
                printf("dec ");
                arg1->value();
                break;
            case OP_ENTRY:
                printf("entry");
                break;
            case OP_EXIT:
                printf("exit");
                break;
            case OP_AS:
                result->value();
                printf(" = ");
                arg1->value();
                break;
            case OP_ADD:
                result->value();
                printf(" = ");
                arg1->value();
                printf(" + ");
                arg2->value();
                break;
            case OP_SUB:
                result->value();
                printf(" = ");
                arg1->value();
                printf(" - ");
                arg2->value();
                break;
            case OP_MUL:
                result->value();
                printf(" = ");
                arg1->value();
                printf(" * ");
                arg2->value();
                break;
            case OP_DIV:
                result->value();
                printf(" = ");
                arg1->value();
                printf(" / ");
                arg2->value();
                break;
            case OP_MOD:
                result->value();
                printf(" = ");
                arg1->value();
                printf(" %% ");
                arg2->value();
                break;
            case OP_NEG:
                result->value();
                printf(" = ");
                printf("-");
                arg1->value();
                break;
            case OP_GT:
                result->value();
                printf(" = ");
                arg1->value();
                printf(" > ");
                arg2->value();
                break;
            case OP_GE:
                result->value();
                printf(" = ");
                arg1->value();
                printf(" >= ");
                arg2->value();
                break;
            case OP_LT:
                result->value();
                printf(" = ");
                arg1->value();
                printf(" < ");
                arg2->value();
                break;
            case OP_LE:
                result->value();
                printf(" = ");
                arg1->value();
                printf(" <= ");
                arg2->value();
                break;
            case OP_EQU:
                result->value();
                printf(" = ");
                arg1->value();
                printf(" == ");
                arg2->value();
                break;
            case OP_NE:
                result->value();
                printf(" = ");
                arg1->value();
                printf(" != ");
                arg2->value();
                break;
            case OP_NOT:
                result->value();
                printf(" = ");
                printf("!");
                arg1->value();
                break;
            case OP_AND:
                result->value();
                printf(" = ");
                arg1->value();
                printf(" && ");
                arg2->value();
                break;
            case OP_OR:
                result->value();
                printf(" = ");
                arg1->value();
                printf(" || ");
                arg2->value();
                break;
            case OP_JMP:
                printf("goto %s", target->label.c_str());
                break;
            case OP_JT:
                printf("if( ");
                arg1->value();
                printf(" )goto %s", target->label.c_str());
                break;
            case OP_JF:
                printf("if( !");
                arg1->value();
                printf(" )goto %s", target->label.c_str());
                break;
            // case OP_JG:printf("if( ");arg1->value();printf(" >
            // ");arg2->value();printf(" )goto %s",
            // target->label.c_str());break; case OP_JGE:printf("if(
            // ");arg1->value();printf(" >=
            // ");arg2->value();printf(" )goto %s",
            // target->label.c_str());break; case OP_JL:printf("if(
            // ");arg1->value();printf(" <
            // ");arg2->value();printf(" )goto %s",
            // target->label.c_str());break; case OP_JLE:printf("if(
            // ");arg1->value();printf(" <=
            // ");arg2->value();printf(" )goto %s",
            // target->label.c_str());break; case OP_JE:printf("if(
            // ");arg1->value();printf(" ==
            // ");arg2->value();printf(" )goto %s",
            // target->label.c_str());break;
            case OP_JNE:
                printf("if( ");
                arg1->value();
                printf(" != ");
                arg2->value();
                printf(" )goto %s", target->label.c_str());
                break;
            case OP_ARG:
                printf("arg ");
                arg1->value();
                break;
            case OP_PROC:
                printf("%s()", fun->getName().c_str());
                break;
            case OP_CALL:
                result->value();
                printf(" = %s()", fun->getName().c_str());
                break;
            case OP_RET:
                printf("return goto %s", target->label.c_str());
                break;
            case OP_RETV:
                printf("return ");
                arg1->value();
                printf(" goto %s", target->label.c_str());
                break;
            case OP_LEA:
                result->value();
                printf(" = ");
                printf("&");
                arg1->value();
                break;
            case OP_SET:
                printf("*");
                arg1->value();
                printf(" = ");
                result->value();
                break;
            case OP_GET:
                result->value();
                printf(" = ");
                printf("*");
                arg1->value();
                break;
        }
        printf("\n");
    }
};

/*
        中间代码
*/
class InterCode {
    vector<InterInst *> code;

   public:
    //内存管理
    ~InterCode();  //清除内存

    //管理操作
    void addInst(InterInst *inst);  //添加一条中间代码

    //关键操作
    void markFirst();  //标识“首指令”

    //外部调用接口
    void toString();                 //输出指令
    vector<InterInst *> &getCode();  //获取中间代码序列
};

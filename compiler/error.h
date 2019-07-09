#pragma once
#include "common.h"

/*
        错误类
*/
class Error {
    static Scanner *scanner;  //当前使用的扫描器

   public:
    //构造与初始化
    Error(Scanner *sc) {
        scanner = sc;  //扫描器
    }

    static int errorNum;  //错误个数
    static int warnNum;   //警告个数
    //外界接口
    static int getErrorNum() { return errorNum; }
    static int getWarnNum() { return warnNum; }

    //错误接口
    static void lexError(int code) {
        //词法错误信息串
        static const char *lexErrorTable[] = {
            "字符串丢失右引号",       "二进制数没有实体数据",
            "十六进制数没有实体数据", "字符丢失右单引号",
            "不支持空字符",           "错误的或运算符",
            "多行注释没有正常结束",   "词法记号不存在"};
        errorNum++;
        printf("%s<%d行,%d列> 词法错误 : %s.\n", scanner->getFile(),
               scanner->getLine(), scanner->getLine(), lexErrorTable[code]);
    }

    static void synError(int code, Token *t) {
        //语法错误信息串
        static const char *synErrorTable[] = {
            "类型",  "标识符", "数组长度", "常量", "逗号", "分号", "=", "冒号",
            "while", "(",      ")",        "[",    "]",    "{",    "}"};
        errorNum++;
        if (code % 2 == 0)  // lost
            printf("%s<第%d行> 语法错误 : 在 %s 之前丢失 %s .\n",
                   scanner->getFile(), scanner->getLine(),
                   t->toString().c_str(), synErrorTable[code / 2]);
        else  // wrong
            printf("%s<第%d行> 语法错误 : 在 %s 处没有正确匹配 %s .\n",
                   scanner->getFile(), scanner->getLine(),
                   t->toString().c_str(), synErrorTable[code / 2]);
    }

    static void semError(int code, string name = "") {
        //语义错误信息串
        static const char *semErrorTable[] = {
            "变量重定义",  //附加名称信息
            "函数重定义",
            "变量未声明",
            "函数未声明",
            "函数声明与定义不匹配",
            "函数行参实参不匹配",
            "变量声明时不允许初始化",
            "函数定义不能声明extern",
            "数组长度应该是正整数",
            "变量初始化类型错误",
            "全局变量初始化值不是常量",
            "变量不能声明为void类型",  //没有名称信息
            "无效的左值表达式",
            "赋值表达式类型不兼容",
            "表达式运算对象不能是基本类型",
            "表达式运算对象不是基本类型",
            "数组索引运算类型错误",
            "void的函数返回值不能参与表达式运算",
            "break语句不能出现在循环或switch语句之外",
            "continue不能出现在循环之外",
            "return语句和函数返回值类型不匹配"};
        errorNum++;
        printf("%s<第%d行> 语义错误 : %s %s.\n", scanner->getFile(),
               scanner->getLine(), name.c_str(), semErrorTable[code]);
    }

    static void semWarn(int code, string name = "") {
        //语义警告信息串
        static const char *semWarnTable[] = {
            "函数参数列表类型冲突",  //附加名称信息
            "函数返回值类型不精确匹配"};
        warnNum++;
        printf("%s<第%d行> 语义警告 : %s %s.\n", scanner->getFile(),
               scanner->getLine(), name.c_str(), semWarnTable[code]);
    }
};

int Error::errorNum = 0;  //错误数
int Error::warnNum = 0;   //警告错误数
Scanner *Error::scanner = NULL;

//错误级别,可选，用于修饰错误信息头部
#define FATAL "<fatal>:"
#define ERROR "<error>:"
#define WARN "<warn>:"

//调试输出
#ifdef DEBUG
#define PDEBUG(fmt, args...) printf(fmt, ##args)
#else
#define PDEBUG(fmt, args...)
#endif  // DEBUG

void SEMWARN(int code);  //打印语义警告

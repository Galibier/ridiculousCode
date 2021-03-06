#pragma once
#include <map>
#include "common.h"
#include "compiler.h"
#include "error.h"
#include "lexer.h"
#include "token.h"

#define LEXERROR(code) Error::lexError(code)

class Scanner {
    char* fileName;
    FILE* file;

    int lineLen;
    int readPos;
    char lastch;
    static const int BUFLEN = 80;
    char line[BUFLEN];

    int lineNum;
    int colNum;

    void showChar(char ch) {
        if (ch == -1)
            printf("EOF");
        else if (ch == '\n')
            printf("\\n");
        else if (ch == '\t')
            printf("\\t");
        else if (ch == ' ')
            printf("<blank>");
        else
            printf("%c", ch);
        printf("\t\t<%d>\n", ch);
    }

   public:
    Scanner(char* name)
        : fileName(name),
          lineLen(0),
          readPos(-1),
          lastch(0),
          lineNum(1),
          colNum(0) {
        file = fopen(name, "r");
        if (file == nullptr) {
            printf(
                FATAL
                "Fail to open file: %s, please check the filename and path\n",
                name);
            Error::errorNum++;
        }
    }

    ~Scanner() {
        if (file != nullptr) {
            PDEBUG(WARN "The files are not all scanned!\n");
            Error::warnNum++;
            fclose(file);
        }
    }

    int scan() {
        if (file == nullptr) return -1;

        if (readPos == lineLen - 1) {
            lineLen = fread(line, 1, BUFLEN, file);
            if (lineLen == 0) {
                lineLen = 1;
                line[0] = -1;
            }
            readPos = -1;
        }

        readPos++;
        char ch = line[readPos];
        if (lastch == '\n') {
            lineNum++;
            colNum = 0;
        }
        if (ch == -1) {
            fclose(file);
            file = NULL;
        } else if (ch != '\n') {
            colNum++;
        }

        lastch = ch;
        if (Args::showChar) showChar(ch);
        return ch;
    }

    char* getFile() { return fileName; }
    int getLine() { return lineNum; }
    int getCol() { return colNum; }
};

class Keywords {
    struct string_hash {
        size_t operator()(const string& str) const {
            return __gnu_cxx::__stl_hash_string(str.c_str());
        }
    };

    map<string, Tag, string_hash> keywords;

   public:
    Keywords() {
        keywords["int"] = KW_INT;
        keywords["char"] = KW_CHAR;
        keywords["void"] = KW_VOID;
        keywords["extern"] = KW_EXTERN;
        keywords["if"] = KW_IF;
        keywords["else"] = KW_ELSE;
        keywords["switch"] = KW_SWITCH;
        keywords["case"] = KW_CASE;
        keywords["default"] = KW_DEFAULT;
        keywords["while"] = KW_WHILE;
        keywords["do"] = KW_DO;
        keywords["for"] = KW_FOR;
        keywords["break"] = KW_BREAK;
        keywords["continue"] = KW_CONTINUE;
        keywords["return"] = KW_RETURN;
    }

    Tag getTag(string name) {
        return keywords.find(name) != keywords.end() ? keywords[name] : ID;
    }
};

class Lexer {
    static Keywords keywords;

    Scanner& scanner;
    char ch;
    Token* token;

    bool scan(char need = 0) {
        ch = scanner.scan();
        if (need) {
            if (ch != need) return false;
            ch = scanner.scan();
            return true;
        }
        return true;
    }

   public:
    Lexer(Scanner& sc) : scanner(sc), token(nullptr), ch(' ') {}

    ~Lexer() {
        if (token != nullptr) {
            delete token;
        }
    }
    Token* tokenize();
};

Token* Lexer::tokenize() {
    for (; ch != -1;) {  //过滤掉无效的词法记号，只保留正常词法记号或者NULL
        Token* t = NULL;  //初始化一个词法记号指针
        while (ch == ' ' || ch == '\n' || ch == '\t')  //忽略空白符
            scan();
        //标识符 关键字
        if (ch >= 'a' && ch <= 'z' || ch >= 'A' && ch <= 'Z' || ch == '_') {
            string name = "";
            do {
                name.push_back(ch);  //记录字符
                scan();
            } while (ch >= 'a' && ch <= 'z' || ch >= 'A' && ch <= 'Z' ||
                     ch == '_' || ch >= '0' && ch <= '9');
            //匹配结束
            Tag tag = keywords.getTag(name);
            if (tag == ID)  //正常的标志符
                t = new Id(name);
            else  //关键字
                t = new Token(tag);
        }
        //字符串
        else if (ch == '"') {
            string str = "";
            while (!scan('"')) {
                if (ch == '\\') {  //转义
                    scan();
                    if (ch == 'n')
                        str.push_back('\n');
                    else if (ch == '\\')
                        str.push_back('\\');
                    else if (ch == 't')
                        str.push_back('\t');
                    else if (ch == '"')
                        str.push_back('"');
                    else if (ch == '0')
                        str.push_back('\0');
                    else if (ch == '\n')
                        ;  //什么也不做，字符串换行
                    else if (ch == -1) {
                        LEXERROR(STR_NO_R_QUTION);
                        t = new Token(ERR);
                        break;
                    } else
                        str.push_back(ch);
                } else if (ch == '\n' || ch == -1) {  //文件结束
                    LEXERROR(STR_NO_R_QUTION);
                    t = new Token(ERR);
                    break;
                } else
                    str.push_back(ch);
            }
            //最终字符串
            if (!t) t = new Str(str);
        }
        //数字
        else if (ch >= '0' && ch <= '9') {
            int val = 0;
            if (ch != '0') {  // 10进制
                do {
                    val = val * 10 + ch - '0';
                    scan();
                } while (ch >= '0' && ch <= '9');
            } else {
                scan();
                if (ch == 'x') {  // 16进制
                    scan();
                    if (ch >= '0' && ch <= '9' || ch >= 'A' && ch <= 'F' ||
                        ch >= 'a' && ch <= 'f') {
                        do {
                            val = val * 16 + ch;
                            if (ch >= '0' && ch <= '9')
                                val -= '0';
                            else if (ch >= 'A' && ch <= 'F')
                                val += 10 - 'A';
                            else if (ch >= 'a' && ch <= 'f')
                                val += 10 - 'a';
                            scan();
                        } while (ch >= '0' && ch <= '9' ||
                                 ch >= 'A' && ch <= 'F' ||
                                 ch >= 'a' && ch <= 'f');
                    } else {
                        LEXERROR(NUM_HEX_TYPE);  // 0x后无数据
                        t = new Token(ERR);
                    }
                } else if (ch == 'b') {  // 2进制
                    scan();
                    if (ch >= '0' && ch <= '1') {
                        do {
                            val = val * 2 + ch - '0';
                            scan();
                        } while (ch >= '0' && ch <= '1');
                    } else {
                        LEXERROR(NUM_BIN_TYPE);  // 0b后无数据
                        t = new Token(ERR);
                    }
                } else if (ch >= '0' && ch <= '7') {  // 8进制
                    do {
                        val = val * 8 + ch - '0';
                        scan();
                    } while (ch >= '0' && ch <= '7');
                }
            }
            //最终数字
            if (!t) t = new Num(val);
        }
        //字符
        else if (ch == '\'') {
            char c;
            scan();            //同字符串
            if (ch == '\\') {  //转义
                scan();
                if (ch == 'n')
                    c = '\n';
                else if (ch == '\\')
                    c = '\\';
                else if (ch == 't')
                    c = '\t';
                else if (ch == '0')
                    c = '\0';
                else if (ch == '\'')
                    c = '\'';
                else if (ch == -1 || ch == '\n') {  //文件结束 换行
                    LEXERROR(CHAR_NO_R_QUTION);
                    t = new Token(ERR);
                } else
                    c = ch;                       //没有转义
            } else if (ch == '\n' || ch == -1) {  //行 文件结束
                LEXERROR(CHAR_NO_R_QUTION);
                t = new Token(ERR);
            } else if (ch == '\'') {  //没有数据
                LEXERROR(CHAR_NO_DATA);
                t = new Token(ERR);
                scan();  //读掉引号
            } else
                c = ch;  //正常字符
            if (!t) {
                if (scan('\'')) {  //匹配右侧引号,读掉引号
                    t = new Char(c);
                } else {
                    LEXERROR(CHAR_NO_R_QUTION);
                    t = new Token(ERR);
                }
            }
        } else {
            switch (ch)  //界符
            {
                case '#':  //忽略行（忽略宏定义）
                    while (ch != '\n' && ch != -1) scan();  //行 文件不结束
                    t = new Token(ERR);
                    break;
                case '+':
                    t = new Token(scan('+') ? INC : ADD);
                    break;
                case '-':
                    t = new Token(scan('-') ? DEC : SUB);
                    break;
                case '*':
                    t = new Token(MUL);
                    scan();
                    break;
                case '/':
                    scan();
                    if (ch == '/') {                            //单行注释
                        while (ch != '\n' && ch != -1) scan();  //行 文件不结束
                        t = new Token(ERR);
                    } else if (ch == '*') {  //多行注释,不允许嵌套注释
                        while (!scan(-1)) {  //一直扫描
                            if (ch == '*') {
                                if (scan('/')) break;
                            }
                        }
                        if (ch == -1)  //没正常结束注释
                            LEXERROR(COMMENT_NO_END);
                        t = new Token(ERR);
                    } else
                        t = new Token(DIV);
                    break;
                case '%':
                    t = new Token(MOD);
                    scan();
                    break;
                case '>':
                    t = new Token(scan('=') ? GE : GT);
                    break;
                case '<':
                    t = new Token(scan('=') ? LE : LT);
                    break;
                case '=':
                    t = new Token(scan('=') ? EQU : ASSIGN);
                    break;
                case '&':
                    t = new Token(scan('&') ? AND : LEA);
                    break;
                case '|':
                    t = new Token(scan('|') ? OR : ERR);
                    if (t->tag == ERR) LEXERROR(OR_NO_PAIR);  //||没有一对
                    break;
                case '!':
                    t = new Token(scan('=') ? NEQU : NOT);
                    break;
                case ',':
                    t = new Token(COMMA);
                    scan();
                    break;
                case ':':
                    t = new Token(COLON);
                    scan();
                    break;
                case ';':
                    t = new Token(SEMICON);
                    scan();
                    break;
                case '(':
                    t = new Token(LPAREN);
                    scan();
                    break;
                case ')':
                    t = new Token(RPAREN);
                    scan();
                    break;
                case '[':
                    t = new Token(LBRACK);
                    scan();
                    break;
                case ']':
                    t = new Token(RBRACK);
                    scan();
                    break;
                case '{':
                    t = new Token(LBRACE);
                    scan();
                    break;
                case '}':
                    t = new Token(RBRACE);
                    scan();
                    break;
                case -1:
                    scan();
                    break;
                default:
                    t = new Token(ERR);  //错误的词法记号
                    LEXERROR(TOKEN_NO_EXIST);
                    scan();
            }
        }
        //词法记号内存管理
        if (token) delete token;
        token = t;                       //强制记录
        if (token && token->tag != ERR)  //有效,直接返回
            return token;
        else
            continue;  //否则一直扫描直到结束
    }
    
    if (token) delete token;
    return token = new Token(END);
}
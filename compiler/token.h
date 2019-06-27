#pragma once
#include "common.h"

const char* tokenName[] = {
    "error",  "end-of-file", "identifier", "int",    "char",     "void",
    "extern", "number",      "character",  "string", "!",        "&",
    "+",      "-",           "*",          "/",      "%",        "++",
    "--",     ">",           ">=",         "<",      "<=",       "==",
    "!=",     "&&",          "||",         "(",      ")",        "[",
    "]",      "{",           "}",          ",",      ":",        ";",
    "=",      "if",          "else",       "switch", "case",     "default",
    "while",  "do",          "for",        "break",  "continue", "return"};

class Token {
   public:
    Tag tag;
    Token(Tag t) : tag(t) {}
    virtual string toString() { return tokenName[tag]; }
    virtual ~Token() {}
};

class Id : public Token {
   public:
    string name;
    Id(string n) : Token(ID), name(n) {}
    virtual string toString() { return Token::toString() + name; }
};

class Str : public Token {
   public:
    string str;
    Str(string s) : Token(STR), str(s) {}
    virtual string toString() {
        return string("[") + Token::toString() + "]:" + str;
    }
};

class Num : public Token {
   public:
    int val;
    Num(int v) : Token(NUM), val(v) {}
    virtual string toString() {
        return string("[") + Token::toString() + "]:" + to_string(val);
    }
};

class Char : public Token {
   public:
    char ch;
    Char(char c) : Token(CH), ch(c) {}
    virtual string toString() {
        return string("[") + Token::toString() + "]:" + ch;
    }
};

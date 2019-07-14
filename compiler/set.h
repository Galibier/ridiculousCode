#pragma once
#include "common.h"

/*
        集合类——使用位图表达集合运算
*/
class Set {
    vector<unsigned int> bmList;  //位图列表：位图长度大于32时

   public:
    int count;  //记录集合元素个数

    //构造与初始化
    Set() { count = 0; }
    
    Set(int size, bool val) { init(size, val); }
    
    void init(int size, bool val) {
        count = size;                         //记录大小
        size = size / 32 + (size % 32 != 0);  //计算需要多少个32位位图
        unsigned int v = val ? 0xffffffff : 0;
        for (int i = 0; i < size; i++) bmList.push_back(v);  //初始化位图
    }

    void p() {
        int num = count;  //计数器
        // if(bmList.size()==0){
        // 	while(num--)printf("- ");
        // }
        for (unsigned int i = 0; i < bmList.size(); ++i) {
            unsigned int n = 0x1;
            while (n) {
                printf("%d ", !!(bmList[i] & n));
                if (!--num) break;
                n <<= 1;
            }
            if (!--num) break;
        }
        fflush(stdout);
    }

    //集合基本运算
    Set operator&(Set val) {
        Set ret(count, 0);
        for (unsigned int i = 0; i < bmList.size(); i++) {
            ret.bmList[i] = bmList[i] & val.bmList[i];
        }
        return ret;
    }

    Set operator|(Set val) {
        Set ret(count, 0);
        for (unsigned int i = 0; i < bmList.size(); i++) {
            ret.bmList[i] = bmList[i] | val.bmList[i];
        }
        return ret;
    }

    Set operator-(Set val) {
        Set ret(count, 0);
        for (unsigned int i = 0; i < bmList.size(); i++) {
            ret.bmList[i] = bmList[i] & ~val.bmList[i];
        }
        return ret;
    }

    Set operator^(Set val) {
        Set ret(count, 0);
        for (unsigned int i = 0; i < bmList.size(); i++) {
            ret.bmList[i] = bmList[i] ^ val.bmList[i];
        }
        return ret;
    }

    Set operator~() {
        Set ret(count, 0);
        for (unsigned int i = 0; i < bmList.size(); i++) {
            ret.bmList[i] = ~bmList[i];
        }
        return ret;
    }

    bool operator==(Set& val) {
        if (count != val.count) return false;
        for (unsigned int i = 0; i < bmList.size(); i++) {
            if (bmList[i] != val.bmList[i]) return false;
        }
        return true;
    }

    bool operator!=(Set& val) {
        if (count != val.count) return true;
        for (unsigned int i = 0; i < bmList.size(); i++) {
            if (bmList[i] != val.bmList[i]) return true;
        }
        return false;
    }

    bool get(int i) {
        if (i < 0 || i >= count) {
            // printf("集合索引失败！\n");
            return false;
        }
        return !!(bmList[i / 32] & (1 << (i % 32)));
    }

    void set(int i) {
        if (i < 0 || i >= count) {
            // printf("集合索引失败！\n");
            return;
        }
        bmList[i / 32] |= (1 << (i % 32));
    }

    void reset(int i) {
        if (i < 0 || i >= count) {
            // printf("集合索引失败！\n");
            return;
        }
        bmList[i / 32] &= ~(1 << (i % 32));
    }

    int max() {
        for (int i = bmList.size() - 1; i >= 0; --i) {
            unsigned int n = 0x80000000;
            int index = 31;
            while (n) {
                if (bmList[i] & n) break;
                --index;
                n >>= 1;
            }
            if (index >= 0) {  //出现1
                return i * 32 + index;
            }
        }
        return -1;
    }

    bool empty() { return bmList.size() == 0; }
};

/*
        数据流传播信息
*/
struct TransInfo {
    Set in;   //输入集合
    Set out;  //输出集合
};

/*
        冗余消除数据流信息
*/
struct RedundInfo {
    TransInfo anticipated;  //被预期执行表达式集合
    TransInfo available;    //可用表达式集合
    TransInfo postponable;  //可后延表达式集合
    TransInfo used;         //被使用表达式集合
    Set earliest;  //最前放置表达式集合：earliest(B)=anticipated(B).in-available(B).in
    Set latest;     //最后放置表达式集合：latest(B)=(earliest(B) |
                    // postponable(B).in) & 	(e_use(B) | ~(&(
                    // earliest(B.succ[i]) | postponable(B.succ[i]).in )))
    TransInfo dom;  //基本块的前驱集合
    int index;      //基本块的索引
};

/*
        复写传播的数据流信息
*/
struct CopyInfo {
    Set in;    //输入集合
    Set out;   //输出集合
    Set gen;   //产生复写表达式集合
    Set kill;  //杀死复写表达式集合
};

/*
        活跃变量的数据流信息
*/
struct LiveInfo {
    Set in;   //输入集合
    Set out;  //输出集合
    Set use;  //使用变量集合——变量的使用先与定值
    Set def;  //定值变量集合——变量的定值先与使用
};
#pragma once
#include "common.h"
#include "set.h"
/*******************************************************************************
                            为变量分配寄存器或者栈帧偏移
*******************************************************************************/

/*
        冲突图节点类型
*/
struct Node {
    Var* var;  //节点对应的变量
    int degree;  //节点的度，跟踪节点边的个数，为图着色算法提供选择依据
    int color;  //节点颜色，与var->regId对应，-1为无效值（无法/不能着色）
    Set exColors;  //节点排斥的颜色，避免相邻节点着同色。其补集中第一个有效元素为color！
    vector<Node*> links;  //节点的连接边，按照有序插入，使用二分查找是否存在已有邻居

    Node(Var* v, Set& E) : var(v), degree(0), color(-1) {
        exColors = E;  //排斥色集合为空
    }

    void addLink(Node* node) {
        vector<Node*>::iterator pos =
            lower_bound(links.begin(), links.end(), node);
        if (pos == links.end() || *pos != node) {  //没有找到，则插入
            links.insert(pos, node);
            degree++;  //度增加
                       // printf("\t%s  ---  %s\n",
            // var->getName().c_str(),node->var->getName().c_str());
        }
    }

    void addExColor(int color) {
        if (degree == -1) return;  //已经着色节点，不再处理！
        exColors.set(color);       //添加排除色
        degree--;                  //度减少
    }

    void paint(Set& colorBox) {
        Set availColors = colorBox - exColors;         //可用的颜色集合
        for (int i = 0; i < availColors.count; i++) {  //遍历所有可用颜色
            if (availColors.get(i)) {                  //还有颜色可用
                color = i;                             //着色！
                var->regId = color;  //记录颜色（寄存器ID）到变量
                degree =
                    -1;  //度置为小于任何一个未处理节点的度值，这样保证不会重复访问——每次选大度节点！
                for (int j = 0; j < links.size(); j++) {  //遍历邻居
                    links[j]->addExColor(color);  //邻居添加排除色
                }
                return;  //着色成功！
            }
        }
        degree = -1;  //已经尝试着色，未成功，标记为处理节点
                      //着色失败！！！
    }
};

/*
        作用域类型
*/
struct Scope {
    struct scope_less {
        bool operator()(Scope* left, Scope* right) {
            return left->id < right->id;  //按照id确定节点大小
        }
    };

    int id;
    int esp;
    vector<Scope*> children;  //子作用域

    Scope(int i, int addr) : id(i), esp(addr), parent(NULL) {}

    ~Scope() {
        for (unsigned int i = 0; i < children.size(); i++) delete children[i];
    }

    Scope* find(int i) {
        //二分查找
        Scope* sc = new Scope(i, esp);  //先创建子作用域，拷贝父esp
        vector<Scope*>::iterator pos =
            lower_bound(children.begin(), children.end(), sc, scope_less());
        if (pos == children.end() || (*pos)->id != i) {  //没有找到，则插入
            children.insert(pos, sc);                    //有序插入
            sc->parent = this;                           //记录父节点
        } else {
            delete sc;  //删除查找对象
            sc = *pos;  //找到了
        }
        return sc;
    }
    //查找i作用域，不存在则产生一个子作用域，id=i，esp继承当前父esp
    //输出信息控制，|--开始位置
    int x;          //列位置，实际未用到
    int y;          //行位置
    Scope* parent;  //记录父亲节点
};

/*
        冲突图类型
*/
class CoGraph {
    struct node_less {
        bool operator()(Node* left, Node* right) {
            return left->degree <=
                   right
                       ->degree;  //按照度确定节点大小,非严格小于，避免多余移动操作
        }
    };

    list<InterInst*> optCode;  //中间代码
    LiveVar* lv;  //活跃变量分析对象，使用该对象提供集合对应的变量信息
    vector<Node*> nodes;  //图节点数组，图着色选择最大度节点时，使用堆排序！
    vector<Var*> varList;  //变量列表，缓存所有需要分配的变量
    Set U;                 //颜色集合全集
    Set E;                 //颜色集合空集
    Scope* scRoot;         //根作用域指针
    Fun* fun;              //函数对象

    Node* pickNode() {
        //图节点度发生变化，重新构建最大堆
        make_heap(nodes.begin(), nodes.end(),
                  node_less());  //从小到大O(NlogN) N-->0
        // for(unsigned int i=0;i<nodes.size();i++){
        // 	printf("\t%s(%d) ",
        // nodes[i]->var->getName().c_str(),nodes[i]->degree);
        // }
        // printf("\n");
        Node* node = nodes.front();  //取最大度节点
        // printf("\t<--%s\n", node->var->getName().c_str());
        return node;
    }
    void regAlloc() {
        Set colorBox = U;                    //颜色集合
        int nodeNum = nodes.size();          //节点个数
        for (int i = 0; i < nodeNum; i++) {  //处理所有节点
            Node* node = pickNode();         //选取未处理的最大度节点
            node->paint(colorBox);           //对该节点着色
        }
    }

    int& getEsp(vector<int>& path) {
        // printTree(scRoot);
        Scope* scope = scRoot;  //当前作用域初始化为全局作用域
        for (unsigned int i = 1; i < path.size(); i++) {  //遍历路径
            scope = scope->find(path[i]);  //向下寻找作用域，没有会自动创建！
        }
        // for (int i = 0; i < path.size(); ++i)
        // {
        // 	printf("/%d",path[i] );
        // }
        return scope->esp;  //返回作用域的偏移的引用
    }

    void stackAlloc() {
        int base = Plat::
            stackBase_protect;  //寄存器分配后需要保护现场，使用保护现场栈帧基址
        //初始化作用域序列
        scRoot = new Scope(0, base);  //全局作用域,栈起始偏移为ebp-base
        int max =
            base;  //记录栈帧的最大深度，初始化为base，防止没有内存分配时最大值出错
        //遍历所有的DEC和ARG指令
        for (list<InterInst*>::iterator i = optCode.begin(); i != optCode.end();
             ++i) {
            InterInst* inst = *i;         //遍历指令
            Operator op = inst->getOp();  //获取操作符
            if (op == OP_DEC) {           //局部变量声明
                Var* arg1 = inst->getArg1();
                if (arg1->regId == -1) {  //没有分配到寄存器，计算栈帧地址
                    int& esp =
                        getEsp(arg1->getPath());  //获取变量对应作用与的esp引用
                    int size = arg1->getSize();
                    size +=
                        (4 - size % 4) % 4;  //按照4字节的大小整数倍分配局部变量
                    esp += size;            //累计当前作用域大小
                    arg1->setOffset(-esp);  //局部变量偏移为负数
                    if (esp > max) max = esp;
                    // printf(" => esp=%d\n",-esp);
                }
            }
            //参数不通过拷贝，而通过push入栈
            // else if(op==OP_ARG){//参数入栈指令
            // 	int&
            // esp=getEsp(inst->path);//获取ARG指令对应作用域,计算出的esp引用
            // 	//按照4字节的大小整数倍分配局部变量
            // 	esp+=4;//累计当前作用域大小，不设置变量偏移，因为arg命令只是占位
            // 	inst->offset=-esp;//设置ARG指令设定的内存地址
            // 	if(esp>max)max=esp;
            // 	//printf(" => esp=%d\n",-esp);
            // }
        }
        //设置函数的最大栈帧深度
        fun->setMaxDep(max);
    }

    void __printTree(Scope* root, int blk, int x, int& y) {
        //记录打印位置
        root->x = x;
        root->y = y;
        //填充不连续的列
        if (root->parent) {                                    //有父节点
            vector<Scope*>& brother = root->parent->children;  //兄弟节点
            vector<Scope*>::iterator pos;                      //位置
            pos = lower_bound(brother.begin(), brother.end(), root,
                              Scope::scope_less());  //查找位置，一定存在
            if (pos != brother.begin()) {            //不是第一个兄弟
                Scope* prev = (*--pos);              //前一个兄弟
                int disp = root->y - prev->y - 1;    //求差值
                printf("\033[s");                    //保存光标位置
                while (disp--) {        //不停上移动光标，输出|
                    printf("\033[1A");  //上移
                    printf("|");        //打印|
                    printf("\033[1D");  //左移回复光标位置
                }
                printf("\033[u");  //恢复光标位置
            }
        }
        printf("|——\033[33m<%d>:%d\033[0m", root->id, root->esp);
        printf("\n");
        x += (blk + 1) * 4;  //计算空的列个数
        for (int i = 0; i < root->children.size(); ++i) {
            ++y;  //同级节点累加行
            int t = blk;
            while (t--) printf("    ");
            printf("    ");
            __printTree(root->children[i], blk + 1, x, y);
        }
    }

    void printTree(Scope* root, bool tree_style = true) {
        if (!tree_style) {
            //括号打印
            printf("( <%d>:%d ", root->id, root->esp);
            for (int i = 0; i < root->children.size(); ++i) {
                printTree(root->children[i], false);
            }
            printf(") ");
        } else {                         //树形打印
            int y = 0;                   //从0行开始
            __printTree(root, 0, 0, y);  //树形打印！
        }
    }

   public:
    //参数列表，局部变量DEC声明创建所有变量的Node。
    //参数变量如果分配后在寄存器内，需要在函数开始处添加load指令
    //全局变量不作寄存器优化！！！
    CoGraph(list<InterInst*>& optCode, vector<Var*>& para, LiveVar* lv,
            Fun* f) {
        fun = f;
        this->optCode = optCode;  //记录代码
        this->lv = lv;            //记录活跃变量分析对象指针
#ifdef REG
        U.init(Plat::regNum, 1);  //初始化颜色集合,全集
        E.init(Plat::regNum, 0);  //初始化颜色集合,空集
#else
        U.init(0, 1);  //初始化颜色集合,全集
        E.init(0, 0);  //初始化颜色集合,空集
#endif

        //函数参数变量
        for (unsigned int i = 0; i < para.size(); ++i)
            varList.push_back(para[i]);  //将参数放入列表
        //所有的局部变量
        for (list<InterInst*>::iterator i = optCode.begin(); i != optCode.end();
             ++i) {
            InterInst* inst = *i;         //遍历指令
            Operator op = inst->getOp();  //获取操作符
            if (op == OP_DEC) {           //局部变量声明
                Var* arg1 = inst->getArg1();
                // if(arg1->getArray())continue;//不能为数组分配寄存器
                varList.push_back(arg1);  //记录变量
            }
            if (op == OP_LEA) {  //标记只能放在内存的变量！
                Var* arg1 = inst->getArg1();
                if (arg1) arg1->inMem = true;  // p=&a, a只能放在内存
            }
        }
        Set& liveE = lv->getE();  //活跃变量分析时的空集合
        //计算当前变量列表想对于活跃变量分析时变量列表的掩码
        Set mask = liveE;  //初始化为空集
        for (unsigned int i = 0; i < varList.size(); i++) {  //遍历当前变量列表
            mask.set(varList[i]->index);  //取出变量在活跃变量分析时的列表索引
        }
        //构建图节点
        for (unsigned int i = 0; i < varList.size(); i++) {
            Node* node;
            if (varList[i]->getArray() ||
                varList[i]->inMem)  //数组或者只能在内存的变量，不着色
                node = new Node(varList[i], U);  //创建数组的节点
            else
                node = new Node(varList[i], E);  //创建变量的节点
            varList[i]->index =
                i;  //修正新的变量列表索引，方便查找变量所在的新的列表位置！
            nodes.push_back(node);  //保存节点
        }
        // printf("varList:");
        // for(unsigned int i=0;i<varList.size();i++)
        // 	printf("%s ",varList[i]->getName().c_str() );
        // printf("\nmask=");mask.p();printf("\n");
        //构建冲突边
        Set buf = liveE;  //缓冲上次处理的live.out集合，减少匹配运算
        //逆序遍历live.out集合
        for (list<InterInst*>::reverse_iterator i = optCode.rbegin();
             i != optCode.rend(); ++i) {
            Set& liveout = (*i)->liveInfo.out;  // live.out集合
            //(*i)->toString();
            if (liveout != buf) {  //新的冲突关系
                buf = liveout;     //缓存
                //根据指令活跃变量集合生成完全图的边，可以通过“逆序遍历缓存集合”的方式减少计算
                // printf("\tliveout=");liveout.p();printf("\n");
                vector<Var*> coVar = lv->getCoVar(liveout & mask);
                ;  //冲突变量序列，先使用掩码过滤
                for (int j = 0; j < (int)coVar.size() - 1;
                     j++) {  // n(n-1)/2个组合
                    for (int k = j + 1; k < coVar.size(); k++) {
                        nodes[coVar[j]->index]->addLink(
                            nodes[coVar[k]->index]);  //添加关系，不会重复添加
                        nodes[coVar[k]->index]->addLink(
                            nodes[coVar[j]->index]);  //相互添加关系
                        // for(unsigned int i=0;i<nodes.size();i++){
                        // 	printf("\t%s(%d) ",
                        // nodes[i]->var->getName().c_str(),nodes[i]->degree);
                        // }printf("\n");
                    }
                }
            }
        }
    }

    ~CoGraph() {
        for (unsigned int i = 0; i < nodes.size(); i++) {
            delete nodes[i];  //清除节点内存
        }
        delete scRoot;  //清除作用域内存
    }

    void alloc() {
        regAlloc();    //寄存器分配
        stackAlloc();  //栈帧地址分配

        ///////////////////////////////变量分配结果
        // printf("寄存器个数=%d\n",Plat::regNum);
        // for(unsigned int i=0;i<varList.size();i++){
        // 	Var* v=varList[i];
        // 	if(v->regId!=-1){//在寄存器
        // 		printf("%s\treg=%d\n",v->getName().c_str(),v->regId);
        // 	}
        // 	else{//在内存
        // 		printf("%s\t[ebp%c%d]\n",v->getName().c_str(),v->getOffset()<0?0:'+',v->getOffset());
        // 	}
        // }
        /////////////////////////////////下边是树打印测试代码
        // printTree(scRoot);
        // int a[][10]={
        // 	{1,2,3,4,5},
        // 	{1,2,3,6,7},
        // 	{1,8},
        // 	{1,9},
        // 	{1,9,10,11},
        // 	{1,9,10,12,13},
        // 	{14,15},
        // 	{16}
        // };
        // for(int i=0;i<sizeof(a)/sizeof(a[0]);i++){
        // 	vector<int> path;
        // 	path.push_back(0);
        // 	for(int j=0;a[i][j];j++)
        // 		path.push_back(a[i][j]);
        // 	getEsp(path);
        // }
        // printTree(scRoot);
    }
};
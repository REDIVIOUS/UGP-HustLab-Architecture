#include "cachelab.h"
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

// 这里定义两个宏，用于mode=1时显示轨道信息（即命令行-v命令）
#define PrintTrace1(M) if(mode == 1) printf(M) //直接输出字符串的情况
#define PrintTrace2(M,m) if(mode == 1) printf(M,m) //输出带变量的形式的情况

// 记录命中、缺失、淘汰的数量
int hit_count = 0;
int miss_count = 0;
int eviction_count = 0;

// 组索引、关联度、组内偏移（二进制位数）
int s = 0;
int E = 0;
int b = 0;

// 是否显示轨道信息mode=1为显示，0为不限时
int mode = 0;

// 定义cache行
typedef struct {
    int valid; //有效位
    int tag; //标记位
    int time; //LRU淘汰标记位（时间戳）
}CacheLine;

CacheLine **cache; //这是我们要操作的cache

// 轨迹文件名字和接收文件输入的缓冲区
char FileName[1000]; //缓存从命令行接收的轨迹文件名
char FileBuffer[1000]; //缓存从轨迹文件中读出的内容

// 给cache分配内存空间
void AllocateCache();

// 释放cache内存空间
void FreeCache();

// 对地址addr处更新cache中的信息（LRU算法的实现）
void UpdateCache(unsigned int addr);

// 显示帮助信息（和./cism-ref中保持一致）
void PrintHelp(char* argv[]);

// 主函数
int main(int argc, char* argv[]){
    // 接收命令行输入模块
    int opt;
    while((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1){
        switch(opt){
            case 'h':
                PrintHelp(argv);
                exit(0);
            case 'v':
                mode = 1;
                break;
            case 's':
                s = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                strcpy(FileName, optarg);
                break;
            default:
                PrintHelp(argv);
        }
    }

    // 初始化cache模块（为cache分配内存空间）
    AllocateCache();

    // 访问轨迹文件模块，按照指令的不同执行更新cache指令
    char op; //装载操作
    unsigned int addr; //装载地址
    int size; //装载大小
    FILE* fp = fopen(FileName, "r");
    if(fp == NULL){
        printf("Can not open the file!\n");
    }
    while(fgets(FileBuffer, 1000, fp)){
        //调试的时候发现FileBuffer中第一个字符有空格，导致%c总是读错了，这里去掉首空格
        FileBuffer[strlen(FileBuffer)-1] = '\0';
        if(FileBuffer[0] == ' '){
            strcpy(FileBuffer, &FileBuffer[1]);
        }
        // 将原来文件中的','换成‘ ’,方便读取size
        for(int i = 0; i < strlen(FileBuffer); i++){
            if(FileBuffer[i] == ','){
                FileBuffer[i] = ' ';
            }
        }
        sscanf(FileBuffer, "%c%xu%d", &op, &addr, &size);
        switch(op){
            case 'L': //数据装载指令
                PrintTrace2("%s",FileBuffer);
                UpdateCache(addr);
                break;
            case 'S': //数据存储指令
                PrintTrace2("%s",FileBuffer);
                UpdateCache(addr);
                break;
            case 'M': //数据修改指令,即数据装载指令接数据存储指令
                PrintTrace2("%s",FileBuffer);
                UpdateCache(addr);
                UpdateCache(addr);
                break;
            default: //根据题意，忽略I(指令装载)指令，其他指令不合法，跳过
                break;
        }
        PrintTrace1("\n");
    }
    fclose(fp);

    // 回收cache内存空间模块，free掉cache对应的内存空间
    FreeCache();

    // 打印结果和标准结果比较
    printSummary(hit_count, miss_count, eviction_count);
    return 0;
}


// 给cache分配空间
void AllocateCache(){
    int S = 1 << s; //由组索引推导出组数
    cache = (CacheLine**)malloc(sizeof(CacheLine*) * S);
    for (int i = 0; i < S; i++){
        cache[i] = (CacheLine*)malloc(sizeof(CacheLine) * E);
        for(int j = 0; j < E; j++){
            cache[i][j].valid = 0;
            cache[i][j].tag = 0;
            cache[i][j].time = 0;
        }
    }
}

// 释放cache占用的内存空间
void FreeCache(){
    int S = 1 << s;
    for(int i = 0; i < S; i++){
        free(cache[i]);
    }
    free(cache);
}

// 更新cache中的信息（LRU算法实现）
void UpdateCache(unsigned int addr){
    // 从地址中剥离出组索引、和标记位
    unsigned int AddrSetIndex = (addr >> b) & ((0x1 << s) - 1); //组索引
    unsigned int AddrTag = addr >> (s + b); //标记位

    int hitflag = 0; //hit的标记
    int i; //第i块cache
    // 首先在cache中寻找一圈是否有命中的情况
    for(i = 0; i < E; i++){
        if(cache[AddrSetIndex][i].valid == 1 && cache[AddrSetIndex][i].tag == AddrTag){
            cache[AddrSetIndex][i].time = 0;
            hit_count++;
            hitflag = 1; //hit标记置1
            PrintTrace1(" hit");
        }
        else{
            cache[AddrSetIndex][i].time += 1;
        }
    }
    // cache中没有命中的情况，进行占空位或者淘汰
    if(hitflag != 1){
        PrintTrace1(" miss");
        miss_count++;
        int MaxTime = cache[AddrSetIndex][0].time; //cache中最大计时次数
        int MaxTimeIndex = 0; //最大计时所对应的块号
        for(i = 0; i < E; i++){
            //如果找到空的cache块，则填入
            if(cache[AddrSetIndex][i].valid == 0){
                cache[AddrSetIndex][i].valid = 1;
                cache[AddrSetIndex][i].tag = AddrTag;
                cache[AddrSetIndex][i].time = 0;
                break;
            }
            // 计算一轮中计时最大的cache块
            if(cache[AddrSetIndex][i].time > MaxTime){
                MaxTimeIndex = i;
                MaxTime = cache[AddrSetIndex][i].time;
            }
        }
        // 如果cache中没有空位的情况，选择一个最大计时来淘汰
        if(i >= E){
            PrintTrace1(" eviction");
            eviction_count++;
            cache[AddrSetIndex][MaxTimeIndex].valid = 1;
            cache[AddrSetIndex][MaxTimeIndex].tag = AddrTag;
            cache[AddrSetIndex][MaxTimeIndex].time = 0;
        }
    }
}

// 根据./csim-ref中help的标准输出，构造以下help输出函数
void PrintHelp(char* argv[]){
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]);
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\n");
    printf("Examples:\n");
    printf("  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n", argv[0]);
    printf("  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n", argv[0]);
}
#ifndef __KILO_H__
#define __KILO_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>

#define TAB_STOP 8

/* 转义序列 */

// 移动光标到下一行开头
char *MOVE_CURSOR_TO_NEXT_LINE = "\x1b[E";
// 清除光标到屏幕底部的所有数据
char *CLEAR_SCREEN = "\x1b[J";
// 清除当前行从光标位置到行尾的内容
char *CLEAR_LINE_REST = "\x1b[K";
// 隐藏光标
char *HIDE_CURSOR = "\x1b[?25l";
// 显示光标
char *SHOW_CURSOR = "\x1b[?25h";
// 定位光标到指定位置，第一个%d，光标在哪一行，第二个%d，光标在哪一列
char *LOCATE_CURSOR = "\x1b[%d;%dH";


/* 把按键对应的转义序列映射为某个数字 */

enum KeyEnum {
    CTRL_C = 3,   // ctrl + c
    CTRL_Q = 17,
    CTRL_S = 19,
    TAB = 9,
    ENTER = 13, // 回车符，\r
    ESC = 27,   // esc
    BACKSPACE = 127,
    ARROW_UP = 1000,
    ARROW_DOWN,
    ARROW_LEFT,
    ARROW_RIGHT,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN,
    DEL_KEY
};

typedef struct cmdKeyMap {
    char *escapeSeq;
    int val;
} CmdKeyMap;

CmdKeyMap CmdKeyMapList[] = {
    {"\x1b[A", ARROW_UP},
    {"\x1b[B", ARROW_DOWN},
    {"\x1b[C", ARROW_RIGHT},
    {"\x1b[D", ARROW_LEFT},
    {"\x1b[3~", DEL_KEY},
    {"\x1b[H", HOME_KEY},
    {"\x1b[F", END_KEY},
    {"\x1b[5~", PAGE_UP},
    {"\x1b[6~", PAGE_DOWN},
    {"\x1bOH", HOME_KEY},
    {"\x1bOF", END_KEY}
};

#define CmdKeyMapListLen ((int) (sizeof(CmdKeyMapList) / sizeof(CmdKeyMap)))


/* 字符串对象，支持动态扩容 */

typedef struct str {
    char *chars;
    int size;
} Str;

#define STR_INIT {NULL, 0}

void strAppend(Str *str, char *s, int len);
void strFree(Str *str);

/* 文件 */

const char LINE_BREAK = '\n';

// 文件中的一行数据
typedef struct erow {
    char *row;    // 文件中的原始数据
    int size;     // 一行数据的大小
    char *rowTab; // 展示在屏幕上的数据，将tab键转换为空格
    int sizeTab;
} Erow;

// 文件中的所有数据
typedef struct econtent {
    char *filename;
    Erow *pErow;
    int size;
    int sumChars;
    char *fileStatus;  // [New File] 
} EContent;

#define ECONTENT_INIT {NULL, NULL, 0, 0, NULL}

EContent F = ECONTENT_INIT;


/* 初始化编辑器 */

// 编辑器的全局属性
struct eConfig {
    // 屏幕行数、列数
    int screenrows, screencols;
    // 光标的横坐标、纵坐标。cx指向原始文本，原始文本中的tab键会被渲染为空格，rx指向渲染后的数据
    int cx, cy, rx, ry;
    // 文本和屏幕之间的行偏移量、列偏移量，当文本长度超过一个屏幕时，使用当前变量控制
    int rowoff, coloff;
    // 展示文本区域的行数
    int textAreaRows;
    // 状态行的行号
    int statusRow;

    // 编辑模式  0 命令行、1 编辑
    int cmdMode;
    // 状态栏数据
    char *statusMsg;
    // 是否刷新屏幕  0 不刷新 1 刷新
    int refreshFlag;
    // 是否退出编辑器，0 不退出，1 退出
    int exitFlag;
    // 是否开启软换行  0 不开启  1 开启
    int softBRFlag;

    char *confFileName;
} E;

// 配置项
char *confKeyArr[] = {"softBRFlag"};

#define confKeyArrLen ((int) (sizeof(confKeyArr) / sizeof( char *)))

int initEditor();
int getWindowSize(int *rows, int *cols);
int parseConfFile(EContent *CONF);


/* 操作文件的函数 */

int readFile(char *filename, EContent *F);
int editorSave(char *filename);

void charInsert(int c);
void stupidInsert(int c);
void updateRowTabByRow(Erow *erow);
// 删除当前光标处的字符
void deleteCurChar();
// 在当前光标处插入一个换行符
void insertEnterCharInCurPos();

void insertRow(EContent *F, int rowIdx, char *buffer, int len);
void deleteRow(EContent *F, int rowIdx);


/* 终端 */

// 终端属性
struct termios origTermios, rawTermios;

int enableRawMode();
void disableRawMode();


/* 屏幕数据 */

char *screen;
int screensize;

void renderTextArea();
void renderStatusLine(char *msg);
void refreshCursor(int row, int col);
void editorScroll();
void editorScrollWithSBR();
void renderRow();
void appendRowToScreen(Erow *erow, Str *str);
int appendRowToScreenSBR(Erow *erow, Str *str);  // 软换行

/* 键盘输入 */

// select函数
fd_set readfds;   // 读事件相关的文件描述符的集合
struct timeval timeout;

int convertCyToRy(int cy);
// int convertRyToCy(Erow *erow, int ry);
int convertCxToRx(Erow *erow, int cx);
int convertRxToCx(Erow *erow, int rx);
int getRowLimit(Erow *erow);
void moveCursor(int key);
int processKeypress();


#endif // !__KILO_H__


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>


int getWindowSizeByInstruction(int *p_rows, int *p_cols) {
    // 1、将光标移动到屏幕右下角
    write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12);

    // 2、获取当前光标位置，系统会把光标位置打印到标准输出，格式是：\x1b[行号;列号R
    write(STDOUT_FILENO, "\x1b[6n", 4);
    printf("\r\n");

    // 3、解析光标位置

    // 从标准输出读取数据
    char buf[16];
    int i = 0;
    while (1) {
        read(STDOUT_FILENO, &buf[i], 1);
        if (buf[i] == 'R') {
            break;
        }
        i++;
    }
    buf[++i] = '\0';

    // 解析从标准输出读取到的数据
    sscanf(buf, "\x1b[%d;%dR", p_rows, p_cols);
    return 1;
}


int getWindowSizeByAPI(int *p_rows, int *p_cols) {
    struct winsize ws;
    // 参数TIOCGWINSZ，表示获取屏幕大小，函数的执行结果会被存储到第三个参数中
    // 如果返回-1，表示执行失败
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    }
    *p_rows = ws.ws_row;
    *p_cols = ws.ws_col;
    return 1;
}


int getWindowSize(int *p_rows, int *p_cols) {
    if (getWindowSizeByAPI(p_rows, p_cols) == -1) {
        return getWindowSizeByInstruction(p_rows, p_cols);
    }
    return 1;
}


int main(int argc, char const *argv[]) {
    int screen_rows = 0;
    int screen_cols = 0;
    getWindowSize(&screen_rows, &screen_cols);
    printf("当前屏幕的行数：%d\n", screen_rows);
    printf("当前屏幕的列数：%d\n", screen_cols);
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>


/*** data ***/

struct termios orig_termios;

/*** terminal ***/

// 错误处理
void die(const char *s);

// 关闭原始模式
void disableRawMode();

// 开启原始模式
void enableRawMode();


void die(const char *s) {
    perror(s);
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
        die("程序退出时设置终端属性错误：");
    }
}

void enableRawMode() {
    // 获取终端的属性
    struct termios raw;
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        die("获取终端属性错误：");
    }
    raw = orig_termios;

    // 在程序退出时还原终端设置
    atexit(disableRawMode);

    // ECHO：回显，表示终端会显示用户的输入。这里关闭了回显，
    // ICANON：经典模式。在这里关闭了行缓冲
    // ISIG：信号模式。在这里关闭了信号模式，程序不再处理Ctrl+C和Ctrl+Z
    // IEXTEN：用于设置或检查终端是否启用了扩展输入处理。程序不再处理ctrl+v
    raw.c_lflag = ~(ECHO | ICANON | ISIG | IEXTEN);

    // IXON：用于设置或检查终端是否启用了软件流控制，Ctrl+S 和 Ctrl+Q
    // 按下Ctrl+S将停止数据传输，按下Ctrl+Q将恢复数据传输
    // ICRNL：设置或检查终端是否将输入的回车符（\r）转换为换行符（\n）
    raw.c_iflag = ~(IXON | ICRNL);

    // OPOST：用于设置或检查终端是否启用了输出处理，换行符（\n）转换为回车换行符（\r\n）
    raw.c_oflag = ~(OPOST);

    // 设置read函数的等待时间
    // 最少0个字节就可以返回
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    // 设置修改后的属性
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("设置终端属性错误：");
    }
}

int main(int argc, char const *argv[]) {
    // 开启原始模式
    enableRawMode();

    // 清空整个屏幕
    write(STDOUT_FILENO, "\x1b[2J", 4);

    int x = 1, y = 1;
    // 将光标移动到屏幕的左上角
    char str[16];
    snprintf(str, 16, "\x1b[%d;%dH", y, x);
    write(STDOUT_FILENO, str, strlen(str));

    // 读取用户的输入，在终端移动光标，只可以在一个屏幕大小的范围内移动
    char c;
    while (1) {
        c = '\0';
        if (read(STDIN_FILENO, &c, 1) == -1) {
            die("read函数错误：");
        }

        char seq[3];
        if (c == '\x1b') {
            read(STDIN_FILENO, &seq[1], 2);
            if (seq[1] == 91 && seq[2] == 65) {
                // 向上箭头
                y--;
            } else if (seq[1] == 91 && seq[2] == 66) {
                // 向下箭头
                y++;
            } else if (seq[1] == 91 && seq[2] == 67) {
                // 向右箭头
                x++;
            } else if (seq[1] == 91 && seq[2] == 68) {
                // 向左箭头
                x--;
            }
            snprintf(str, 16, "\x1b[%d;%dH", y, x);
            write(STDOUT_FILENO, str, strlen(str));
        }

        if (c == 'q') {
            break;
        }
    }

    return 0;
}
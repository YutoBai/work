#include "terminal.h"
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <errno.h>

// 保存原始终端设置，用于程序退出时恢复终端状态
static struct termios orig_termios;

// 启用终端原始模式（raw mode）
void terminal_enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) exit(1);
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);// 输入模式：关闭BREAK、CR->NL、奇偶校验、剥离第八位、软件流控
    
    raw.c_oflag &= ~(OPOST);// 输出模式：关闭所有输出处理（如自动换行）
    
    raw.c_cflag |= (CS8);// 控制模式：设置字符大小为8位

    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);// 本地模式：关闭回显、规范输入、扩展输入、信号字符
    raw.c_cc[VMIN] = 1;// 控制字符：最小输入字节数为1
    raw.c_cc[VTIME] = 0;//超时为0（非阻塞读）

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) exit(1);
}


//关闭原始模式，恢复终端到原始设置
void terminal_disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

//向终端写入字符流 s为字符串指针；len为写入长度
void terminal_write(const char *s, int len) {
    write(STDOUT_FILENO, s, len);
}

//读取一个按键，支持普通字符和方向键、PageUp/Down等功能键，return 按键的ASCII值或定义的常量
int terminal_read_key(void) {
    int nread;
    char c;
    // 循环读取一个字节
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) exit(1);
    }
    // 如果是转义序列（如方向键、功能键），解析它
    if (c == '\x1b') {
        char seq[3];
        // 读第二、三字节
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
        if (seq[0] == '[') {
            // 可能是~结尾的功能键
            if (seq[1] >= '0' && seq[1] <= '9') {
                char seq2;
                if (read(STDIN_FILENO, &seq2, 1) != 1) return '\x1b';
                if (seq2 == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            } else {
                // 方向键和Home/End
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        }
        // 如果不是已知转义，返回ESC本身
        return '\x1b';
    }
    // 普通字符直接返回
    return c;
}

//获取终端窗口的行数和列数:rows 返回行数指针;cols 返回列数指针
void terminal_get_window_size(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {        // ioctl获取窗口大小,获取失败则使用默认值
        *rows = 24;
        *cols = 80;
    } else {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
    }
}
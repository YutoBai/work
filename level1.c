#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

struct termios origin_termios;

// 错误处理函数
void die(const char *s) {
    perror(s);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &origin_termios);  // 恢复终端设置
    exit(1);
}

// 关闭原始模式，恢复终端设置
void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &origin_termios) == -1) {
        perror("tcsetattr");
    }
}

// 启用原始模式,获取当前终端设置
void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &origin_termios) == -1) 
        die("tcgetattr");
    atexit(disableRawMode);// 程序退出时恢复设置

    struct termios raw = origin_termios;
    
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);//禁用各种终端处理特性（BRKINT, INPCK, ISTRIP等），输入标志：关闭 BRKINT, ICRNL, INPCK, ISTRIP, IXON
    
    raw.c_oflag &= ~(OPOST);// 输出标志：关闭 OPOST
  
    raw.c_cflag |= (CS8);  // 控制标志：设置字符大小为8位
   
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); // 禁用回显模式、信号处理，规范模式本地标志：关闭 ECHO, ICANON, IEXTEN, ISIG
    
    raw.c_cc[VMIN] = 1;// 控制字符：最小输入字节数
    raw.c_cc[VTIME] = 0.1; // 超时时间：0.1秒超时

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) 
        die("tcsetattr");
}

int main() {
    enableRawMode();

    printf("进入原始模式，按'q'退出。按键的ASCII值如下：\n");

    while (1) {
        char c = 0;
        ssize_t nread = read(STDIN_FILENO, &c, 1);
        if (nread == -1) die("read");
        if (nread == 1) {
            printf("你按下了字符: '%c' (ASCII: %d)\n", (c >= 32 && c <= 126) ? c : '?', c);
            fflush(stdout); // 强制刷新输出
            if (c == 'q') break;
        }
    }
    return 0;
}
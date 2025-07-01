#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>

/*** 导入level1的原始模式管理代码 ***/
// ...（假设已实现 enableRawMode/disableRawMode/editorReadKey 等）

/*** Level 2：特殊键定义与有限状态机解析 ***/
enum editorKey {
    ARROW_LEFT = 1000, ARROW_RIGHT, ARROW_UP, ARROW_DOWN,
    HOME_KEY, END_KEY, PAGE_UP, PAGE_DOWN, DEL_KEY
};

int editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) exit(1);
    }
    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                char seq2;
                if (read(STDIN_FILENO, &seq2, 1) != 1) return '\x1b';
                if (seq2 == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            } else {
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
        return '\x1b';
    }
    return c;
}

/*** 屏幕尺寸检测 ***/
int getWindowSize(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) return -1;
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
}

/*** append buffer结构体与函数 ***/
struct abuf {
    char *b;
    int len;
};
#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
    char *new_buf = realloc(ab->b, ab->len + len);
    if (!new_buf) return;
    memcpy(&new_buf[ab->len], s, len);
    ab->b = new_buf;
    ab->len += len;
}
void abFree(struct abuf *ab) { free(ab->b); }

/*** 编辑器状态与屏幕渲染 ***/
struct editorConfig {
    int cx, cy;
    int screenrows;
    int screencols;
};
struct editorConfig E;

void editorDrawRows(struct abuf *ab) {
    for (int y = 0; y < E.screenrows; y++) {
        abAppend(ab, "~", 1); // 行首波浪线
        abAppend(ab, "\x1b[K", 3); // 清除行尾
        if (y < E.screenrows - 1) abAppend(ab, "\r\n", 2);
    }
}
void editorRefreshScreen() {
    struct abuf ab = ABUF_INIT;
    abAppend(&ab, "\x1b[?25l", 6); // 隐藏光标
    abAppend(&ab, "\x1b[H", 3);    // 光标左上
    editorDrawRows(&ab);
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buf, strlen(buf));
    abAppend(&ab, "\x1b[?25h", 6); // 显示光标
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** 特殊键处理与主循环 ***/
void editorMoveCursor(int key) {
    switch (key) {
        case ARROW_LEFT:  if (E.cx > 0) E.cx--; break;
        case ARROW_RIGHT: if (E.cx < E.screencols-1) E.cx++; break;
        case ARROW_UP:    if (E.cy > 0) E.cy--; break;
        case ARROW_DOWN:  if (E.cy < E.screenrows-1) E.cy++; break;
    }
}
void editorProcessKeypress() {
    int c = editorReadKey();
    switch (c) {
        case 'q':
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
        case ARROW_UP: case ARROW_DOWN: case ARROW_LEFT: case ARROW_RIGHT:
            editorMoveCursor(c); break;
    }
}

int main() {
    enableRawMode();
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) exit(1);
    E.cx = 0; E.cy = 0;
    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
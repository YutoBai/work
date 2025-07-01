#include "editor.h"
#include "terminal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define EDITOR_MAX_ROWS 1024
#define EDITOR_MAX_COLS 256
#define TAB_WIDTH 4  // Tab显示为4个空格

// 编辑器的全局状态
typedef struct {
    int cx, cy;          // 光标（列，行）
    int rowoff;          // 屏幕顶端对应文本行
    int screenrows;      // 屏幕可显示行数
    int screencols;      // 屏幕可显示列数
    int numrows;         // 文本行数
    char *rows[EDITOR_MAX_ROWS]; // 行内容
    int quit;            // 退出标志
    char filename[256];  // 保存文件名
} EditorState;

static EditorState E;

// 函数声明
static int editor_row_render_x(const char* row, int cx);
static void editor_draw_rows(void);
static void editor_draw_status_bar(void);
static void editor_refresh_screen(void);
static void editor_process_key(int key);
static void editor_move_cursor(int key);
static void editor_open_file(const char *filename);

// 初始化编辑器状态，读取窗口大小
void editor_init(void) {
    memset(&E, 0, sizeof(E));
    terminal_get_window_size(&E.screenrows, &E.screencols);
    E.screenrows -= 3;                  // 未知bug,值设为2时光标显示正常保留一行给状态栏
    E.rowoff = 0;
    for(int i=0; i<EDITOR_MAX_ROWS; ++i) E.rows[i] = NULL;
}

// 打开文件，读取每行到E.rows
void editor_open(const char *filename) {
    if (filename){
        editor_open_file(filename);
        strncpy(E.filename, filename, sizeof(E.filename) - 1);//保存文件名
        E.filename[sizeof(E.filename) - 1] = '\0';
    }
    else {
        E.numrows = 1;
        E.rows[0] = strdup("Hello,World!");
        strcpy(E.filename, "No Name");
    }
    E.cx = E.cy = E.rowoff = 0;
}

// 主循环：刷新屏幕和处理键盘输入
void editor_loop(void) {
    terminal_enable_raw_mode();
    while (!E.quit) {
        editor_refresh_screen();
        int key = terminal_read_key();
        editor_process_key(key);
    }
    terminal_disable_raw_mode();
}

// 退出时释放内存
void editor_quit(void) {
    for (int i = 0; i < E.numrows; ++i)
        free(E.rows[i]);
}

// 打开文件，将每行读入E.rows数组
static void editor_open_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        E.numrows = 1;
        E.rows[0] = strdup("Hello,World!");
        return;
    }
    char buf[EDITOR_MAX_COLS];
    E.numrows = 0;
    while (fgets(buf, sizeof(buf), fp) && E.numrows < EDITOR_MAX_ROWS) {
        buf[strcspn(buf, "\r\n")] = 0; // 去掉换行
        E.rows[E.numrows++] = strdup(buf);
    }
    fclose(fp);
    if (E.numrows == 0) {
        E.numrows = 1;
        E.rows[0] = strdup("Hello,World!");
    }
}

// 绘制正文区域，每行显示文本或占位符
static void editor_draw_rows(void) {
    for (int y = 0; y < E.screenrows; y++) {
        int filerow = y + E.rowoff; //当前屏幕第 y 行实际显示的是文本文件的第 filerow 行
        if (filerow >= E.numrows) {
            terminal_write("~\r\n", 3);//如果 filerow 超出了实际文件的总行数（即该行并没有内容可显示），就画一个 ~，再加回车换行
        } else {                        
            int cx = 0;                 //cx 表示当前渲染到屏幕的列号，用于处理制表符的对齐
            const char* row = E.rows[filerow];
            // 处理制表符，渲染为多个空格(设置的是4)
            for (int i = 0; row[i] != '\0' && cx < E.screencols; ++i) {
                if (row[i] == '\t') {
                    int space = TAB_WIDTH - (cx % TAB_WIDTH);
                    for (int j = 0; j < space && cx < E.screencols; ++j, ++cx) {
                        terminal_write(" ", 1);
                    }
                } else {                        //如果不是 \t，直接输出这个字符，并 cx++
                    terminal_write(&row[i], 1);
                    cx++;
                }
            }
            terminal_write("\r\n", 2);
        }
    }
}

// 绘制底部状态栏（反显）
static void editor_draw_status_bar(void) {
    char status[128];
    char namebuf[21]; // 最多显示20字符+1结尾
    // 判断：如果文件名超长截断，如果没有用"No Name"
    if (E.filename[0] && strcmp(E.filename, "No Name") != 0) {
        strncpy(namebuf, E.filename, 20);
        namebuf[20] = '\0';
    } else {
        strcpy(namebuf, "No Name");
    }

    int len = snprintf(status, sizeof(status),
        "文件: %-20s  行: %d/%d  列：%d/%d  按q退出", namebuf, E.cy + 1, E.numrows , E.cx + 1 , strlen(E.rows[E.cy])+1);
    
    terminal_write("\x1b[7m", 4);  // 反显
    terminal_write(status, len);
    while (len < E.screencols) {
        terminal_write(" ", 1);
        len++;
    }
    terminal_write("\x1b[m", 3);  // 关闭反显
    terminal_write("\r\n", 2);
}

// 绘制状态栏下方的消息栏（预留给搜索/提示/消息）
static void editor_draw_message_bar(void) {
    char msg[128] = "Ctrl-F 搜索..."; // 目前为空，可改为 "Ctrl-F 搜索..."
    int len = snprintf(msg, sizeof(msg), "%s", "Ctrl-F 搜索..."); // 先空着

    terminal_write("\x1b[K", 3); // 清空本行
    terminal_write("\x1b[7m", 4);  // 反显
    terminal_write(msg, len);
    while (len < E.screencols) {
        terminal_write(" ", 1);
        len++;
    }
    terminal_write("\x1b[m", 3);  // 关闭反显
    terminal_write("\r\n", 2);
}

// 计算逻辑光标位置cx在渲染后行的横坐标（因tab宽度不同，第n个字符变成了四个空格）
static int editor_row_render_x(const char* row, int cx) {
    int rx = 0;
    for (int i = 0; i < cx && row[i]; i++) {
        if (row[i] == '\t')
            rx += TAB_WIDTH - (rx % TAB_WIDTH);
        else
            rx++;
    }
    return rx;
}

// 刷新屏幕：清屏、绘制正文和状态栏、定位光标
static void editor_refresh_screen(void) {
    terminal_write("\x1b[2J", 4);   // \x1b 是 ESC 字符（十六进制），[2J 是“清屏”命令
    terminal_write("\x1b[H", 3);    // 光标回左上
    terminal_write("\x1b[?25l", 6); // 隐藏光标

    editor_draw_rows();             // 绘制正文
    editor_draw_status_bar();       // 绘制状态栏
    editor_draw_message_bar();      // 后续实现文本搜索功能（如Ctrl- F搜索）

    // 计算渲染后光标横坐标
    int rx = 0;
    if (E.cy >= 0 && E.cy < E.numrows)
        rx = editor_row_render_x(E.rows[E.cy], E.cx);
    int cx = rx + 1; // ANSI序号从1开始
    int cy = E.cy - E.rowoff + 1;
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", cy, cx);
    terminal_write(buf, strlen(buf));

    terminal_write("\x1b[?25h", 6); // 显示光标
}

// 按键处理（包括方向键、PageUp/PageDown、End、Tab等）
static void editor_process_key(int key) {
    switch (key) {
        case 'q': // 按q退出
            E.quit = 1;
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editor_move_cursor(key);
            break;
        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            if (E.cy < E.numrows)
                E.cx = strlen(E.rows[E.cy]);//字符串长度
            break;
        case PAGE_UP: {
            int times = E.screenrows;
            while (times-- && E.cy > 0)
                E.cy--;//行减
            E.rowoff -= E.screenrows;
            if (E.rowoff < 0) E.rowoff = 0;
            break;
        }
        case PAGE_DOWN: {
            int times = E.screenrows;
            while (times-- && E.cy < E.numrows - 1)
                E.cy++;//行加
            E.rowoff += E.screenrows;
            if (E.rowoff > E.numrows - E.screenrows)
                E.rowoff = E.numrows - E.screenrows;
            if (E.rowoff < 0) E.rowoff = 0;
            break;
        }
        case '\t': // TAB键：移动光标到下一个tab stop
            if (E.cy >= 0 && E.cy < E.numrows) {
                int rx = editor_row_render_x(E.rows[E.cy], E.cx);
                int nexttab = rx + (TAB_WIDTH - (rx % TAB_WIDTH));
                int cx = E.cx;
                int cur_rx = rx;
                while (E.rows[E.cy][cx] && cur_rx < nexttab) {
                    if (E.rows[E.cy][cx] == '\t')
                        cur_rx += TAB_WIDTH - (cur_rx % TAB_WIDTH);
                    else
                        cur_rx++;
                    cx++;
                }
                E.cx = cx;
            }
            break;
        default:
            break;
    }
    // 保证光标行列在有效范围
    if (E.cy < 0) E.cy = 0;
    if (E.cy >= E.numrows) E.cy = E.numrows - 1;
    int rowlen = (E.cy >= 0 && E.cy < E.numrows) ? strlen(E.rows[E.cy]) : 0;
    if (E.cx < 0) E.cx = 0;
    if (E.cx > rowlen) E.cx = rowlen;
    // 滚屏：保证光标在可见区域
    if (E.cy < E.rowoff)
        E.rowoff = E.cy;
    if (E.cy >= E.rowoff + E.screenrows)
        E.rowoff = E.cy - E.screenrows + 1;
    if (E.rowoff < 0) E.rowoff = 0;
}

// 方向键移动光标
static void editor_move_cursor(int key) {
    switch (key) {
        case ARROW_LEFT:
            if (E.cx > 0) E.cx--;
            break;
        case ARROW_RIGHT:
            if (E.cy < E.numrows && E.cx < (int)strlen(E.rows[E.cy])) E.cx++;
            break;
        case ARROW_UP:
            if (E.cy > 0) { 
                E.cy--; 
                if(E.cx > (int)strlen(E.rows[E.cy])) 
                    E.cx = strlen(E.rows[E.cy]); 
            }
            break;
        case ARROW_DOWN:
            if (E.cy < E.numrows - 1) { 
                E.cy++; 
                if(E.cx > (int)strlen(E.rows[E.cy])) 
                    E.cx = strlen(E.rows[E.cy]); 
            }
            break;
    }
}
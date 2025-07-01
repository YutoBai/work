#ifndef TERMINAL_H
#define TERMINAL_H

// 终端原始模式
void terminal_enable_raw_mode(void);
void terminal_disable_raw_mode(void);

// 读写与窗口
void terminal_write(const char *s, int len);
int terminal_read_key(void);
void terminal_get_window_size(int *rows, int *cols);

// 逻辑按键枚举
enum {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

#endif
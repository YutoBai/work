#ifndef EDITOR_H
#define EDITOR_H


void editor_init(void);// 初始化编辑器

void editor_open(const char *filename);// 打开文件（可为NULL，新建空白）

void editor_loop(void);// 主循环，刷新显示

void editor_quit(void);// 退出，释放缓存

#endif
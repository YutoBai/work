#include <stdio.h>
#include "editor.h"

int main(int argc, char *argv[]) {
    const char *filename = (argc >= 2) ? argv[1] : NULL;
    editor_init();
    editor_open(filename);
    editor_loop();
    editor_quit();
    return 0;
}
#ifndef ERROR_H
#define ERROR_H

#include <stdio.h>

#define ESC_RED "\x1b[31m"
#define ESC_YELLOW "\x1b[33m"
#define ESC_CYAN "\x1b[36m"
#define ESC_MAGENTA "\x1b[35m"
#define ESC_GREEN "\x1b[32m"
#define ESC_NORMAL "\x1b[0m"
#define ESC_BOLD "\x1b[1m"

void log_error(char *file, size_t ln, size_t col);
void show_error(char *file, size_t ln, size_t col);
size_t error_count();

#endif
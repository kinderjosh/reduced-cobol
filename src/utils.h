#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>

char *mystrdup(char *str);
char *get_basename(char *file);
char *get_basepath(char *path);
char *get_file_extension(char *file);
char *replace_file_extension(char *file, char *extension, bool remove_path);

#endif
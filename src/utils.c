#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

char *mystrdup(char *str) {
    char *dup = malloc(strlen(str) + 1);
    strcpy(dup, str);
    return dup;
}

char *get_basename(char *file) {
    char *extension = strchr(file, '.');
    assert(extension != NULL);

    size_t len = extension - file;
    char *basename = malloc(len + 1);
    strncpy(basename, file, len);
    basename[len] = '\0';
    return basename;
}

char *get_basepath(char *path) {
    char *copy = mystrdup(path);

    if (strchr(copy, '/') == NULL)
        return copy;

    char *base = NULL;
    char *tok = strtok(copy, "/");
    char *last_tok;

    while (tok != NULL) {
        last_tok = tok;
        tok = strtok(NULL, "/");

        if (tok == NULL)
            base = mystrdup(last_tok);
    }

    assert(base != NULL);
    free(copy);
    return base;
}

char *replace_file_extension(char *file, char *extension, bool remove_path) {
    char *basename;

    if (remove_path) {
        char *basepath = get_basepath(file);
        basename = get_basename(basepath);
        free(basepath);
    } else
        basename = get_basename(file);

    char *rep = malloc(strlen(basename) + strlen(extension) + 2);
    sprintf(rep, "%s.%s", basename, extension);

    free(basename);
    return rep;
}
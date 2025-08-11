#ifndef PARSER_H
#define PARSER_H

#include "token.h"
#include "ast.h"
#include <stdio.h>

typedef struct {
    char *file;
    Token *tokens;
    size_t token_count;
    Token *tok;
    size_t pos;
    unsigned int cur_division;
    char *cur_program;
} Parser;

AST *parse_file(char *file);

#endif
#ifndef PARSER_H
#define PARSER_H

#include "token.h"
#include "ast.h"
#include <stdio.h>

typedef enum {
    DIV_NONE,
    DIV_IDENTIFICATION,
    DIV_DATA,
    DIV_PROCEDURE
} Division;

typedef enum {
    SECT_NONE,
    SECT_WORKING_STORAGE
} Section;

typedef struct {
    char *file;
    Token *tokens;
    size_t token_count;
    Token *tok;
    size_t pos;
    Division cur_division;
    Section cur_section;
    char *cur_program;
} Parser;

AST *parse_file(char *file);

#endif
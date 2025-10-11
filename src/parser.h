#ifndef PARSER_H
#define PARSER_H

#include "token.h"
#include "ast.h"
#include <stdio.h>
#include <stdbool.h>

typedef enum {
    DIV_NONE,
    DIV_IDENTIFICATION,
    DIV_ENVIRONMENT,
    DIV_DATA,
    DIV_PROCEDURE
} Division;

typedef enum {
    SECT_NONE,
    SECT_INPUT_OUTPUT,
    SECT_FILE,
    SECT_LINKAGE,
    SECT_WORKING_STORAGE
} Section;

typedef struct {
    char *file;
    Token *tokens;
    size_t token_count;
    Token *tok;
    size_t pos;
    char program_id[65];
    bool in_main;
    Division cur_div;
    Section cur_sect;
    bool parse_extra_value;
    bool in_set;
} Parser;

Variable *get_struct_sym(AST *ast);
PictureType get_value_type(AST *ast);
AST *parse_file(char *file, char **main_files, bool *out_had_main);

#endif
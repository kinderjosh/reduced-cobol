#ifndef LEXER_H
#define LEXER_H

#include "token.h"
#include <stdio.h>
#include <stdbool.h>

typedef struct {
    char *file;
    char *src;
    size_t src_len;
    char cur;
    size_t pos;
    size_t ln;
    size_t col;
} Lexer;

Lexer create_lexer(char *file, char **main_infiles);
void delete_lexer(Lexer *lex);
Token lex_next_token(Lexer *lex);

#endif
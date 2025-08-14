#include "token.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

Token create_token(TokenType type, char *value, size_t ln, size_t col) {
    return (Token){ .type = type, .value = value, .ln = ln, .col = col };
}

void delete_token(Token *tok) {
    free(tok->value);
}

char *tokentype_to_string(TokenType type) {
    switch (type) {
        case TOK_EOF: return "eof";
        case TOK_EOL: return "eol";
        case TOK_ID: return "identifier";
        case TOK_INT: return "int";
        case TOK_FLOAT: return "float";
        case TOK_STRING: return "string";
        case TOK_LPAREN: return "lparen";
        case TOK_RPAREN: return "rparen";
        case TOK_DOT: return "dot";
        case TOK_PLUS: return "plus";
        case TOK_MINUS: return "minus";
        case TOK_STAR: return "star";
        case TOK_SLASH: return "slash";
        case TOK_MOD: return "mod";
        case TOK_EQUAL: return "equal";
        default: break;
    }

    assert(false);
    return "undefined";
}
#include "parser.h"
#include "token.h"
#include "lexer.h"
#include "ast.h"
#include "utils.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>

#define NOP(ln, col) create_ast(AST_NOP, ln, col)

enum {
    DIV_NONE,
    DIV_IDENTIFICATION,
    DIV_PROCEDURE
};

char *cur_file;

Parser create_parser(char *file) {
    Lexer lex = create_lexer(file);
    Token tok;

    Token *tokens = malloc(32 * sizeof(Token));
    size_t token_count = 0;
    size_t token_cap = 32;

    while ((tok = lex_next_token(&lex)).type != TOK_EOF) {
        // Extra +1 for the EOF.
        if (token_count + 2 >= token_cap) {
            token_cap *= 2;
            tokens = realloc(tokens, token_cap * sizeof(Token));
        }

        tokens[token_count++] = tok;
    }

    // Add the EOF.
    tokens[token_count++] = tok;
    delete_lexer(&lex);
    return (Parser){ .file = file, .tokens = tokens, .token_count = token_count, .tok = &tokens[0], .pos = 0, .cur_division = DIV_NONE, .cur_program = NULL };
}

void delete_parser(Parser *prs) {
    for (size_t i = 0; i < prs->token_count; i++)
        delete_token(&prs->tokens[i]);

    free(prs->tokens);

    if (prs->cur_program != NULL)
        free(prs->cur_program);
}

static void eat(Parser *prs, TokenType type) {
    if (prs->tok->type != type) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "found token '%s' when expecting '%s'\n", tokentype_to_string(prs->tok->type), tokentype_to_string(type));
        show_error(prs->file, prs->tok->ln, prs->tok->col);
    }

    if (prs->tok->type != TOK_EOF)
        prs->tok = &prs->tokens[++prs->pos];
}

void eat_until(Parser *prs, TokenType type) {
    while (prs->tok->type != TOK_EOF && prs->tok->type != type)
        eat(prs, prs->tok->type);
}

bool expect_identifier(Parser *prs, char *id) {
    if (prs->tok->type != TOK_ID) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);

        if (id == NULL)
            fprintf(stderr, "found '%s' when expecting identifier\n", tokentype_to_string(prs->tok->type));
        else
            fprintf(stderr, "found '%s' when expecting identifier '%s'\n", tokentype_to_string(prs->tok->type), id);

        show_error(prs->file, prs->tok->ln, prs->tok->col);
        return false;
    } else if (id != NULL && strcmp(prs->tok->value, id) != 0) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "found identifier '%s' when expecting '%s'\n", prs->tok->value, id);
        show_error(prs->file, prs->tok->ln, prs->tok->col);
        return false;
    }

    return true;
}

AST *parse_stmt(Parser *prs);

AST *parse_value(Parser *prs) {
    AST *value = parse_stmt(prs);

    switch (value->type) {
        case AST_NOP:
        case AST_INT:
        case AST_FLOAT:
        case AST_STRING:
        case AST_VAR: break;
        default:
            log_error(value->file, value->ln, value->col);
            fprintf(stderr, "invalid value '%s'\n", asttype_to_string(value->type));
            show_error(value->file, value->ln, value->col);
            break;
    }

    return value;
}

void assert_in_division(Parser *prs, unsigned int div, size_t ln, size_t col) {
    char *str = "NONE";

    if (div == DIV_IDENTIFICATION)
        str = "IDENTIFICATION";

    if (prs->cur_division != div) {
        log_error(prs->file, ln, col);
        fprintf(stderr, "invalid statement outside of %s DIVISION\n", str);
        show_error(prs->file, ln, col);
        eat_until(prs, TOK_DOT); // Next statement.
    }
}

void parse_division(Parser *prs, char *div) {
    if (strcmp(div, "IDENTIFICATION") == 0)
        prs->cur_division = DIV_IDENTIFICATION;
    else if (strcmp(div, "PROCEDURE") == 0)
        prs->cur_division = DIV_PROCEDURE;
}

AST *parse_id(Parser *prs) {
    const size_t ln = prs->tok->ln;
    const size_t col = prs->tok->col;

    char *id = mystrdup(prs->tok->value);
    eat(prs, TOK_ID);

    // Divisions.
    if (strcmp(id, "IDENTIFICATION") == 0 || strcmp(id, "PROCEDURE") == 0) {
        if (expect_identifier(prs, "DIVISION")) {
            eat(prs, TOK_ID);
            parse_division(prs, id);
        }
        
        free(id);
        return NOP(ln, col);
    }

    // Identifications.
    else if (strcmp(id, "PROGRAM-ID") == 0) {
        free(id);
        assert_in_division(prs, DIV_IDENTIFICATION, ln, col);

        if (prs->tok->type == TOK_DOT)
            eat(prs, TOK_DOT);

        if (expect_identifier(prs, NULL)) {
            prs->cur_program = mystrdup(prs->tok->value);
            eat(prs, TOK_ID);
        }

        return NOP(ln, col);
    }

    // Intrinsics.
    else if (strcmp(id, "STOP") == 0) {
        free(id);
        assert_in_division(prs, DIV_PROCEDURE, ln, col);
        
        if (expect_identifier(prs, "RUN")) {
            eat(prs, TOK_ID);
            AST *ast = create_ast(AST_INTRINSIC, ln, col);
            ast->intrinsic.type = INTR_STOP;
            ast->intrinsic.arg = NOP(ln, col);
            return ast;
        }

        return NOP(ln, col);
    } else if (strcmp(id, "DISPLAY") == 0) {
        free(id);
        assert_in_division(prs, DIV_PROCEDURE, ln, col);

        AST *ast = create_ast(AST_INTRINSIC, ln, col);
        ast->intrinsic.type = INTR_DISPLAY;
        ast->intrinsic.arg = parse_value(prs);
        return ast;
    }
    
    log_error(prs->file, ln, col);
    fprintf(stderr, "undefined identifier '%s'\n", id);
    show_error(prs->file, ln, col);
    free(id);
    return NOP(ln, col);
}

AST *parse_constant(Parser *prs) {
    if (prs->tok->type == TOK_STRING) {
        AST *ast = create_ast(AST_STRING, prs->tok->ln, prs->tok->col);
        ast->constant.string = mystrdup(prs->tok->value);
        eat(prs, TOK_STRING);
        return ast;
    }

    AST *ast;
    char *endptr;
    errno = 0;

    if (prs->tok->type == TOK_INT) {
        ast = create_ast(AST_INT, prs->tok->ln, prs->tok->col);
        ast->constant.i64 = strtoll(prs->tok->value, &endptr, 10);
    } else {
        ast = create_ast(AST_FLOAT, prs->tok->ln, prs->tok->col);
        ast->constant.f64 = strtod(prs->tok->value, &endptr);
    }

    if (endptr == prs->tok->value || *endptr != '\0') {
        log_error(prs->file, ast->ln, ast->col);
        fprintf(stderr, "digit conversion failed\n");
        show_error(prs->file, ast->ln, ast->col);
    } else if (errno == ERANGE || errno == EINVAL) {
        log_error(prs->file, ast->ln, ast->col);
        fprintf(stderr, "digit conversion failed: %s\n", strerror(errno));
        show_error(prs->file, ast->ln, ast->col);
    }

    eat(prs, prs->tok->type);
    return ast;
}

AST *parse_stmt(Parser *prs) {
    while (prs->tok->type == TOK_DOT)
        eat(prs, TOK_DOT);

    switch (prs->tok->type) {
        case TOK_EOF: return NOP(prs->tok->ln, prs->tok->col);
        case TOK_ID: return parse_id(prs);
        case TOK_INT:
        case TOK_FLOAT:
        case TOK_STRING: return parse_constant(prs);
        default: break;
    }

    log_error(prs->file, prs->tok->ln, prs->tok->col);
    fprintf(stderr, "invalid statement '%s'\n", tokentype_to_string(prs->tok->type));
    show_error(prs->file, prs->tok->ln, prs->tok->col);

    // Next statement.
    eat_until(prs, TOK_DOT);
    eat(prs, TOK_DOT);

    return NOP(prs->tok->ln, prs->tok->col);
}

AST *parse_file(char *file) {
    cur_file = mystrdup(file);
    Parser prs = create_parser(file);

    AST *root = create_ast(AST_ROOT, 1, 1);
    root->root = create_astlist();

    while (prs.tok->type != TOK_EOF) {
        AST *stmt = parse_stmt(&prs);

        switch (stmt->type) {
            case AST_NOP:
                delete_ast(stmt);
                continue;
            case AST_INTRINSIC: break;
            default:
                log_error(stmt->file, stmt->ln, stmt->col);
                fprintf(stderr, "invalid statement '%s'\n", asttype_to_string(stmt->type));
                show_error(stmt->file, stmt->ln, stmt->col);
                break;
        }

        astlist_push(&root->root, stmt);
    }

    if (prs.cur_program == NULL) {
        log_error(file, 0, 0);
        fprintf(stderr, "missing PROGRAM-ID\n");
    }

    delete_parser(&prs);
    free(cur_file);
    return root;
}
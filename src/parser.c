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
#include <inttypes.h>

#define NOP(ln, col) create_ast(AST_NOP, ln, col)

#define TABLE_SIZE 1000

static Variable variables[TABLE_SIZE];

char *cur_file;

uint32_t hash_FNV1a(const char *data, size_t size) {
    uint32_t h = 2166136261UL;

    for (size_t i = 0; i < size; i++) {
        h ^= data[i];
        h *= 16777619;
    }

    return h % TABLE_SIZE;
}

Variable *find_variable(char *file, char *name) {
    char *data = malloc(strlen(file) + strlen(name) + 1);
    strcpy(data, file);
    strcat(data, name);

    Variable *var = &variables[hash_FNV1a(data, strlen(data))];
    free(data);
    return var;
}

bool variable_exists(char *file, char *name) {
    return find_variable(file, name)->used;
}

void add_variable(char *file, char *name, PictureType type, unsigned int count) {
    Variable *var = find_variable(file, name);
    assert(!var->used);
    var->file = file;
    var->name = name;
    var->type = type;
    var->count = count;
    var->used = true;
}

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

static Token *peek(Parser *prs, int offset) {
    if (prs->pos + offset >= prs->token_count)
        return &prs->tokens[prs->token_count - 1];
    else if ((int)prs->pos + offset < 1)
        return &prs->tokens[0];

    return &prs->tokens[prs->pos + offset];
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

AST *parse_value(Parser *prs, PictureType type) {
    (void)type;
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

bool assert_in_division(Parser *prs, Division div, size_t ln, size_t col) {
    char *str = "NONE";

    if (div == DIV_IDENTIFICATION)
        str = "IDENTIFICATION";
    else if (div == DIV_DATA)
        str = "DATA";
    else if (div == DIV_PROCEDURE)
        str = "PROCEDURE";

    if (prs->cur_division != div) {
        log_error(prs->file, ln, col);
        fprintf(stderr, "invalid statement outside of %s DIVISION\n", str);
        show_error(prs->file, ln, col);
        eat_until(prs, TOK_DOT); // Next statement.
        return false;
    }

    return true;
}

void parse_division(Parser *prs, char *div) {
    if (strcmp(div, "IDENTIFICATION") == 0)
        prs->cur_division = DIV_IDENTIFICATION;
    else if (strcmp(div, "DATA") == 0)
        prs->cur_division = DIV_DATA;
    else if (strcmp(div, "PROCEDURE") == 0)
        prs->cur_division = DIV_PROCEDURE;
}

bool assert_in_section(Parser *prs, Section sect, size_t ln, size_t col) {
    char *str = "NONE";

    if (sect == SECT_WORKING_STORAGE)
        str = "WORKING-STORAGE";

    if (prs->cur_section != sect) {
        log_error(prs->file, ln, col);
        fprintf(stderr, "invalid statement outside of %s SECTION\n", str);
        show_error(prs->file, ln, col);
        eat_until(prs, TOK_DOT); // Next statement.
        return false;
    }

    return true;
}

void parse_section(Parser *prs, char *sect) {
    if (strcmp(sect, "WORKING-STORAGE") == 0)
        prs->cur_section = SECT_WORKING_STORAGE;
}

AST *parse_move(Parser *prs, size_t ln, size_t col) {
    if (!assert_in_division(prs, DIV_PROCEDURE, ln, col))
        return NOP(ln, col);

    PictureType pictype = TYPE_ANY;
    Token *var_tok = peek(prs, 2);
    AST *var;

    if (var_tok->type != TOK_ID) {
        log_error(prs->file, var_tok->ln, var_tok->col);
        fprintf(stderr, "expected variable to move to but found '%s'\n", tokentype_to_string(var_tok->type));
        show_error(prs->file, var_tok->ln, var_tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    } else {
        Variable *sym = find_variable(prs->file, var_tok->value);

        if (!sym->used) {
            log_error(prs->file, var_tok->ln, var_tok->col);
            fprintf(stderr, "undefined variable '%s'\n", var_tok->value);
            show_error(prs->file, var_tok->ln, var_tok->col);

            eat_until(prs, TOK_DOT);
            return NOP(ln, col);
        }

        pictype = sym->type;
        var = create_ast(AST_VAR, var_tok->ln, var_tok->col);
        var->var.name = mystrdup(var_tok->value);
        var->var.sym = sym;
    }

    AST *ast = create_ast(AST_MOVE, ln, col);
    ast->move.src = parse_value(prs, pictype);

    if (!expect_identifier(prs, "TO")) {
        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    eat(prs, TOK_ID);
    ast->move.dst = var;
    eat(prs, TOK_ID);
    return ast;
}

AST *parse_id(Parser *prs) {
    const size_t ln = prs->tok->ln;
    const size_t col = prs->tok->col;

    char *id = mystrdup(prs->tok->value);
    eat(prs, TOK_ID);

    // Divisions.
    if (strcmp(id, "IDENTIFICATION") == 0 || strcmp(id, "PROCEDURE") == 0 || strcmp(id, "DATA") == 0) {
        if (expect_identifier(prs, "DIVISION")) {
            eat(prs, TOK_ID);
            parse_division(prs, id);
        }
        
        free(id);
        return NOP(ln, col);
    }

    // Sections.
    if (strcmp(id, "WORKING-STORAGE") == 0) {
        if (!assert_in_division(prs, DIV_DATA, ln, col))
            return NOP(ln, col);

        if (expect_identifier(prs, "SECTION")) {
            eat(prs, TOK_ID);
            parse_section(prs, id);
        }
        
        free(id);
        return NOP(ln, col);
    }

    // Identifications.
    else if (strcmp(id, "PROGRAM-ID") == 0) {
        free(id);

        if (!assert_in_division(prs, DIV_IDENTIFICATION, ln, col))
            return NOP(ln, col);

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

        if (!assert_in_division(prs, DIV_PROCEDURE, ln, col))
            return NOP(ln, col);
        
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

        if (!assert_in_division(prs, DIV_PROCEDURE, ln, col))
            return NOP(ln, col);

        AST *ast = create_ast(AST_INTRINSIC, ln, col);
        ast->intrinsic.type = INTR_DISPLAY;
        ast->intrinsic.arg = parse_value(prs, TYPE_ANY);

        if (ast->intrinsic.arg->type == AST_INT || ast->intrinsic.arg->type == AST_FLOAT) {
            log_error(ast->file, ast->ln, ast->col);
            fprintf(stderr, "attempting to display constant number '%s'\n", asttype_to_string(ast->intrinsic.arg->type));
            show_error(ast->file, ast->ln, ast->col);
        }

        return ast;
    } else if (strcmp(id, "MOVE") == 0) {
        free(id);
        return parse_move(prs, ln, col);
    }

    // User-defined stuff.
    Variable *sym;

    if ((sym = find_variable(prs->file, id))->used) {
        AST *ast = create_ast(AST_VAR, ln, col);
        ast->var.name = id;
        ast->var.sym = sym;
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

AST *parse_pic(Parser *prs) {
    if (!assert_in_section(prs, SECT_WORKING_STORAGE, prs->tok->ln, prs->tok->col))
        return NOP(prs->tok->ln, prs->tok->col);

    assert(prs->tok->type == TOK_INT);
    AST *level = parse_constant(prs);

    if (level->constant.i64 < 1 || level->constant.i64 > 49) {
        log_error(level->file, level->ln, level->col);
        fprintf(stderr, "invalid level '%" PRId64 "'; level not within 1-49\n", level->constant.i64);
        show_error(level->file, level->ln, level->col);
    }

    char *name = mystrdup(prs->tok->value);
    eat(prs, TOK_ID);
    eat(prs, TOK_ID); // PIC

    // A = alphabetical, X = alphanumeric, 9 = numeric
    PictureType type;

    if (strcmp(prs->tok->value, "A") == 0) {
        type = TYPE_ALPHABETIC;
        eat(prs, prs->tok->type);
    } else if (strcmp(prs->tok->value, "X") == 0) {
        type = TYPE_ALPHANUMERIC;
        eat(prs, prs->tok->type);
    } else if (strcmp(prs->tok->value, "9") == 0) {
        type = TYPE_NUMERIC;
        eat(prs, prs->tok->type);
    } else {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "invalid picture type '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);
        type = TYPE_ALPHANUMERIC;
    }

    AST *ast = create_ast(AST_PIC, level->ln, level->col);
    ast->pic.level = level->constant.i64;
    delete_ast(level);
    ast->pic.name = name;
    ast->pic.type = type;

    if (prs->tok->type == TOK_LPAREN) {
        eat(prs, TOK_LPAREN);

        if (prs->tok->type != TOK_INT) {
            log_error(prs->file, prs->tok->ln, prs->tok->col);
            fprintf(stderr, "non constant integer picture length '%s'\n", tokentype_to_string(prs->tok->type));
            show_error(prs->file, prs->tok->ln, prs->tok->col);

            eat_until(prs, TOK_DOT);
            ast->pic.count = 0;
        } else {
            AST *size = parse_constant(prs);
            ast->pic.count = (unsigned int)size->constant.i64;
            delete_ast(size);
        }

        eat(prs, TOK_RPAREN);
    } else
        ast->pic.count = 0;

    if (strcmp(prs->tok->value, "VALUE") == 0) {
        eat(prs, TOK_ID);
        ast->pic.value = parse_value(prs, type);
    } else
        ast->pic.value = NULL;

    add_variable(ast->file, ast->pic.name, ast->pic.type, ast->pic.count);
    return ast;
}

AST *parse_stmt(Parser *prs) {
    while (prs->tok->type == TOK_DOT)
        eat(prs, TOK_DOT);

    switch (prs->tok->type) {
        case TOK_EOF: return NOP(prs->tok->ln, prs->tok->col);
        case TOK_ID: return parse_id(prs);
        case TOK_INT:
            // Check if it's a PICTURE clause.
            if (peek(prs, 1)->type == TOK_ID && peek(prs, 2)->type == TOK_ID &&
                    strcmp(peek(prs, 2)->value, "PIC") == 0)
                return parse_pic(prs);

            __attribute__((fallthrough));
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
            case AST_INTRINSIC:
            case AST_PIC:
            case AST_MOVE: break;
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
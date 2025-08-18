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

// I kinda feel guilty for these but like it's the best way...
#define IS_MATH(prs) (prs->tok->type == TOK_PLUS || prs->tok->type == TOK_MINUS || prs->tok->type == TOK_STAR || prs->tok->type == TOK_SLASH || strcmp(prs->tok->value, "MOD") == 0)
#define IS_CONDITION(prs) (prs->tok->type == TOK_EQ || prs->tok->type == TOK_EQUAL || prs->tok->type == TOK_NEQ || prs->tok->type == TOK_LT || prs->tok->type == TOK_LTE || prs->tok->type == TOK_GT || prs->tok->type == TOK_GTE || strcmp(prs->tok->value, "IS") == 0 || strcmp(prs->tok->value, "AND") == 0 || strcmp(prs->tok->value, "OR") == 0 || (strcmp(prs->tok->value, "NOT") == 0 && strcmp(peek(prs, 1)->value, "EQUAL") == 0))

#define TABLE_SIZE 1000

static Variable variables[TABLE_SIZE];

// Sometimes we want to return multiple things but we can't,
// so we just append to this list.
static ASTList *root_ptr;

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

Variable *add_variable(char *file, char *name, PictureType type, unsigned int count) {
    Variable *var = find_variable(file, name);
    assert(!var->used);
    var->file = file; // Wrong but who cares TOFIX
    var->name = name;
    var->type = type;
    var->count = count;
    var->used = true;
    return var;
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

void jump_to(Parser *prs, size_t pos) {
    prs->pos = pos;
    prs->tok = &prs->tokens[pos];
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
        case AST_VAR:
        case AST_PARENS:
        case AST_NOT:
        case AST_LABEL: break;
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

// TOFIX: I really don't like how gross and builtin this shit is.
AST *parse_any_no_error(Parser *prs) {
    if (prs->tok->type == TOK_INT || prs->tok->type == TOK_FLOAT || prs->tok->type == TOK_STRING)
        return parse_stmt(prs);

    Variable *var = NULL;

    if ((var = find_variable(prs->file, prs->tok->value))->used) {
        AST *ast = create_ast(AST_VAR, prs->tok->ln, prs->tok->col);
        ast->var.name = mystrdup(prs->tok->value);
        ast->var.sym = var;
        eat(prs, TOK_ID);
        return ast;
    }

    //eat(prs, prs->tok->type);
    return NOP(prs->tok->ln, prs->tok->col);
}

static bool displayed_previously;

AST *parse_display(Parser *prs, size_t ln, size_t col, ASTList *root) {
    if (!assert_in_division(prs, DIV_PROCEDURE, ln, col))
        return NOP(ln, col);

    size_t before = prs->pos;
    AST *thing = parse_any_no_error(prs);
    jump_to(prs, before);

    // End of display.
    if (thing->type != AST_STRING && thing->type != AST_VAR) {
        delete_ast(thing);

        if (!displayed_previously)
            return NOP(ln, col);

        // Add the newline at the end of a formatted display.
        AST *nl = create_ast(AST_DISPLAY, prs->tok->ln, prs->tok->col);
        nl->display.value = create_ast(AST_STRING, prs->tok->ln, prs->tok->col);
        nl->display.value->constant.string = mystrdup("\\n");
        nl->display.add_newline = false;
        return nl;
    }

    delete_ast(thing);
    AST *ast = create_ast(AST_DISPLAY, ln, col);
    ast->display.value = parse_stmt(prs);

    if (ast->display.value->type == AST_INT || ast->display.value->type == AST_FLOAT) {
        log_error(ast->file, ast->ln, ast->col);
        fprintf(stderr, "attempting to display constant number '%s'\n", asttype_to_string(ast->display.value->type));
        show_error(ast->file, ast->ln, ast->col);
    }

    if (prs->tok->type == TOK_DOT) {
        ast->display.add_newline = true;
        return ast;
    }

    // Sometimes like in IF statements we won't have a . to find,
    // so we'll just stop when we find an invalid display value.
    before = prs->pos;
    AST *next = parse_any_no_error(prs);

    if (next->type != AST_STRING && next->type != AST_VAR) {
        // Invalid thing, stop.
        jump_to(prs, before);
        delete_ast(next);
        ast->display.add_newline = true;
        return ast;
    }

    // String concatenation going on here, we need to print the original string first.
    displayed_previously = true;
    ast->display.add_newline = false;
    astlist_push(root, ast);

    AST *bruh = create_ast(AST_DISPLAY, next->ln, next->col);
    bruh->display.value = next;
    bruh->display.add_newline = false;
    astlist_push(root, bruh);

    return parse_display(prs, ln, col, root);
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

AST *parse_arithmetic(Parser *prs, char *name, size_t ln, size_t col) {
    if (!assert_in_division(prs, DIV_PROCEDURE, ln, col))
        return NOP(ln, col);

    // TODO: Probably type this somehow.
    AST *value = parse_value(prs, TYPE_ANY);

    if (strcmp(name, "ADD") == 0 && !expect_identifier(prs, "TO")) {
        delete_ast(value);
        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    } else if (strcmp(name, "SUBTRACT") == 0 && !expect_identifier(prs, "FROM")) {
        delete_ast(value);
        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    } else if (strcmp(name, "MULTIPLY") == 0 && !expect_identifier(prs, "BY")) {
        delete_ast(value);
        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    } else if (strcmp(name, "DIVIDE") == 0 && !expect_identifier(prs, "INTO")) {
        delete_ast(value);
        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    eat(prs, TOK_ID);
    AST *right = parse_value(prs, TYPE_ANY);
    AST *give = NULL;

    if (strcmp(prs->tok->value, "GIVING") != 0) {
        if (strcmp(name, "MULTIPLY") == 0 || strcmp(name, "DIVIDE") == 0) {
            log_error(prs->file, ln, col);
            fprintf(stderr, "%s must be given implicitly\n", name);
            show_error(prs->file, ln, col);
        }

        // If we did get one of these, it'll show an error below anyway.
        if (right->type != AST_VAR) {
            log_error(right->file, right->ln, right->col);
            fprintf(stderr, "implicit giving statement but right value isn't a storage value\n");
            show_error(right->file, right->ln, right->col);
        }
    } else {
        eat(prs, TOK_ID);
        give = parse_value(prs, TYPE_ANY);

        if (give->type != AST_VAR) {
            log_error(give->file, give->ln, give->col);
            fprintf(stderr, "giving value isn't a storage value\n");
            show_error(give->file, give->ln, give->col);
        }
    }

    // MULTIPLY and DIVIDE can't be implicitly given.
    if (strcmp(name, "DIVIDE") == 0 && strcmp(prs->tok->value, "REMAINDER") == 0) {
        eat(prs, TOK_ID);
        AST *remainder_dst = parse_value(prs, TYPE_ANY);

        if (remainder_dst->type != AST_VAR) {
            log_error(remainder_dst->file, remainder_dst->ln, remainder_dst->col);
            fprintf(stderr, "remainder value isn't a storage value\n");
            show_error(remainder_dst->file, remainder_dst->ln, remainder_dst->col);
            delete_ast(remainder_dst);
        } else {
            // Two arithmetics in one, can't return this, so we have to push a MODULUS AST.
            AST *remainder = create_ast(AST_ARITHMETIC, remainder_dst->ln, remainder_dst->col);
            remainder->arithmetic.right = right;
            remainder->arithmetic.dst = remainder_dst;
            remainder->arithmetic.left = value;
            remainder->arithmetic.name = mystrdup("REMAINDER");
            remainder->arithmetic.cloned_left = remainder->arithmetic.cloned_right = true;
            remainder->arithmetic.implicit_giving = false;

            if (strcmp(name, "DIVIDE") == 0 && give != NULL && strcmp(right->var.name, remainder_dst->var.name) == 0) {
                // Doing a modulus into its own variable, don't want to
                // overrwrite with a division here.
                remainder->arithmetic.cloned_left = remainder->arithmetic.cloned_right = false;
                delete_ast(give); // GIVING for the division, not the remainder, don't need it.
                free(name);
                return remainder;
            }

            astlist_push(root_ptr, remainder);
        }
    }

    AST *ast = create_ast(AST_ARITHMETIC, ln, col);
    ast->arithmetic.left = value;
    ast->arithmetic.name = name;
    ast->arithmetic.right = right;
    ast->arithmetic.cloned_left = ast->arithmetic.cloned_right = false;

    if (give == NULL)
        ast->arithmetic.implicit_giving = true;
    else {
        ast->arithmetic.implicit_giving = false;
        ast->arithmetic.dst = give;
    }

    return ast;
}

AST *parse_math(Parser *prs, AST *first, PictureType type) {
    if (first == NULL)
        first = parse_value(prs, TYPE_ANY);

    AST *ast = create_ast(AST_MATH, first->ln, first->col);
    ast->math = create_astlist();
    astlist_push(&ast->math, first);

    while (IS_MATH(prs)) {
        AST *oper = create_ast(AST_OPER, prs->tok->ln, prs->tok->col);

        if (strcmp(prs->tok->value, "MOD") == 0)
            oper->oper = TOK_MOD;
        else
            oper->oper = prs->tok->type;

        eat(prs, prs->tok->type);

        astlist_push(&ast->math, oper);
        astlist_push(&ast->math, parse_value(prs, type));
    }

    return ast;
}

AST *parse_oper(Parser *prs) {
    AST *oper = create_ast(AST_OPER, prs->tok->ln, prs->tok->col);
    oper->oper = TOK_EQ; // Fallback.

    if (strcmp(prs->tok->value, "IS") == 0) {
        eat(prs, TOK_ID);

        if (strcmp(prs->tok->value, "EQUAL") == 0) {
            oper->oper = TOK_EQ;
            eat(prs, TOK_ID);

            if (strcmp(prs->tok->value, "TO") != 0) {
                log_error(prs->file, prs->tok->ln, prs->tok->col);
                fprintf(stderr, "expected identifier 'TO' following 'EQUAL'\n");
                show_error(prs->file, prs->tok->ln, prs->tok->col);
                return oper;
            }

            eat(prs, TOK_ID);
        } else if (strcmp(prs->tok->value, "LESS") == 0) {
            eat(prs, TOK_ID);

            if (strcmp(prs->tok->value, "THAN") != 0) {
                log_error(prs->file, prs->tok->ln, prs->tok->col);
                fprintf(stderr, "expected identifier 'THAN' following 'LESS'\n");
                show_error(prs->file, prs->tok->ln, prs->tok->col);
                return oper;
            }

            eat(prs, TOK_ID);

            if (strcmp(prs->tok->value, "OR") != 0) {
                oper->oper = TOK_LT;
                return oper;
            }

            eat(prs, TOK_ID);

            if (strcmp(prs->tok->value, "EQUAL") != 0) {
                log_error(prs->file, prs->tok->ln, prs->tok->col);
                fprintf(stderr, "expected identifier 'EQUAL' following 'OR'\n");
                show_error(prs->file, prs->tok->ln, prs->tok->col);
                return oper;
            }

            eat(prs, TOK_ID);

            if (strcmp(prs->tok->value, "TO") != 0) {
                log_error(prs->file, prs->tok->ln, prs->tok->col);
                fprintf(stderr, "expected identifier 'TO' following 'EQUAL'\n");
                show_error(prs->file, prs->tok->ln, prs->tok->col);
            } else {
                oper->oper = TOK_LTE;
                eat(prs, TOK_ID);
            }
        } else if (strcmp(prs->tok->value, "GREATER") == 0) {
            eat(prs, TOK_ID);

            if (strcmp(prs->tok->value, "THAN") != 0) {
                log_error(prs->file, prs->tok->ln, prs->tok->col);
                fprintf(stderr, "expected identifier 'THAN' following 'GREATER'\n");
                show_error(prs->file, prs->tok->ln, prs->tok->col);
                return oper;
            }

            eat(prs, TOK_ID);

            if (strcmp(prs->tok->value, "OR") != 0) {
                oper->oper = TOK_GT;
                return oper;
            }

            eat(prs, TOK_ID);

            if (strcmp(prs->tok->value, "EQUAL") != 0) {
                log_error(prs->file, prs->tok->ln, prs->tok->col);
                fprintf(stderr, "expected identifier 'EQUAL' following 'OR'\n");
                show_error(prs->file, prs->tok->ln, prs->tok->col);
                return oper;
            }

            eat(prs, TOK_ID);

            if (strcmp(prs->tok->value, "TO") != 0) {
                log_error(prs->file, prs->tok->ln, prs->tok->col);
                fprintf(stderr, "expected identifier 'TO' following 'EQUAL'\n");
                show_error(prs->file, prs->tok->ln, prs->tok->col);
            } else {
                oper->oper = TOK_GTE;
                eat(prs, TOK_ID);
            }
        }
    } else if (strcmp(prs->tok->value, "NOT") == 0) {
        eat(prs, TOK_ID);

        if (strcmp(prs->tok->value, "EQUAL") != 0) {
            log_error(prs->file, prs->tok->ln, prs->tok->col);
            fprintf(stderr, "expected identifier 'EQUAL' following 'NOT'\n");
            show_error(prs->file, prs->tok->ln, prs->tok->col);
            return oper;
        }

        eat(prs, TOK_ID);

        if (strcmp(prs->tok->value, "TO") != 0) {
            log_error(prs->file, prs->tok->ln, prs->tok->col);
            fprintf(stderr, "expected identifier 'TO' following 'EQUAL'\n");
            show_error(prs->file, prs->tok->ln, prs->tok->col);
        } else {
            eat(prs, TOK_ID);
            oper->oper = TOK_NEQ;
        }
    } else if (strcmp(prs->tok->value, "AND") == 0) {
        oper->oper = TOK_AND;
        eat(prs, TOK_ID);
    } else if (strcmp(prs->tok->value, "OR") == 0) {
        oper->oper = TOK_OR;
        eat(prs, TOK_ID);
    } else {
        oper->oper = prs->tok->type;
        eat(prs, prs->tok->type);
    }

    return oper;
}

AST *parse_condition(Parser *prs, AST *first) {
    if (first == NULL)
        first = parse_value(prs, TYPE_ANY);

    AST *ast = create_ast(AST_CONDITION, first->ln, first->col);
    ast->condition = create_astlist();
    astlist_push(&ast->condition, first);

    while (IS_CONDITION(prs)) {
        astlist_push(&ast->condition, parse_oper(prs));
        astlist_push(&ast->condition, parse_value(prs, TYPE_ANY));

        if (strcmp(prs->tok->value, "AND") == 0 || strcmp(prs->tok->value, "OR") == 0) {
            AST *oper = create_ast(AST_OPER, prs->tok->ln, prs->tok->col);
            oper->oper = strcmp(prs->tok->value, "AND") == 0 ? TOK_AND : TOK_OR;
            eat(prs, TOK_ID);
            astlist_push(&ast->condition, oper);
            astlist_push(&ast->condition, parse_value(prs, TYPE_ANY));
        }
    }

    return ast;
}

AST *parse_compute(Parser *prs, size_t ln, size_t col) {
    if (!assert_in_division(prs, DIV_PROCEDURE, ln, col))
        return NOP(ln, col);

    AST *dst = parse_value(prs, TYPE_ANY);

    if (dst->type != AST_VAR) {
        log_error(dst->file, dst->ln, dst->col);
        fprintf(stderr, "compute value isn't a storage value\n");
        show_error(dst->file, dst->ln, dst->col);

        delete_ast(dst);
        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    eat(prs, TOK_EQUAL);

    AST *ast = create_ast(AST_COMPUTE, ln, col);
    ast->compute.dst = dst;
    ast->compute.math = parse_math(prs, NULL, dst->var.sym->type);
    return ast;
}

// false = deleted
bool validate_stmt(AST *stmt) {
    switch (stmt->type) {
        case AST_NOP:
            delete_ast(stmt);
            return false;
        case AST_STOP:
        case AST_DISPLAY:
        case AST_PIC:
        case AST_MOVE:
        case AST_ARITHMETIC:
        case AST_COMPUTE:
        case AST_IF:
        case AST_LABEL:
        case AST_PERFORM:
        case AST_PROC:
        case AST_PERFORM_CONDITION:
        case AST_PERFORM_COUNT: break;
        default:
            log_error(stmt->file, stmt->ln, stmt->col);
            fprintf(stderr, "invalid statement '%s'\n", asttype_to_string(stmt->type));
            show_error(stmt->file, stmt->ln, stmt->col);
            break;
    }
    
    return true;
}

AST *parse_if(Parser *prs, size_t ln, size_t col) {
    if (!assert_in_division(prs, DIV_PROCEDURE, ln, col))
        return NOP(ln, col);

    AST *condition = parse_condition(prs, NULL);

    if (!expect_identifier(prs, "THEN")) {
        delete_ast(condition);
        return NOP(ln, col);
    }

    eat(prs, TOK_ID);
    ASTList body = create_astlist();

    while (prs->tok->type != TOK_EOF && strcmp(prs->tok->value, "END-IF") != 0 && strcmp(prs->tok->value, "ELSE") != 0) {
        AST *stmt = parse_stmt(prs);

        if (validate_stmt(stmt))
            astlist_push(&body, stmt);
    }

    AST *ast = create_ast(AST_IF, ln, col);
    ast->if_stmt.condition = condition;
    ast->if_stmt.body = body;
    ast->if_stmt.else_body = create_astlist();

    if (strcmp(prs->tok->value, "END-IF") == 0) {
        eat(prs, TOK_ID);
        return ast;
    } else if (strcmp(prs->tok->value, "ELSE") != 0)
        return ast;

    eat(prs, TOK_ID);

    while (prs->tok->type != TOK_EOF && strcmp(prs->tok->value, "END-IF") != 0) {
        AST *stmt = parse_stmt(prs);

        if (validate_stmt(stmt))
            astlist_push(&ast->if_stmt.else_body, stmt);
    }

    if (expect_identifier(prs, "END-IF"))
        eat(prs, TOK_ID);

    return ast;
}

AST *parse_not(Parser *prs, size_t ln, size_t col) {
    if (!assert_in_division(prs, DIV_PROCEDURE, ln, col))
        return NOP(ln, col);

    AST *ast = create_ast(AST_NOT, ln, col);
    ast->not_value = parse_value(prs, TYPE_ANY);
    return ast;
}

/*
AST *parse_goto(Parser *prs, size_t ln, size_t col) {
    if (!assert_in_division(prs, DIV_PROCEDURE, ln, col))
        return NOP(ln, col);

    if (!expect_identifier(prs, "TO"))
        return NOP(ln, col);

    eat(prs, TOK_ID);

    if (prs->tok->type != TOK_ID) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "expected label name but found '%s'\n", tokentype_to_string(prs->tok->type));
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    // We don't want to check if this label exists yet, as we want to
    // be able to jump forwards before a label is defined.
    // We make sure all labels are checked and resolved after compilation
    // in resolve_labels().

    AST  *ast = create_ast(AST_GOTO, ln, col);
    ast->go = create_ast(AST_LABEL, prs->tok->ln, prs->tok->col);
    ast->go->label = mystrdup(prs->tok->value);
    eat(prs, TOK_ID);
    return ast;
}
*/

AST *parse_perform(Parser *prs, size_t ln, size_t col) {
    if (prs->tok->type != TOK_ID) {
        log_error(prs->file, ln, col);
        fprintf(stderr, "expected procedure name but found '%s'\n", tokentype_to_string(prs->tok->type));
        show_error(prs->file, ln, col);

        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    AST *ast;

    if (strcmp(peek(prs, 2)->value, "TIMES") == 0 ||
            strcmp(peek(prs, 1)->value, "UNTIL") == 0) {
        AST *perf = create_ast(AST_PERFORM, ln, col);
        perf->perform = create_ast(AST_LABEL, ln, col);
        perf->perform->label = mystrdup(prs->tok->value);
        eat(prs, TOK_ID);

        if (strcmp(prs->tok->value, "UNTIL") == 0) {
            eat(prs, TOK_ID);
            ast = create_ast(AST_PERFORM_CONDITION, ln, col);
            ast->perform_condition.proc =  perf;
            ast->perform_condition.condition = parse_condition(prs, NULL);
            return ast;
        }

        ast = create_ast(AST_PERFORM_COUNT, ln, col);
        ast->perform_count.proc =  perf;

        if (prs->tok->type != TOK_INT) {
            log_error(prs->file, prs->tok->ln, prs->tok->col);
            fprintf(stderr, "expected integer constant TIMES count but found '%s'\n", tokentype_to_string(prs->tok->type));
            show_error(prs->file, prs->tok->ln, prs->tok->col);
            ast->perform_count.times = 0;
        } else {
            // parse_value() will also handle conversion errors.
            AST *count = parse_value(prs, TYPE_NUMERIC);
            ast->perform_count.times = (unsigned int)count->constant.i64;
            delete_ast(count);
        }

        if (expect_identifier(prs, "TIMES"))
            eat(prs, TOK_ID);

        return ast;
    }

    ast = create_ast(AST_PERFORM, ln, col);
    ast->perform = create_ast(AST_LABEL, prs->tok->ln, prs->tok->col);
    ast->perform->label = mystrdup(prs->tok->value);
    eat(prs, TOK_ID);
    return ast;

    /*

    ASTList body = create_astlist();

    //while (prs->tok->type != TOK_EOF && strcmp(prs->tok->value, "END-PERFORM") != 0 && strcmp(prs->tok->value, "UNTIL") != 0 &&
    while (prs->tok->type != TOK_EOF && strcmp(prs->tok->value, "UNTIL") != 0) {
        AST *stmt = parse_stmt(prs);

        if (validate_stmt(stmt))
            astlist_push(&body, stmt);
    }

    ast = create_ast(AST_PERFORM_CONDITION, ln, col);
    eat(prs, TOK_ID);
    ast->perform_condition.block = body;
    ast->perform_condition.condition = parse_condition(prs, NULL);
    return ast;
    */
}

AST *parse_procedure(Parser *prs, char *name, size_t ln, size_t col) {
    add_variable(prs->file, name, TYPE_ANY, 0)->is_label = true;

    AST *ast = create_ast(AST_PROC, ln, col);
    ast->proc.name = name;
    ast->proc.body = create_astlist();

    while (prs->tok->type != TOK_EOF) {
        // New procedure or end of program.
        if (prs->tok->type == TOK_ID && (peek(prs, 1)->type == TOK_DOT || strcmp(prs->tok->value, "END") == 0))
            break;

        // So we can check for DISPLAY.
        while (prs->tok->type == TOK_DOT)
            eat(prs, TOK_DOT);

        AST *stmt;
        
        if (strcmp(prs->tok->value, "DISPLAY") == 0) {
            eat(prs, TOK_ID);

            // DISPLAY can push arguments to the root, so we need to
            // specify to push into the procedure body instead.
            stmt = parse_display(prs, prs->tok->ln, prs->tok->col, &ast->proc.body);
        } else
            stmt = parse_stmt(prs);

        if (validate_stmt(stmt))
            astlist_push(&ast->proc.body, stmt);
    }

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
            return create_ast(AST_STOP, ln, col);
        }

        return NOP(ln, col);
    } else if (strcmp(id, "DISPLAY") == 0) {
        free(id);
        displayed_previously = false;
        return parse_display(prs, ln, col, root_ptr);
    } else if (strcmp(id, "MOVE") == 0) {
        free(id);
        return parse_move(prs, ln, col);
    } else if (strcmp(id, "ADD") == 0 || strcmp(id, "SUBTRACT") == 0 || strcmp(id, "MULTIPLY") == 0 || strcmp(id, "DIVIDE") == 0)
        return parse_arithmetic(prs, id, ln, col);
    else if (strcmp(id, "COMPUTE") == 0) {
        free(id);
        return parse_compute(prs, ln, col);
    } else if (strcmp(id, "IF") == 0) {
        free(id);
        return parse_if(prs, ln, col);
    } else if (strcmp(id, "NOT") == 0) {
        free(id);
        return parse_not(prs, ln, col);
    //} else if (strcmp(id, "GO") == 0) {
      //  free(id);
        //return parse_goto(prs, ln, col);
    } else if (strcmp(id, "END") == 0) {
        free(id);

        if (!expect_identifier(prs, "PROGRAM"))
            return NOP(ln, col);

        eat(prs, TOK_ID);

        if (expect_identifier(prs, prs->cur_program))
            eat(prs, TOK_ID);

        return NOP(ln, col);
    } else if (strcmp(id, "PERFORM") == 0) {
        free(id);
        return parse_perform(prs, ln, col);
    }

    // User-defined stuff.
    Variable *sym;

    if ((sym = find_variable(prs->file, id))->used) {
        if (sym->is_label) {
            AST *ast = create_ast(AST_LABEL, ln, col);
            ast->label = id;
            return ast;
        }

        AST *ast = create_ast(AST_VAR, ln, col);
        ast->var.name = id;
        ast->var.sym = sym;
        return ast;
    } else if (prs->cur_division == DIV_PROCEDURE && prs->tok->type == TOK_DOT)
        return parse_procedure(prs, id, ln, col);

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
        } else if (type == TYPE_NUMERIC) {
            // Currently, we don't care if numbers are just PIC 9 or have a length.
            // Just ignore it for now.
            // TODO: This maybe.
            eat_until(prs, TOK_RPAREN);
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

AST *parse_parens(Parser *prs) {
    AST *ast = create_ast(AST_PARENS, prs->tok->ln, prs->tok->col);
    eat(prs, TOK_LPAREN);
    ast->parens = parse_value(prs, TYPE_ANY);

    if (IS_MATH(prs))
        ast->parens = parse_math(prs, ast->parens, TYPE_NUMERIC);
    else if (IS_CONDITION(prs))
        ast->parens = parse_condition(prs, ast->parens);

    eat(prs, TOK_RPAREN);
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
        case TOK_LPAREN: return parse_parens(prs);
        default: break;
    }

    log_error(prs->file, prs->tok->ln, prs->tok->col);
    fprintf(stderr, "invalid statement '%s'\n", tokentype_to_string(prs->tok->type));
    show_error(prs->file, prs->tok->ln, prs->tok->col);

    eat_until(prs, TOK_DOT); // Next statement.
    return NOP(prs->tok->ln, prs->tok->col);
}

AST *parse_file(char *file) {
    cur_file = mystrdup(file);
    Parser prs = create_parser(file);

    AST *root = create_ast(AST_ROOT, 1, 1);
    root->root = create_astlist();
    root_ptr = &root->root;

    while (prs.tok->type != TOK_EOF) {
        AST *stmt = parse_stmt(&prs);

        if (validate_stmt(stmt))
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
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
#include <ctype.h>

#define NOP(ln, col) create_ast(AST_NOP, ln, col)

#define IS_MATH(prs) (prs->tok->type == TOK_PLUS || prs->tok->type == TOK_MINUS || prs->tok->type == TOK_STAR || prs->tok->type == TOK_SLASH || strcmp(prs->tok->value, "MOD") == 0)

#define IS_CONDITION(prs) (prs->tok->type == TOK_EQ || prs->tok->type == TOK_EQUAL || prs->tok->type == TOK_NEQ || prs->tok->type == TOK_LT || prs->tok->type == TOK_LTE || prs->tok->type == TOK_GT || prs->tok->type == TOK_GTE || strcmp(prs->tok->value, "IS") == 0 || strcmp(prs->tok->value, "AND") == 0 || strcmp(prs->tok->value, "OR") == 0 || (strcmp(prs->tok->value, "NOT") == 0 && strcmp(peek(prs, 1)->value, "EQUAL") == 0))

// TODO: Implement COMP-3 and COMP-6.
#define IS_COMP(tok) (strcmp(tok->value, "COMP") == 0 || strcmp(tok->value, "COMP-1") == 0 || strcmp(tok->value, "COMP-2") == 0 || strcmp(tok->value, "COMP-4") == 0 || strcmp(tok->value, "COMP-5") == 0)

#define TABLE_SIZE 10000

extern char *cur_dir;
//extern ASTList delayed_assigns;

static Variable variables[TABLE_SIZE];

// Sometimes we want to return multiple things but we can't,
// so we just append to this list.
static ASTList *root_ptr;

static size_t uids = 0;

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
    /*
    char *data = malloc(strlen(file) + strlen(name) + 1);
    strcpy(data, file);
    strcat(data, name);
    */

    // For now, we won't include the filename in the hashing algorithm,
    // because it messes with finding symbols from other files, a.k.a copybooks.
    // However, the codebase already passes the file as a parameter everywhere,
    // and this may change later, so we'll just supress the warning for now.
    (void)file;

    char *data = mystrdup(name);
    Variable *var = &variables[hash_FNV1a(data, strlen(data))];
    free(data);
    return var;
}

bool variable_exists(char *file, char *name) {
    return find_variable(file, name)->used;
}

Variable *add_variable(char *file, char *name, PictureType type, unsigned int count) {
    Variable *var = find_variable(file, name);

    if (var->used && var->is_linkage_src) {
        // This variable was externed in another file,
        // and is being declared in this file.
        return var;
    }

    assert(!var->used);
    var->file = file; // Wrong but who cares TOFIX
    var->name = name;
    var->type = type;
    var->count = count;
    var->used = true;
    var->is_fd = var->is_index = var->is_label = var->is_linkage_src = var->using_in_proc_div = var->pointer_been_set = false;
    var->fields = NULL;
    var->struct_sym = NULL;
    var->uid = uids++;
    var->type.pointer_uid = var->uid;
    return var;
}

Variable *get_struct_sym(AST *ast) {
    if (ast->type == AST_VAR)
        return ast->var.sym;
    else if (ast->type == AST_FIELD)
        return get_struct_sym(ast->field.base);

    assert(false);
    fprintf(stderr, "get_struct_sym ERROR!!!\n");
    exit(EXIT_FAILURE);
    return NULL; // Shut gcc up.
}

PictureType get_value_type(AST *ast) {
    switch (ast->type) {
        case AST_ZERO:
        // 0 places = trim leading zeros
        case AST_INT: return (PictureType){ .type = TYPE_SIGNED_NUMERIC, .count = 0, .places = 0 };
        case AST_FLOAT: return (PictureType){ .type = TYPE_DECIMAL_NUMERIC, .count = 0, .places = 0 };
        case AST_STRING: {
            PictureType type = (PictureType){ .type = TYPE_ALPHANUMERIC, .places = strlen(ast->constant.string) };
            type.count = type.places; // Not sure if doing this in the initializer is UB.
            return type;
        }
        case AST_VAR: return ast->var.sym->type;
        case AST_PARENS: return get_value_type(ast->parens);
        case AST_NOT:
        case AST_MATH: return (PictureType){ .type = TYPE_DECIMAL_NUMERIC, .count = 0 };
        case AST_BOOL:
        case AST_NULL:
        case AST_CONDITION: return (PictureType){ .type = TYPE_UNSIGNED_NUMERIC, .count = 0 };
        case AST_SUBSCRIPT: {
            PictureType type = get_value_type(ast->subscript.base);
            Variable *sym = get_struct_sym(ast->subscript.base);

            if (sym->count > 0 && sym->type.count > 0)
                type.count = sym->type.places;
            else if (ast->subscript.base->type != AST_FIELD)
                type.count = 0;

            return type;
        }
        // Make LENGTHOF COMP-5 to print as %zu
        case AST_LENGTHOF: return (PictureType){ .type = TYPE_UNSIGNED_SUPRESSED_NUMERIC, .comp_type = COMP5, .count = 0, .places = 18 };
        case AST_FIELD: return ast->field.sym->type;
        case AST_ADDRESSOF:  {
            PictureType type = get_value_type(ast->addressof_value);
            return (PictureType){ .type = TYPE_POINTER, .count = type.count, .comp_type = COMP_POINTER };
        }
        default: break;
    }

    printf(">>>%s\n", asttype_to_string(ast->type));
    assert(false);
    return (PictureType){ .type = TYPE_SIGNED_NUMERIC, .count = 0 };
}

Parser create_parser(char *file, char **main_infiles) {
    Lexer lex = create_lexer(file, main_infiles);
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
    return (Parser){ .file = file, .tokens = tokens, .token_count = token_count, .tok = &tokens[0], 
        .pos = 0, .program_id = {0}, .in_main = false, .cur_div = DIV_NONE, .cur_sect = SECT_NONE, .parse_extra_value = true, .in_set = false };
}

void delete_parser(Parser *prs) {
    for (size_t i = 0; i < prs->token_count; i++)
        delete_token(&prs->tokens[i]);

    free(prs->tokens);
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
AST *parse_math(Parser *prs, AST *first, unsigned int type);
AST *parse_subscript(Parser *prs, AST *base);

AST *parse_value(Parser *prs, unsigned int type) {
    // TODO: Actually kinda check types bruh?
    (void)type;
    AST *value = parse_stmt(prs);

    switch (value->type) {
        case AST_NOP:
        case AST_INT:
        case AST_FLOAT:
        case AST_STRING: break;
        case AST_VAR:
            if (!prs->in_set && !value->var.sym->is_linkage_src && value->var.sym->type.comp_type == COMP_POINTER && !value->var.sym->pointer_been_set) {
                log_error(prs->file, value->ln, value->col);
                fprintf(stderr, "pointer variable '%s' used before being SET\n", value->var.name);
                show_error(prs->file, value->ln, value->col);
            }
            break;
        case AST_PARENS:
        case AST_NOT:
        case AST_LABEL:
        case AST_NULL:
        case AST_BOOL:
        case AST_ZERO:
        case AST_LENGTHOF:
        case AST_ADDRESSOF: break;
        case AST_SUBSCRIPT:
            if (value->subscript.value == NULL)
                break;

            goto fallthrough;
        case AST_FIELD:
            if (value->field.value == NULL)
                break;

            goto fallthrough;
        default:
fallthrough:
            log_error(value->file, value->ln, value->col);
            fprintf(stderr, "invalid value '%s'\n", asttype_to_string(value->type));
            show_error(value->file, value->ln, value->col);
            break;
    }

    if (!prs->parse_extra_value)
        return value;

    if (IS_MATH(prs))
        return parse_math(prs, value, type);
    else if (prs->tok->type == TOK_LPAREN)
        return parse_subscript(prs, value);

    return value;
}

AST *parse_id(Parser *prs);

// TOFIX: I really don't like how gross and builtin this shit is.
AST *parse_any_no_error(Parser *prs) {
    if (prs->tok->type == TOK_INT || prs->tok->type == TOK_FLOAT || prs->tok->type == TOK_STRING)
        return parse_stmt(prs);
    else if (strcmp(prs->tok->value, "LENGTH") == 0)
        return parse_id(prs);
    else if (strcmp(prs->tok->value, "ADDRESS") == 0)
        return parse_id(prs);

    Variable *var = find_variable(prs->file, prs->tok->value);
    
    if (!var->used)
        return NOP(prs->tok->ln, prs->tok->col);
    
    AST *ast;

    if (var->struct_sym != NULL) {
        ast = create_ast(AST_FIELD, prs->tok->ln, prs->tok->col);
        ast->field.sym = var;
        ast->field.base = create_ast(AST_VAR, prs->tok->ln, prs->tok->col);
        ast->field.base->var.name = mystrdup(var->struct_sym->name);
        ast->field.base->var.sym = var->struct_sym;
        ast->field.value = NULL;
    } else {
        ast = create_ast(AST_VAR, prs->tok->ln, prs->tok->col);
        ast->var.name = mystrdup(prs->tok->value);
        ast->var.sym = var;
    }

    eat(prs, TOK_ID);

    if (prs->tok->type == TOK_LPAREN)
        return parse_subscript(prs, ast);

    return ast;
}

static bool displayed_previously;
static bool first_display;

#define LAST_DISPLAY_VALUE_WASNT_VALID(thing) (thing->type != AST_STRING && thing->type != AST_VAR && thing->type != AST_SUBSCRIPT && thing->type != AST_LENGTHOF && thing->type != AST_FIELD && thing->type != AST_ADDRESSOF)

AST *parse_display(Parser *prs, ASTList *root) {
    const size_t ln = prs->tok->ln;
    const size_t col = prs->tok->col;

    // This is the end of the display statement, doesn't print a newline, return nothing.
    if (strcmp(prs->tok->value, "WITH") == 0 && strcmp(peek(prs, 1)->value, "NO") == 0 && strcmp(peek(prs, 2)->value, "ADVANCING") == 0) {
        eat(prs, TOK_ID);
        eat(prs, TOK_ID);
        eat(prs, TOK_ID);
        return NOP(ln, col);
    }

    if (first_display) // Recursive function, only eat DISPLAY the first time.
        eat(prs, TOK_ID);

    size_t before = prs->pos;
    AST *thing = parse_any_no_error(prs);
    jump_to(prs, before);

    // End of display.
    if (LAST_DISPLAY_VALUE_WASNT_VALID(thing)) {
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
    ast->display.value = parse_value(prs, TYPE_ANY);

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

    if (LAST_DISPLAY_VALUE_WASNT_VALID(next)) {
        // Invalid thing, stop.
        jump_to(prs, before);
        delete_ast(next);

        if (strcmp(prs->tok->value, "WITH") == 0 && strcmp(peek(prs, 1)->value, "NO") == 0 && strcmp(peek(prs, 2)->value, "ADVANCING") == 0) {
            eat(prs, TOK_ID);
            eat(prs, TOK_ID);
            eat(prs, TOK_ID);
            ast->display.add_newline = false;
        } else
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

    first_display = false;
    return parse_display(prs, root);
}

AST *parse_move(Parser *prs) {
    const size_t ln = prs->tok->ln;
    const size_t col = prs->tok->col;
    eat(prs, TOK_ID);

    /*
    PictureType pictype = { .type = TYPE_ANY, .count = 0 };
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
        } else if (sym->is_index) {
            log_error(prs->file, var_tok->ln, var_tok->col);
            fprintf(stderr, "moving into index variable '%s' when SET should be used instead\n", var_tok->value);
            show_error(prs->file, var_tok->ln, var_tok->col);

            eat_until(prs, TOK_DOT);
            return NOP(ln, col);
        }

        pictype = sym->type;
        size_t old_pos = prs->pos;
        jump_to(prs, prs->pos + 2);

        // var_tok is now the current token.

        // TODO: If alphabetic or alphanumeric, this isn't checked because of moving strings,
        // will this cause errors if not a string?
        // Check if we are moving into a table variable and not a subscript.
        if (sym->count > 0 && peek(prs, 1)->type != TOK_LPAREN && sym->type.type != TYPE_ALPHABETIC && sym->type.type != TYPE_ALPHANUMERIC) {
            log_error(prs->file, var_tok->ln, var_tok->col);
            fprintf(stderr, "moving into table '%s'\n", sym->name);
            show_error(prs->file, var_tok->ln, var_tok->col);
        }

        var = parse_stmt(prs);
        jump_to(prs, old_pos);
    }
    */

    AST *ast = create_ast(AST_MOVE, ln, col);
    ast->move.is_set = false;
    ast->move.src = parse_value(prs, TYPE_ANY);
    //ast->move.src = parse_value(prs, type);

    if (!expect_identifier(prs, "TO")) {
        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    eat(prs, TOK_ID);
    //ast->move.dst = var;
    ast->move.dst = parse_value(prs, TYPE_ANY);

    PictureType dst_type = get_value_type(ast->move.dst);

    if (dst_type.comp_type == COMP_POINTER)
        dst_type.pointer_uid = get_value_type(ast->move.dst).pointer_uid;

    /*
    eat(prs, TOK_ID);

    // Skip any () if there.
    if (var->type == AST_SUBSCRIPT) {
        eat_until(prs, TOK_RPAREN);
        eat(prs, TOK_RPAREN);
    }
    */

    return ast;
}

AST *parse_arithmetic(Parser *prs) {
    const size_t ln = prs->tok->ln;
    const size_t col = prs->tok->col;

    char *name = mystrdup(prs->tok->value);
    eat(prs, TOK_ID);

    AST *value = parse_value(prs, TYPE_ANY);

    if (strcmp(name, "ADD") == 0 && !expect_identifier(prs, "TO")) {
        delete_ast(value);
        eat_until(prs, TOK_DOT);
        free(name);
        return NOP(ln, col);
    } else if (strcmp(name, "SUBTRACT") == 0 && !expect_identifier(prs, "FROM")) {
        delete_ast(value);
        eat_until(prs, TOK_DOT);
        free(name);
        return NOP(ln, col);
    } else if (strcmp(name, "MULTIPLY") == 0 && !expect_identifier(prs, "BY")) {
        delete_ast(value);
        eat_until(prs, TOK_DOT);
        free(name);
        return NOP(ln, col);
    } else if (strcmp(name, "DIVIDE") == 0 && !expect_identifier(prs, "BY")) {
        delete_ast(value);
        eat_until(prs, TOK_DOT);
        free(name);
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
        if (right->type != AST_VAR && right->type != AST_SUBSCRIPT) {
            log_error(right->file, right->ln, right->col);
            fprintf(stderr, "implicit giving clause but right value isn't a storage value\n");
            show_error(right->file, right->ln, right->col);
        }
    } else {
        eat(prs, TOK_ID);
        give = parse_value(prs, TYPE_ANY);

        if (give->type != AST_VAR && give->type != AST_SUBSCRIPT) {
            log_error(give->file, give->ln, give->col);
            fprintf(stderr, "giving value isn't a storage value\n");
            show_error(give->file, give->ln, give->col);
        }
    }

    // MULTIPLY and DIVIDE can't be implicitly given.
    if (strcmp(name, "DIVIDE") == 0 && strcmp(prs->tok->value, "REMAINDER") == 0) {
        eat(prs, TOK_ID);
        AST *remainder_dst = parse_value(prs, TYPE_ANY);

        if (remainder_dst->type != AST_VAR && remainder_dst->type != AST_SUBSCRIPT) {
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

            if (strcmp(name, "DIVIDE") == 0 && give != NULL) {
                //if (right->type == AST_VAR && strcmp(right->var.name, remainder_dst->var.name) == 0) {
                    // Doing a modulus into its own variable, don't want to
                    // overrwrite with a division here.
                    remainder->arithmetic.cloned_left = remainder->arithmetic.cloned_right = false;
                    delete_ast(give); // GIVING for the division, not the remainder, don't need it.
                    free(name);
                    return remainder;
                //}
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

AST *parse_math(Parser *prs, AST *first, unsigned int type) {
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

AST *parse_compute(Parser *prs) {
    const size_t ln = prs->tok->ln;
    const size_t col = prs->tok->col;
    eat(prs, TOK_ID);

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
    ast->compute.math = parse_math(prs, NULL, dst->var.sym->type.type);
    return ast;
}

// false = deleted
bool validate_stmt(AST *stmt) {
    switch (stmt->type) {
        case AST_NOP:
            delete_ast(stmt);
            return false;
        case AST_STOP:
        case AST_STOP_RUN:
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
        case AST_PERFORM_COUNT:
        case AST_PERFORM_VARYING:
        case AST_PERFORM_UNTIL:
        case AST_CALL:
        case AST_STRING_BUILDER:
        case AST_STRING_SPLITTER:
        case AST_OPEN:
        case AST_CLOSE:
        case AST_SELECT:
        case AST_READ:
        case AST_WRITE:
        case AST_INSPECT:
        case AST_ACCEPT:
        case AST_EXIT:
        case AST_SET_POINTER_TYPE: break;
        default:
            log_error(stmt->file, stmt->ln, stmt->col);
            fprintf(stderr, "invalid clause '%s'\n", asttype_to_string(stmt->type));
            show_error(stmt->file, stmt->ln, stmt->col);
            break;
    }
    
    return true;
}

AST *parse_procedure_stmt(Parser *prs, ASTList *root);

AST *parse_if(Parser *prs) {
    const size_t ln = prs->tok->ln;
    const size_t col = prs->tok->col;
    eat(prs, TOK_ID);

    AST *condition = parse_condition(prs, NULL);

    if (!expect_identifier(prs, "THEN")) {
        delete_ast(condition);
        return NOP(ln, col);
    }

    eat(prs, TOK_ID);
    ASTList body = create_astlist();

    while (prs->tok->type != TOK_EOF && strcmp(prs->tok->value, "END-IF") != 0 && strcmp(prs->tok->value, "ELSE") != 0) {
        AST *stmt = parse_procedure_stmt(prs, &body);

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
        AST *stmt = parse_procedure_stmt(prs, &ast->if_stmt.else_body);

        if (validate_stmt(stmt))
            astlist_push(&ast->if_stmt.else_body, stmt);
    }

    if (expect_identifier(prs, "END-IF"))
        eat(prs, TOK_ID);

    return ast;
}

AST *parse_not(Parser *prs) {
    AST *ast = create_ast(AST_NOT, prs->tok->ln, prs->tok->col);
    eat(prs, TOK_ID);
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

AST *parse_perform_varying(Parser *prs, size_t ln, size_t col) {
    eat(prs, TOK_ID);

    if (!expect_identifier(prs, NULL)) {
        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    Variable *var = find_variable(prs->file, prs->tok->value);

    if (!var->used) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "undefined variable '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    } else if (var->is_index) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "varying variable '%s' is an index variable\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    } else if (var->count > 0) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "varying variable '%s' is a table\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    char *name = mystrdup(prs->tok->value);
    eat(prs, TOK_ID);

    if (!expect_identifier(prs, "FROM")) {
        free(name);
        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    eat(prs, TOK_ID);
    AST *from = parse_value(prs, TYPE_ANY);

    if (!expect_identifier(prs, "BY")) {
        free(name);
        delete_ast(from);
        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    eat(prs, TOK_ID);
    AST *by = parse_value(prs, TYPE_ANY);

    if (!expect_identifier(prs, "UNTIL")) {
        free(name);
        delete_ast(from);
        delete_ast(by);
        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    eat(prs, TOK_ID);

    AST *iter = create_ast(AST_VAR, prs->tok->ln, prs->tok->col);
    iter->var.name = name;
    iter->var.sym = var;

    AST *ast = create_ast(AST_PERFORM_VARYING, ln, col);
    ast->perform_varying.var = iter;
    ast->perform_varying.from = from;
    ast->perform_varying.by = by;
    ast->perform_varying.until = parse_condition(prs, NULL);
    ast->perform_varying.body = create_astlist();

    while (prs->tok->type != TOK_EOF && strcmp(prs->tok->value, "END-PERFORM") != 0) {
        AST *stmt = parse_procedure_stmt(prs, &ast->perform_varying.body);

        if (validate_stmt(stmt))
            astlist_push(&ast->perform_varying.body, stmt);
    }

    eat(prs, TOK_ID);

    if (prs->tok->type == TOK_DOT)
        eat(prs, TOK_DOT);

    return ast;
}

AST *parse_perform_until(Parser *prs, size_t ln, size_t col) {
    eat(prs, TOK_ID);

    AST *ast = create_ast(AST_PERFORM_UNTIL, ln, col);
    ast->perform_until.until = parse_condition(prs, NULL);
    ast->perform_until.body = create_astlist();

    while (prs->tok->type != TOK_EOF && strcmp(prs->tok->value, "END-PERFORM") != 0) {
        AST *stmt = parse_procedure_stmt(prs, &ast->perform_until.body);

        if (validate_stmt(stmt))
            astlist_push(&ast->perform_until.body, stmt);
    }

    if (expect_identifier(prs, "END-PERFORM")) {
        eat(prs, TOK_ID);

        if (prs->tok->type == TOK_DOT)
            eat(prs, TOK_DOT);
    }
    return ast;
}

AST *parse_perform(Parser *prs) {
    const size_t ln = prs->tok->ln;
    const size_t col = prs->tok->col;
    eat(prs, TOK_ID);

    if (strcmp(prs->tok->value, "VARYING") == 0)
        return parse_perform_varying(prs, ln, col);
    else if (strcmp(prs->tok->value, "UNTIL") == 0)
        return parse_perform_until(prs, ln, col);
    else if (!expect_identifier(prs, NULL)) {
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
            AST *count = parse_value(prs, TYPE_UNSIGNED_NUMERIC);
            ast->perform_count.times = (unsigned int)count->constant.i32;
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

AST *parse_procedure(Parser *prs) {
    prs->in_main = false;
    const size_t ln = prs->tok->ln;
    const size_t col = prs->tok->col;

    char *name = mystrdup(prs->tok->value);
    eat(prs, TOK_ID);

    Variable *var = add_variable(prs->file, name, (PictureType){ .type = TYPE_ANY, .count = 0 }, 0);
    var->is_label = true;

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
            first_display = true;

            // DISPLAY can push arguments to the root, so we need to
            // specify to push into the procedure body instead.
            stmt = parse_display(prs, &ast->proc.body);
        } else
            stmt = parse_procedure_stmt(prs, &ast->proc.body);

        if (validate_stmt(stmt))
            astlist_push(&ast->proc.body, stmt);
    }

    return ast;
}

AST *parse_field(Parser *prs, AST *base) {
    assert(base->type == AST_VAR);
    eat(prs, TOK_LPAREN);

    if (!expect_identifier(prs, NULL)) {
        eat_until(prs, TOK_DOT);
        return base;
    }

    Variable *sym = find_variable(prs->file, prs->tok->value);
    bool found = false;

    for (size_t i = 0; i < base->var.sym->fields->size; i++) {
        if (strcmp(base->var.sym->fields->items[i]->pic.name, prs->tok->value) == 0) {
            found = true;
            break;
        }
    }

    if (!found) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "undefined field '%s' in group variable '%s'\n", prs->tok->value, base->var.name);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return base;
    }

    eat(prs, TOK_ID);
    eat(prs, TOK_RPAREN);

    AST *ast = create_ast(AST_FIELD, base->ln, base->col);
    ast->field.sym = sym;
    ast->field.base = base;
    ast->field.value = NULL;
    return ast;
}

AST *parse_subscript(Parser *prs, AST *base) {
    unsigned int table_size = 1;
    bool unknown_size = false;

    switch (base->type) {
        case AST_VAR:
            // Just let pointers be accessed.
            if (base->var.sym->type.comp_type == COMP_POINTER) {
                unknown_size = true;
                break;
            }

            else if (base->var.sym->count > 0) {
                table_size = base->var.sym->count;
                break;
            }
            // Strings should also be accessible.
            else if ((base->var.sym->type.type == TYPE_ALPHABETIC || base->var.sym->type.type == TYPE_ALPHANUMERIC) && base->var.sym->type.count > 0) {
                table_size = base->var.sym->type.count;
                break;
            } else if (base->var.sym->fields->size > 0)
                return parse_field(prs, base);

            log_error(base->file, base->ln, base->col);
            fprintf(stderr, "accessing non-table variable '%s'\n", base->var.name);
            show_error(base->file, base->ln, base->col);
            break;
        case AST_FIELD:
            if (base->field.sym->count > 0) {
                table_size = base->field.sym->count;
                break;
            }
            // Strings should also be accessible.
            else if ((base->field.sym->type.type == TYPE_ALPHABETIC || base->field.sym->type.type == TYPE_ALPHANUMERIC) && base->field.sym->type.count > 0) {
                table_size = base->field.sym->type.count;
                break;
            } else if (base->field.sym->fields != NULL && base->field.sym->fields->size > 0)
                return parse_field(prs, base);
            else if (base->field.sym->struct_sym->count > 0) {
                table_size = base->field.sym->struct_sym->count;
                break;
            }

            log_error(base->file, base->ln, base->col);
            fprintf(stderr, "accessing non-table variable '%s'\n", base->field.sym->name);
            show_error(base->file, base->ln, base->col);
            break;
        // We don't know the count at compile time, 
        // so we'll just assume 49 because thats the max level of PICs.
        case AST_ARGV:
            table_size = 49;
            break;
        default:
            log_error(base->file, base->ln, base->col);
            fprintf(stderr, "accessing non-table value '%s'\n", asttype_to_string(base->type));
            show_error(base->file, base->ln, base->col);
            break;
    }

    AST *ast = create_ast(AST_SUBSCRIPT, base->ln, base->col);
    ast->subscript.base = base;

    eat(prs, TOK_LPAREN);
    AST *index = parse_value(prs, TYPE_ANY);

    if (!unknown_size) {
        switch (index->type) {
            case AST_NOP:
            case AST_MATH:
            case AST_SUBSCRIPT: break;
            case AST_INT:
                if (index->constant.i32 < 1) {
                    log_error(index->file, index->ln, index->col);
                    fprintf(stderr, "subscript %d goes below the minimum of 1\n", index->constant.i32);
                    show_error(index->file, index->ln, index->col);
                } else if ((unsigned int)index->constant.i32 > table_size) {
                    log_error(index->file, index->ln, index->col);
                    fprintf(stderr, "subscript %d goes out of bounds of table size %u\n", index->constant.i32, table_size);
                    show_error(index->file, index->ln, index->col);
                }
                break;
            case AST_VAR:
                if (index->var.sym->count > 0) {
                    log_error(index->file, index->ln, index->col);
                    fprintf(stderr, "subscript variable '%s' is a table\n", index->var.name);
                    show_error(index->file, index->ln, index->col);
                } else if (index->var.sym->type.type != TYPE_UNSIGNED_NUMERIC && index->var.sym->type.type != TYPE_SIGNED_NUMERIC && 
                        index->var.sym->type.type != TYPE_SIGNED_SUPRESSED_NUMERIC && index->var.sym->type.type != TYPE_UNSIGNED_SUPRESSED_NUMERIC) {
                    log_error(index->file, index->ln, index->col);
                    fprintf(stderr, "subscript variable '%s' is not numeric\n", index->var.name);
                    show_error(index->file, index->ln, index->col);
                }
                break;
            default:
                log_error(index->file, index->ln, index->col);
                fprintf(stderr, "invalid subscript value '%s'\n", asttype_to_string(index->type));
                show_error(index->file, index->ln, index->col);
                break;
        }
    }

    ast->subscript.index = index;
    eat(prs, TOK_RPAREN);

    // TOFIX: I don't think we actually need this field for COBOL,
    // it's just a relic from my previous C-like compilers.
    ast->subscript.value = NULL;

    return ast;
}

Variable *get_sym_from_ast(AST *ast) {
    switch (ast->type) {
        case AST_VAR: return ast->var.sym;
        case AST_SUBSCRIPT: return get_sym_from_ast(ast->subscript.base);
        case AST_FIELD: return ast->field.sym;
        default: break;
    }

    fprintf(stderr, "BAD ERROR!!!");
    exit(EXIT_FAILURE);
    return NULL; // Shut gcc up.
}

AST *parse_set(Parser *prs) {
    const size_t ln = prs->tok->ln;
    const size_t col = prs->tok->col;
    eat(prs, TOK_ID);

    /*
    Variable *var = find_variable(prs->file, prs->tok->value);

    if (!var->used) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "undefined index variable '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    } else if (!var->is_index && var->type.comp_type != COMP_POINTER) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "setting non-index variable '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    char *name = mystrdup(prs->tok->value);
    eat(prs, TOK_ID);
    */

    prs->in_set = true;

    AST *dst = parse_value(prs, TYPE_ANY);

    if (strcmp(prs->tok->value, "UP") == 0 || strcmp(prs->tok->value, "DOWN") == 0) {
        TokenType math = strcmp(prs->tok->value, "UP") == 0 ? TOK_PLUS : TOK_MINUS;
        eat(prs, TOK_ID);

        if (!expect_identifier(prs, "BY")) {
            delete_ast(dst);
            eat_until(prs, TOK_DOT);
            return NOP(ln, col);
        }

        eat(prs, TOK_ID);

        AST *ast = create_ast(AST_ARITHMETIC, ln, col);
        ast->arithmetic.name = mystrdup(math == TOK_PLUS ? "ADD" : "SUBTRACT");
        ast->arithmetic.cloned_left = ast->arithmetic.cloned_right = false;
        ast->arithmetic.dst = dst;
        ast->arithmetic.implicit_giving = true;
        ast->arithmetic.left = parse_value(prs, TYPE_ANY);
        ast->arithmetic.right = ast->arithmetic.dst;
        return ast;
    }

    if (!expect_identifier(prs, "TO")) {
        delete_ast(dst);
        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    eat(prs, TOK_ID);

    AST *ast = create_ast(AST_MOVE, ln, col);
    ast->move.is_set = true;
    ast->move.dst = dst;
    ast->move.src = parse_value(prs, TYPE_ANY);

    prs->in_set = false;

    PictureType dst_type = get_value_type(dst);

    if (dst_type.comp_type != COMP_POINTER)
        return ast;

    // If we're setting a pointer address, we want that pointer to be
    // used as the same type of the data it's pointing to.
    AST *set = create_ast(AST_SET_POINTER_TYPE, ln, col);
    set->set_pointer_type.sym = get_sym_from_ast(dst);
    set->set_pointer_type.type = get_value_type(ast->move.src->addressof_value);
    astlist_push(root_ptr, set);

    set->set_pointer_type.sym->pointer_been_set = true;
    return ast;
}

AST *parse_call(Parser *prs) {
    const size_t ln = prs->tok->ln;
    const size_t col = prs->tok->col;
    eat(prs, TOK_ID);

    if (prs->tok->type != TOK_STRING) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "expected string for function to call but found '%s'\n", tokentype_to_string(prs->tok->type));
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    AST *ast = create_ast(AST_CALL, ln, col);
    ast->call.name = mystrdup(prs->tok->value);
    eat(prs, TOK_STRING);
    ast->call.args = create_astlist();

    if (strcmp(prs->tok->value, "USING") == 0) {
        eat(prs, TOK_ID);
        astlist_push(&ast->call.args, parse_value(prs, TYPE_ANY));

        while (prs->tok->type == TOK_COMMA) {
            eat(prs, TOK_COMMA);
            astlist_push(&ast->call.args, parse_value(prs, TYPE_ANY));
        }
    }

    if (strcmp(prs->tok->value, "RETURNING") == 0) {
        eat(prs, TOK_ID);
        ast->call.returning = parse_value(prs, TYPE_ANY);
    } else
        ast->call.returning = NULL;

    return ast;
}

StringStatement parse_string_stmt(Parser *prs) {
    AST *value = parse_value(prs, TYPE_ANY);

    if (value->type == AST_VAR && value->var.sym->count > 0) {
        log_error(value->file, value->ln, value->col);
        fprintf(stderr, "table '%s' in string statement\n", value->var.name);
        show_error(value->file, value->ln, value->col);
    }

    StringStatement stmt = (StringStatement){ .value = value, .delimit = 0 };

    if (!expect_identifier(prs, "DELIMITED"))
        return stmt;

    eat(prs, TOK_ID);

    if (!expect_identifier(prs, "BY"))
        return stmt;

    eat(prs, TOK_ID);

    if (strcmp(prs->tok->value, "SPACE") == 0)
        stmt.delimit = DELIM_SPACE;
    else if (strcmp(prs->tok->value, "SIZE") == 0)
        stmt.delimit = DELIM_SIZE;
    else {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "invalid delimiter '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);
        return stmt;
    }

    eat(prs, TOK_ID);
    return stmt;
}

AST *parse_unstring(Parser *prs) {
    const size_t ln = prs->tok->ln;
    const size_t col = prs->tok->col;
    eat(prs, TOK_ID);

    AST *ast = create_ast(AST_STRING_SPLITTER, ln, col);
    ast->string_splitter.base = parse_string_stmt(prs);
    ast->string_splitter.into_vars = create_astlist();

    while (prs->tok->type != TOK_EOF && strcmp(prs->tok->value, "END-UNSTRING") != 0 && strcmp(prs->tok->value, "INTO") != 0)
        astlist_push(&ast->string_splitter.into_vars, parse_value(prs, TYPE_ANY));

    if (!expect_identifier(prs, "INTO")) {
        eat_until(prs, TOK_DOT);
        return ast;
    }

    eat(prs, TOK_ID);

    while (prs->tok->type != TOK_EOF && strcmp(prs->tok->value, "END-UNSTRING") != 0)
        astlist_push(&ast->string_splitter.into_vars, parse_value(prs, TYPE_ANY));

    if (expect_identifier(prs, "END-UNSTRING"))
        eat(prs, TOK_ID);

    return ast;
}

AST *parse_string_builder(Parser *prs) {
    const size_t ln = prs->tok->ln;
    const size_t col = prs->tok->col;
    eat(prs, TOK_ID);

    // (My apostraphe key died halfway through this comment)
    // String manipulation in COBOL SUCKS. From what Ive seen,
    // you start with a base value and delimit it, and then
    // every other value you pass into it gets appended on,
    // in accordance to its delimits and stuff.

    AST *ast = create_ast(AST_STRING_BUILDER, ln, col);
    ast->string_builder.base = parse_string_stmt(prs);
    ast->string_builder.stmts = malloc(4 * sizeof(StringStatement));
    ast->string_builder.stmt_count = 0;
    ast->string_builder.stmt_cap = 4;

    while (prs->tok->type != TOK_EOF && strcmp(prs->tok->value, "INTO") != 0 && strcmp(prs->tok->value, "END-STRING") != 0) {
        StringStatement stmt = parse_string_stmt(prs);

        if (ast->string_builder.stmt_count + 1 >= ast->string_builder.stmt_cap) {
            ast->string_builder.stmt_cap *= 2;
            ast->string_builder.stmts = realloc(ast->string_builder.stmts, ast->string_builder.stmt_cap * sizeof(StringStatement));
        }

        ast->string_builder.stmts[ast->string_builder.stmt_count++] = stmt;
    }

    if (expect_identifier(prs, "INTO")) {
        eat(prs, TOK_ID);
        ast->string_builder.into_var = parse_value(prs, TYPE_ANY);
    }

    if (strcmp(prs->tok->value, "WITH") == 0 && strcmp(peek(prs, 1)->value, "POINTER") == 0) {
        eat(prs, TOK_ID);
        eat(prs, TOK_ID);

        if (!expect_identifier(prs, NULL)) {
            eat_until(prs, TOK_DOT);
            ast->string_builder.with_pointer = NULL;
            return ast;
        }

        ast->string_builder.with_pointer = parse_value(prs, TYPE_ANY);
    } else
        ast->string_builder.with_pointer = NULL;

    if (expect_identifier(prs, "END-STRING"))
        eat(prs, TOK_ID);

    return ast;
}

AST *parse_open(Parser *prs) {
    const size_t ln = prs->tok->ln;
    const size_t col = prs->tok->col;
    eat(prs, TOK_ID);

    if (!expect_identifier(prs, NULL)) {
        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    unsigned int open_type = 0;

    if (strcmp(prs->tok->value, "INPUT") == 0)
        open_type = OPEN_INPUT;
    else if (strcmp(prs->tok->value, "OUTPUT") == 0)
        open_type = OPEN_OUTPUT;
    else if (strcmp(prs->tok->value, "I-O") == 0)
        open_type = OPEN_IO;
    else if (strcmp(prs->tok->value, "EXTEND") == 0)
        open_type = OPEN_EXTEND;
    else {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "invalid opening mode '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    eat(prs, TOK_ID);

    if (!expect_identifier(prs, NULL)) {
        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    Variable *var = find_variable(prs->file, prs->tok->value);

    if (!var->used) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "undefined variable '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    } else if (!var->is_fd) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "file '%s' is not a file descriptor\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    AST *ast = create_ast(AST_OPEN, ln, col);
    ast->open.type = open_type;
    ast->open.filename = create_ast(AST_VAR, prs->tok->ln, prs->tok->col);
    ast->open.filename->var.name = mystrdup(prs->tok->value);
    ast->open.filename->var.sym = var;
    eat(prs, TOK_ID);
    return ast;
}

AST *parse_close(Parser *prs) {
    const size_t ln = prs->tok->ln;
    const size_t col = prs->tok->col;
    eat(prs, TOK_ID);

    if (!expect_identifier(prs, NULL)) {
        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    Variable *var = find_variable(prs->file, prs->tok->value);

    if (!var->used) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "undefined variable '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    } else if (!var->is_fd) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "file '%s' is not a file descriptor\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    AST *ast = create_ast(AST_CLOSE, ln, col);
    ast->close_filename = create_ast(AST_VAR, prs->tok->ln, prs->tok->col);
    ast->close_filename->var.name = mystrdup(prs->tok->value);
    ast->close_filename->var.sym = var;
    eat(prs, TOK_ID);
    return ast;
}

AST *parse_read(Parser *prs) {
    const size_t ln = prs->tok->ln;
    const size_t col = prs->tok->col;
    eat(prs, TOK_ID);

    Variable *var = find_variable(prs->file, prs->tok->value);

    if (!var->used) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "undefined variable '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    } else if (!var->is_fd) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "file '%s' is not a file descriptor\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    eat(prs, TOK_ID);

    if (!expect_identifier(prs, "INTO")) {
        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    eat(prs, TOK_ID);

    Variable *into = find_variable(prs->file, prs->tok->value);

    if (!into->used) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "undefined variable '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    } else if ((into->type.type != TYPE_ALPHABETIC && into->type.type != TYPE_ALPHANUMERIC) ||
            into->type.count == 0) {

        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "reading into non-string variable '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    eat(prs, TOK_ID);
    
    AST *ast = create_ast(AST_READ, ln, col);
    ast->read.fd = create_ast(AST_VAR, ln, col);
    ast->read.fd->var.name = mystrdup(var->name);
    ast->read.fd->var.sym = var;
    ast->read.into = create_ast(AST_VAR, ln, col);
    ast->read.into->var.name = mystrdup(into->name);
    ast->read.into->var.sym = into;
    ast->read.at_end_stmts = create_astlist();
    ast->read.not_at_end_stmts = create_astlist();

    if (strcmp(prs->tok->value, "AT") != 0)
        goto done_at;

    eat(prs, TOK_ID);

    if (!expect_identifier(prs, "END")) {
        eat_until(prs, TOK_DOT);
        goto done_at;
    }

    eat(prs, TOK_ID);

    // Make this an ASTList instead of a single AST because some statements
    // can return multiple AST nodes.
    astlist_push(&ast->read.at_end_stmts, parse_procedure_stmt(prs, &ast->read.at_end_stmts));

done_at:

    if (strcmp(prs->tok->value, "NOT") != 0)
        return ast;

    eat(prs, TOK_ID);

    if (!expect_identifier(prs, "AT")) {
        eat_until(prs, TOK_DOT);
        return ast;
    }

    eat(prs, TOK_ID);

    if (!expect_identifier(prs, "END")) {
        eat_until(prs, TOK_DOT);
        return ast;
    }

    eat(prs, TOK_ID);
    astlist_push(&ast->read.not_at_end_stmts, parse_procedure_stmt(prs, &ast->read.not_at_end_stmts));
    return ast;
}

AST *parse_write(Parser *prs) {
    AST *ast = create_ast(AST_WRITE, prs->tok->ln, prs->tok->col);
    eat(prs, TOK_ID);
    ast->write.value = parse_value(prs, TYPE_ANY);
    return ast;
}

#define NOPHASE (StringTallyPhase1){ .value = NOP(0, 0), .modifier = NULL }
#define NOTALLY (StringTally){ .type = TALLY_ALL, .output_count = NOP(0, 0), .phase = NOPHASE }

StringTallyPhase1 parse_stringtally_phase1(Parser *prs) {
    StringTallyPhase1 phase = (StringTallyPhase1){ .before = false, .after = false, .modifier = NULL, .value = parse_value(prs, TYPE_ANY) };

    if (strcmp(prs->tok->value, "BEFORE") == 0) {
        phase.before = true;
        eat(prs, TOK_ID);

        if (expect_identifier(prs, "INITIAL")) {
            eat(prs, TOK_ID);
            phase.modifier = parse_value(prs, TYPE_ANY);
        } else
            eat_until(prs, TOK_DOT);
    } else if (strcmp(prs->tok->value, "AFTER") == 0) {
        phase.after = true;
        eat(prs, TOK_ID);

        if (expect_identifier(prs, "INITIAL")) {
            eat(prs, TOK_ID);
            phase.modifier = parse_value(prs, TYPE_ANY);
        } else
            eat_until(prs, TOK_DOT);
    }

    return phase;
}

StringTally parse_stringtally(Parser *prs) {
    Variable *output = find_variable(prs->file, prs->tok->value);

    if (!output->used) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "undefined variable '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOTALLY;
    } else if ((output->type.type != TYPE_DECIMAL_NUMERIC && output->type.type != TYPE_SIGNED_NUMERIC && output->type.type != TYPE_UNSIGNED_NUMERIC &&
            output->type.type != TYPE_SIGNED_SUPRESSED_NUMERIC && output->type.type != TYPE_UNSIGNED_SUPRESSED_NUMERIC && output->type.type != TYPE_DECIMAL_SUPRESSED_NUMERIC) 
            || output->count > 0) {

        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "tallying non-integer variable '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOTALLY;
    }

    eat(prs, TOK_ID);

    if (!expect_identifier(prs, "FOR")) {
        eat_until(prs, TOK_DOT);
        return NOTALLY;
    }

    eat(prs, TOK_ID);

    AST *var = create_ast(AST_VAR, prs->tok->ln, prs->tok->col);
    var->var.name = mystrdup(output->name);
    var->var.sym = output;

    StringTally tally;

    if (strcmp(prs->tok->value, "CHARACTERS") == 0) {
        eat(prs, TOK_ID);
        return (StringTally){ .type = TALLY_CHARACTERS, .output_count = var, .phase = NOPHASE };
    } else if (strcmp(prs->tok->value, "ALL") == 0)
        tally.type = TALLY_ALL;
    else {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "invalid TALLYING statement '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOTALLY;
    }

    eat(prs, TOK_ID);

    tally.output_count = var;
    tally.phase = parse_stringtally_phase1(prs);
    return tally;
}

#define NOREPLACE (StringReplace){ .new = NOP(0, 0), .old = NOP(0, 0), .modifier = NULL, .type = 0 }

StringReplace parse_stringreplace(Parser *prs) {
    StringReplace replace;
    replace.modifier = NULL;
    replace.before = replace.after = false;

    if (strcmp(prs->tok->value, "ALL") == 0)
        replace.type = REPLACING_ALL;
    else if (strcmp(prs->tok->value, "FIRST") == 0)
        replace.type = REPLACING_FIRST;
    else {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "invalid REPLACING value '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOREPLACE;
    }

    eat(prs, TOK_ID);

    replace.old = parse_value(prs, TYPE_ANY);

    if (!expect_identifier(prs, "BY")) {
        delete_ast(replace.old);
        eat_until(prs, TOK_DOT);
        return NOREPLACE;
    }

    eat(prs, TOK_ID);
    replace.new = parse_value(prs, TYPE_ANY);

    if (strcmp(prs->tok->value, "BEFORE") == 0) {
        eat(prs, TOK_ID);

        if (expect_identifier(prs, "INITIAL")) {
            eat(prs, TOK_ID);
            replace.before = true;
            replace.modifier = parse_value(prs, TYPE_ANY);
        } else
            eat_until(prs, TOK_DOT);
    } else if (strcmp(prs->tok->value, "AFTER") == 0) {
        eat(prs, TOK_ID);

        if (expect_identifier(prs, "INITIAL")) {
            eat(prs, TOK_ID);
            replace.after = true;
            replace.modifier = parse_value(prs, TYPE_ANY);
        } else
            eat_until(prs, TOK_DOT);
    }

    return replace;
}

InspectTallying parse_tallying(Parser *prs) {
    StringTally *tallies = malloc(4 * sizeof(StringTally));
    size_t tally_count = 0;
    size_t tally_capacity = 4;

    while (prs->tok->type != TOK_EOF && strcmp(peek(prs, 1)->value, "FOR") == 0) {
        if (tally_count + 1 >= tally_capacity) {
            tally_capacity *= 2;
            tallies = realloc(tallies, tally_capacity * sizeof(StringTally));
        }

        tallies[tally_count++] = parse_stringtally(prs);
    }

    return (InspectTallying){ .tallies = tallies, .tally_count = tally_count, .tally_capacity = tally_capacity };
}

InspectReplacing parse_replacing(Parser *prs) {
    StringReplace *replaces = malloc(4 * sizeof(StringReplace));
    size_t replace_count = 0;
    size_t replace_capacity = 4;

    while (prs->tok->type != TOK_EOF && (strcmp(prs->tok->value, "ALL") == 0 || strcmp(prs->tok->value, "FIRST") == 0)) { 
        if (replace_count + 1 >= replace_capacity) {
            replace_capacity *= 2;
            replaces = realloc(replaces, replace_capacity * sizeof(StringReplace));
        }

        replaces[replace_count++] = parse_stringreplace(prs);
    }

    return (InspectReplacing){ .replaces = replaces, .replace_count = replace_count, .replace_capacity = replace_capacity };
}


AST *parse_inspect_tallying(Parser *prs, Variable *input_string, const size_t ln, const size_t col) {
    AST *ast = create_ast(AST_INSPECT, ln, col);
    ast->inspect.type = INSPECT_TALLYING;
    ast->inspect.input_string = create_ast(AST_VAR, ln, col);
    ast->inspect.input_string->var.name = mystrdup(input_string->name);
    ast->inspect.input_string->var.sym = input_string;
    ast->inspect.tallying = parse_tallying(prs);
    return ast;
}

AST *parse_inspect_replacing(Parser *prs, Variable *input_string, const size_t ln, const size_t col) {
    AST *ast = create_ast(AST_INSPECT, ln, col);
    ast->inspect.type = INSPECT_REPLACING;
    ast->inspect.input_string = create_ast(AST_VAR, ln, col);
    ast->inspect.input_string->var.name = mystrdup(input_string->name);
    ast->inspect.input_string->var.sym = input_string;
    ast->inspect.replacing = parse_replacing(prs);
    return ast;
}

AST *parse_inspect(Parser *prs) {
    const size_t ln = prs->tok->ln;
    const size_t col = prs->tok->col;
    eat(prs, TOK_ID);

    Variable *var = find_variable(prs->file, prs->tok->value);

    if (!var->used) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "undefined variable '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    } else if ((var->type.type != TYPE_ALPHABETIC && var->type.type != TYPE_ALPHANUMERIC) ||
            var->type.count == 0) {

        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "inspecting non-string variable '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    eat(prs, TOK_ID);

    if (strcmp(prs->tok->value, "TALLYING") == 0) {
        eat(prs, TOK_ID);
        return parse_inspect_tallying(prs, var, ln, col);
    } else if (strcmp(prs->tok->value, "REPLACING") == 0) {
        eat(prs, TOK_ID);
        return parse_inspect_replacing(prs, var, ln, col);
    }

    log_error(prs->file, prs->tok->ln, prs->tok->col);
    fprintf(stderr, "invalid INSPECT statement '%s'\n", prs->tok->value);
    show_error(prs->file, prs->tok->ln, prs->tok->col);

    eat_until(prs, TOK_DOT);
    return NOP(ln, col);
}

AST *parse_accept(Parser *prs) {
    const size_t ln = prs->tok->ln;
    const size_t col = prs->tok->col;
    eat(prs, TOK_ID);

    /*
    Variable *var = find_variable(prs->file, prs->tok->value);

    if (!var->used) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "undefined variable '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    } else if ((var->type.type != TYPE_ALPHABETIC && var->type.type != TYPE_ALPHANUMERIC) ||
            var->type.count == 0) {

        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "accepting into non-string variable '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }
    */

    AST *ast = create_ast(AST_ACCEPT, ln, col);
    ast->accept.dst = parse_value(prs, TYPE_ANY);
    //ast->accept.dst = create_ast(AST_VAR, ln, col);
    //ast->accept.dst->var.name = mystrdup(prs->tok->value);
    //ast->accept.dst->var.sym = var;
    //eat(prs, TOK_ID);

    if (strcmp(prs->tok->value, "FROM") != 0) {
        ast->accept.from = NULL;
        return ast;
    }

    eat(prs, TOK_ID);

    if (strcmp(prs->tok->value, "COMMAND-LINE") == 0) {
        eat(prs, TOK_ID);
        ast->accept.from = create_ast(AST_ARGV, prs->tok->ln, prs->tok->col);
    } else {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "invalid ACCEPT FROM value '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        ast->accept.from = NULL;
        eat_until(prs, TOK_DOT);
    }

    return ast;
}

AST *parse_exit(Parser *prs) {
    eat(prs, TOK_ID);

    if (!expect_identifier(prs, "PROGRAM")) {
        eat_until(prs, TOK_DOT);
        return NOP(prs->tok->ln, prs->tok->col);
    }

    eat(prs, TOK_ID);
    return create_ast(AST_EXIT, prs->tok->ln, prs->tok->col);
}

AST *parse_not(Parser *prs);

AST *parse_id(Parser *prs) {
    const size_t ln = prs->tok->ln;
    const size_t col = prs->tok->col;

    if (strcmp(prs->tok->value, "NULL") == 0) {
        eat(prs, TOK_ID);
        return create_ast(AST_NULL, ln, col);
    } else if (strcmp(prs->tok->value, "TRUE") == 0 || strcmp(prs->tok->value, "FALSE") == 0) {
        AST *ast = create_ast(AST_BOOL, ln, col);
        ast->bool_value = strcmp(prs->tok->value, "TRUE") == 0;
        eat(prs, TOK_ID);
        return ast;
    } else if (strcmp(prs->tok->value, "ZERO") == 0) {
        eat(prs, TOK_ID);
        return create_ast(AST_ZERO, ln, col);
    } else if (strcmp(prs->tok->value, "NOT") == 0)
        return parse_not(prs);
    else if (strcmp(prs->tok->value, "SPACE") == 0) {
        eat(prs, TOK_ID);
        AST *ast = create_ast(AST_INT, ln, col);
        ast->constant.i32 = ' ';
        return ast;
    } else if (strcmp(prs->tok->value, "LENGTH") == 0 || strcmp(prs->tok->value, "ADDRESS") == 0) {
        const bool is_length = strcmp(prs->tok->value, "LENGTH") == 0;
        eat(prs, TOK_ID);

        if (!expect_identifier(prs, "OF"))
            return NOP(ln, col);

        eat(prs, TOK_ID);

        // Set prs->parse_extra_value to false because we don't want
        // other values getting inside strlen().
        // For example, LENGTH OF "lsdlk" - 2
        // We don't want the -2 to be part of the LENGTH OF.
        // We do allow subscripts though because it's unlikely that they're
        // used in the wrong place, and we may be subscripting a field or something.

        bool before = prs->parse_extra_value;
        prs->parse_extra_value = false;

        AST *ast;
        
        if (is_length) {
            ast = create_ast(AST_LENGTHOF, ln, col);
            ast->lengthof_value = parse_value(prs, TYPE_ANY);

            if (prs->tok->type == TOK_LPAREN)
                ast->lengthof_value = parse_subscript(prs, ast->lengthof_value);
        } else {
            ast = create_ast(AST_ADDRESSOF, ln, col);
            ast->addressof_value = parse_value(prs, TYPE_ANY);

            if (prs->tok->type == TOK_LPAREN)
                ast->addressof_value = parse_subscript(prs, ast->addressof_value);
        }

        prs->parse_extra_value = before;
        return ast;
    }

    Variable *sym;

    if ((sym = find_variable(prs->file, prs->tok->value))->used) {
        if (sym->is_label) {
            AST *ast = create_ast(AST_LABEL, ln, col);
            ast->label = mystrdup(prs->tok->value);
            eat(prs, TOK_ID);
            return ast;
        } else if (sym->struct_sym != NULL) {
            AST *ast = create_ast(AST_FIELD, ln, col);
            ast->field.base = create_ast(AST_VAR, ln, col);
            ast->field.base->var.name = mystrdup(sym->struct_sym->name);
            ast->field.base->var.sym = sym->struct_sym;
            ast->field.sym = sym;
            ast->field.value = NULL;
            eat(prs, TOK_ID);
            return ast;
        }

        AST *ast = create_ast(AST_VAR, ln, col);
        ast->var.name = mystrdup(prs->tok->value);
        ast->var.sym = sym;
        eat(prs, TOK_ID);

        if (sym->is_linkage_src && !sym->using_in_proc_div) {
            log_error(prs->file, ln, col);
            fprintf(stderr, "linkage variable '%s' missing USING statement in PROCEDURE DIVISION\n", ast->var.name);
            show_error(prs->file, ln, col);
        }

        if (prs->tok->type == TOK_LPAREN)
            return parse_subscript(prs, ast);

        return ast;
    }

    log_error(prs->file, ln, col);
    fprintf(stderr, "undefined identifier '%s'\n", prs->tok->value);
    show_error(prs->file, ln, col);
    eat(prs, TOK_ID);
    return NOP(ln, col);
}

AST *parse_procedure_stmt(Parser *prs, ASTList *root) {
    if (prs->tok->type == TOK_ID && peek(prs, 1)->type == TOK_DOT)
        return parse_procedure(prs);

    while (prs->tok->type == TOK_DOT)
        eat(prs, TOK_DOT);

    if (prs->tok->type == TOK_EOF)
        return NOP(prs->tok->ln, prs->tok->col);

    if (strcmp(prs->tok->value, "STOP") == 0) {
        eat(prs, TOK_ID);

        if (expect_identifier(prs, "RUN"))
            eat(prs, TOK_ID);

        return create_ast(prs->in_main ? AST_STOP_RUN : AST_STOP, prs->tok->ln, prs->tok->col);
    } else if (strcmp(prs->tok->value, "DISPLAY") == 0) {
        displayed_previously = false;
        first_display = true;
        return parse_display(prs, root);
    } else if (strcmp(prs->tok->value, "MOVE") == 0)
        return parse_move(prs);
    else if (strcmp(prs->tok->value, "ADD") == 0 || strcmp(prs->tok->value, "SUBTRACT") == 0 || 
            strcmp(prs->tok->value, "MULTIPLY") == 0 || strcmp(prs->tok->value, "DIVIDE") == 0)
        return parse_arithmetic(prs);
    else if (strcmp(prs->tok->value, "COMPUTE") == 0)
        return parse_compute(prs);
    else if (strcmp(prs->tok->value, "IF") == 0)
        return parse_if(prs);
    else if (strcmp(prs->tok->value, "NOT") == 0)
        return parse_not(prs);
    else if (strcmp(prs->tok->value, "END") == 0) {
        eat(prs, TOK_ID);

        if (expect_identifier(prs, "PROGRAM")) {
            eat(prs, TOK_ID);

            if (expect_identifier(prs, prs->program_id))
                eat(prs, TOK_ID);
        } else
            eat_until(prs, TOK_DOT);

        return NOP(prs->tok->ln, prs->tok->col);
    } else if (strcmp(prs->tok->value, "PERFORM") == 0)
        return parse_perform(prs);
    else if (strcmp(prs->tok->value, "SET") == 0)
        return parse_set(prs);
    else if (strcmp(prs->tok->value, "CALL") == 0)
        return parse_call(prs);
    else if (strcmp(prs->tok->value, "STRING") == 0)
        return parse_string_builder(prs);
    else if (strcmp(prs->tok->value, "UNSTRING") == 0)
        return parse_unstring(prs);
    else if (strcmp(prs->tok->value, "OPEN") == 0)
        return parse_open(prs);
    else if (strcmp(prs->tok->value, "CLOSE") == 0)
        return parse_close(prs);
    else if (strcmp(prs->tok->value, "READ") == 0)
        return parse_read(prs);
    else if (strcmp(prs->tok->value, "WRITE") == 0)
        return parse_write(prs);
    else if (strcmp(prs->tok->value, "INSPECT") == 0)
        return parse_inspect(prs);
    else if (strcmp(prs->tok->value, "ACCEPT") == 0)
        return parse_accept(prs);
    else if (strcmp(prs->tok->value, "EXIT") == 0)
        return parse_exit(prs);

    log_error(prs->file, prs->tok->ln, prs->tok->col);
    fprintf(stderr, "invalid clause '%s' in PROCEDURE DIVISION\n", prs->tok->value);
    show_error(prs->file, prs->tok->ln, prs->tok->col);
    eat_until(prs, TOK_DOT);
    return NOP(prs->tok->ln, prs->tok->col);
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
        ast->constant.i32 = strtoll(prs->tok->value, &endptr, 10);
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

/*
unsigned int v99_to_num(Parser *prs) {
    char *ptr = prs->tok->value;
    char buffer[32];
    size_t buffer_len = 0;

    while (*ptr && buffer_len < 32) {
        if (*ptr != 'V')
            buffer[buffer_len++] = *ptr;

        ptr++;
    }

    buffer[buffer_len] = '\0';

    char *endptr;
    errno = 0;
    long num = strtol(buffer, &endptr, 10);

    if (endptr == prs->tok->value || *endptr != '\0') {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "digit conversion failed\n");
        show_error(prs->file, prs->tok->ln, prs->tok->col);
        num = 1;
    } else if (errno == ERANGE || errno == EINVAL) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "digit conversion failed: %s\n", strerror(errno));
        show_error(prs->file, prs->tok->ln, prs->tok->col);
        num = 1;
    }

    return num;
}
    */

unsigned int parse_comptype(Parser *prs, AST *ast) {
    // ast is the existing PIC clause from parse_pic().
    // This is not used when called from parse_comp_pic().

    // COMP-1, COMP-2, COMP-3 and COMP-6 are not allowed in PIC clauses.

    if (strcmp(prs->tok->value, "COMP") == 0 || strcmp(prs->tok->value, "COMP-4") == 0 || strcmp(prs->tok->value, "BINARY") == 0) {
        if (ast != NULL && ast->pic.type.decimal_places > 0) {
            log_error(prs->file, prs->tok->ln, prs->tok->col);
            fprintf(stderr, "COMP type '%s' used for non-integer variable '%s'\n", prs->tok->value, ast->pic.name);
            show_error(prs->file, prs->tok->ln, prs->tok->col);
        }

        return COMP4;
    } else if (strcmp(prs->tok->value, "COMP-1") == 0) {
        if (ast != NULL) {
            log_error(prs->file, prs->tok->ln, prs->tok->col);
            fprintf(stderr, "COMP type '%s' used in a PIC clause for variable '%s'\n", prs->tok->value, ast->pic.name);
            show_error(prs->file, prs->tok->ln, prs->tok->col);
        }

        return COMP1;
    } else if (strcmp(prs->tok->value, "COMP-2") == 0) {
        if (ast != NULL) {
            log_error(prs->file, prs->tok->ln, prs->tok->col);
            fprintf(stderr, "COMP type '%s' used in a PIC clause for variable '%s'\n", prs->tok->value, ast->pic.name);
            show_error(prs->file, prs->tok->ln, prs->tok->col);
        }

        return COMP2;
    } else if (strcmp(prs->tok->value, "COMP-5") == 0)
        return COMP5;
    else if (strcmp(prs->tok->value, "POINTER") == 0)
        return COMP_POINTER;
    else {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "invalid COMP type '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);
        return 0;
    }

    return 0;
}

AST *parse_pic(Parser *prs) {
    AST *level = parse_constant(prs);

    if (level->constant.i32 < 1 || level->constant.i32 > 49) {
        log_error(level->file, level->ln, level->col);
        fprintf(stderr, "invalid level '%d'; level not within 1-49\n", level->constant.i32);
        show_error(level->file, level->ln, level->col);
    }

    char *name = mystrdup(prs->tok->value);
    AST *ast = create_ast(AST_PIC, prs->tok->ln, prs->tok->col);
    eat(prs, TOK_ID);
    ast->pic.level = level->constant.i32;
    delete_ast(level);
    ast->pic.name = name;
    ast->pic.is_index = ast->pic.is_fd = ast->pic.is_linkage_src = false;
    ast->pic.count = 0;
    ast->pic.fields = create_astlist();

    bool had_pic = false;

    // We may be defining a record or a following USAGE IS statement.
    if (strcmp(prs->tok->value, "PIC") == 0) {
        had_pic = true;
        eat(prs, TOK_ID); // PIC

        // A = alphabetical, X = alphanumeric, 9 = numeric
        PictureType type = (PictureType){ .type = TYPE_ANY, .count = 0, .places = 1, .decimal_places = 0 };
        bool implicit_count = false;

        if (strcmp(prs->tok->value, "A") == 0) {
            type.type = TYPE_ALPHABETIC;
            eat(prs, prs->tok->type);
        } else if (strcmp(prs->tok->value, "X") == 0) {
            type.type = TYPE_ALPHANUMERIC;
            eat(prs, prs->tok->type);
        } else if (strcmp(prs->tok->value, "9") == 0) {
            type.type = TYPE_UNSIGNED_NUMERIC;
            eat(prs, prs->tok->type);
        } else if (strcmp(prs->tok->value, "SZ") == 0 || strcmp(prs->tok->value, "SZ9") == 0) {
            type.type = TYPE_SIGNED_SUPRESSED_NUMERIC;
            eat(prs, prs->tok->type);
        } else if (strcmp(prs->tok->value, "9V9") == 0) {
            type.type = TYPE_UNSIGNED_NUMERIC;
            eat(prs, prs->tok->type);
            implicit_count = true;
        } else if (strcmp(prs->tok->value, "ZV9") == 0) {
            type.type = TYPE_DECIMAL_SUPRESSED_NUMERIC;
            eat(prs, prs->tok->type);
            implicit_count = true;
        } else if (strcmp(prs->tok->value, "S9") == 0) {
            type.type = TYPE_SIGNED_NUMERIC;
            eat(prs, prs->tok->type);
        } else if (strcmp(prs->tok->value, "Z9") == 0) {
            type.type = TYPE_UNSIGNED_SUPRESSED_NUMERIC;
            eat(prs, prs->tok->type);
        } else if (strcmp(prs->tok->value, "S9V9") == 0) {
            type.type = TYPE_SIGNED_NUMERIC;
            eat(prs, prs->tok->type);
            implicit_count = true;
        } else if (strcmp(prs->tok->value, "SZ9V9") == 0) {
            type.type = TYPE_DECIMAL_SUPRESSED_NUMERIC;
            eat(prs, prs->tok->type);
        } else {
            log_error(prs->file, prs->tok->ln, prs->tok->col);
            fprintf(stderr, "invalid picture type '%s'\n", prs->tok->value);
            show_error(prs->file, prs->tok->ln, prs->tok->col);
            type.type = TYPE_ALPHANUMERIC;
        }

        ast->pic.type = type;

        if (prs->tok->type == TOK_LPAREN && !implicit_count) {
            eat(prs, TOK_LPAREN);

            if (prs->tok->type != TOK_INT) {
                log_error(prs->file, prs->tok->ln, prs->tok->col);
                fprintf(stderr, "non constant integer picture length '%s'\n", tokentype_to_string(prs->tok->type));
                show_error(prs->file, prs->tok->ln, prs->tok->col);

                eat_until(prs, TOK_DOT);
            } else {
                AST *size = parse_constant(prs);

                if (size->constant.i32 < 1) {
                    log_error(size->file, size->ln, size->col);
                    fprintf(stderr, "found place count '%d' but minimum place count is 1\n", size->constant.i32);
                    show_error(size->file, size->ln, size->col);
                } else
                    ast->pic.type.places = (unsigned int)size->constant.i32;

                delete_ast(size);

                // Strings are kinda like tables so the place count is the character count yk.
                if (type.type == TYPE_ALPHABETIC || type.type == TYPE_ALPHANUMERIC)
                    ast->pic.type.count = ast->pic.type.places;
            }

            eat(prs, TOK_RPAREN);
        }
    
        if (strcmp(prs->tok->value, "V9") == 0 || implicit_count) {
            if (type.type != TYPE_SIGNED_NUMERIC && type.type != TYPE_SIGNED_SUPRESSED_NUMERIC) {
                log_error(prs->file, ast->ln, ast->col);
                fprintf(stderr, "declaring variable '%s' with decimal points but it is unsigned\n", name);
                show_error(prs->file, ast->ln, ast->col);
            }

            ast->pic.type.type = type.type == TYPE_SIGNED_SUPRESSED_NUMERIC || type.type == TYPE_UNSIGNED_SUPRESSED_NUMERIC ?
                TYPE_DECIMAL_SUPRESSED_NUMERIC : TYPE_DECIMAL_NUMERIC;

            if (!implicit_count)
                eat(prs, TOK_ID);

            if (prs->tok->type == TOK_LPAREN) {
                eat(prs, TOK_LPAREN);

                AST *size = parse_constant(prs);
                ast->pic.type.decimal_places = (unsigned int)size->constant.i32;
                delete_ast(size);

                eat(prs, TOK_RPAREN);

                if (ast->pic.type.decimal_places < 1) {
                    log_error(prs->file, prs->tok->ln, prs->tok->col);
                    fprintf(stderr, "found place count '%d' but minimum place count is 1\n", ast->pic.type.decimal_places);
                    show_error(prs->file, prs->tok->ln, prs->tok->col);
                }
            } else
                ast->pic.type.decimal_places = 1;
        }
    }

    if (strcmp(prs->tok->value, "USAGE") == 0 && strcmp(peek(prs, 1)->value, "IS") == 0) {
        eat(prs, TOK_ID);
        eat(prs, TOK_ID);

        if (IS_COMP(prs->tok))
            ast->pic.type.comp_type = parse_comptype(prs, ast);
        else {
            log_error(prs->file, prs->tok->ln, prs->tok->col);
            fprintf(stderr, "invalid USAGE type '%s' for variable '%s'\n", prs->tok->value, ast->pic.name);
            show_error(prs->file, prs->tok->ln, prs->tok->col);
            ast->pic.type.comp_type = 0;
        }

        eat(prs, TOK_ID);
    } else if (!had_pic) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "expected PIC or USAGE clause for variable '%s' but found '%s'\n", ast->pic.name, prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);
        ast->pic.type = (PictureType){ .type = TYPE_SIGNED_NUMERIC, .count = 0 };
    }

    if (strcmp(prs->tok->value, "VALUE") == 0) {
        eat(prs, TOK_ID);
        AST *value = parse_value(prs, ast->pic.type.type);
        ast->pic.value = value;

        if (value->type == AST_STRING && (ast->pic.type.type == TYPE_ALPHABETIC || ast->pic.type.type == TYPE_ALPHANUMERIC) &&
                strlen(value->constant.string) > ast->pic.type.count) {

            log_error(value->file, value->ln, value->col);
            fprintf(stderr, "value string of length %zu exceeds place count of %u in variable '%s'\n", strlen(value->constant.string), ast->pic.type.count, name);
            show_error(value->file, value->ln, value->col);
        }
    } else
        ast->pic.value = NULL;

    if (strcmp(prs->tok->value, "OCCURS") == 0) {
        eat(prs, TOK_ID);

        if (prs->tok->type != TOK_INT) {
            log_error(prs->file, prs->tok->ln, prs->tok->col);
            fprintf(stderr, "expected constant integer for table size but found '%s'\n", tokentype_to_string(prs->tok->type));
            show_error(prs->file, prs->tok->ln, prs->tok->col);
            eat_until(prs, TOK_DOT);
        } else {
            AST *size = parse_constant(prs);
            ast->pic.count = (unsigned int)size->constant.i32;
            delete_ast(size);
        }

        if (expect_identifier(prs, "TIMES")) {
            eat(prs, TOK_ID);

            if (strcmp(prs->tok->value, "INDEXED") != 0) 
                goto done;
            
            eat(prs, TOK_ID);

            if (!expect_identifier(prs, "BY"))
                goto done;

            eat(prs, TOK_ID);

            if (!expect_identifier(prs, NULL))
                goto done;

            Variable *index_var = find_variable(prs->file, prs->tok->value);

            // TODO: Add file/ln/col info to Variable for logging stuff you idiot?
            if (index_var->used) {
                log_error(prs->file, prs->tok->ln, prs->tok->col);
                fprintf(stderr, "redefinition of index variable '%s'\n", prs->tok->value);
                show_error(prs->file, prs->tok->ln, prs->tok->col);
                eat(prs, TOK_ID);
                goto done;
            }

            AST *pic = create_ast(AST_PIC, prs->tok->ln, prs->tok->col);
            pic->pic.name = mystrdup(prs->tok->value);
            pic->pic.level = ast->pic.level;
            pic->pic.fields = create_astlist();

            // Initialize to first index = 1.
            pic->pic.value = create_ast(AST_INT, prs->tok->ln, prs->tok->col);
            pic->pic.value->constant.i32 = 1;

            pic->pic.type.type = TYPE_UNSIGNED_SUPRESSED_NUMERIC;
            pic->pic.type.count = 0;
            pic->pic.type.places = 0;
            pic->pic.count = 0;
            pic->pic.is_index = true;
            pic->pic.is_fd = pic->pic.is_linkage_src = false;

            Variable *var = add_variable(pic->file, pic->pic.name, pic->pic.type, pic->pic.count);
            var->is_index = true;
            eat(prs, TOK_ID);
            astlist_push(root_ptr, pic);
        }
    }

// Bunch of stuff that could possibly be parsed, don't want to
// repeat code or make new functions, a goto here is useful.
done: ;
    Variable *v = add_variable(ast->file, ast->pic.name, ast->pic.type, ast->pic.count);

    if (prs->cur_sect == SECT_LINKAGE) {
        v->is_linkage_src = ast->pic.is_linkage_src = true;

        if (ast->pic.value != NULL) {
            log_error(prs->file, ast->ln, ast->col);
            fprintf(stderr, "value assigned to linkage variable '%s'\n", ast->pic.name);
            show_error(prs->file, ast->ln, ast->col);
        }
    }

    return ast;
}

AST *parse_comp_pic(Parser *prs) {
    // For declaring data with only a USAGE IS, missing the PIC.

    AST *level = parse_constant(prs);

    if (level->constant.i32 < 1 || level->constant.i32 > 49) {
        log_error(level->file, level->ln, level->col);
        fprintf(stderr, "invalid level '%d'; level not within 1-49\n", level->constant.i32);
        show_error(level->file, level->ln, level->col);
    }

    char *name = mystrdup(prs->tok->value);
    eat(prs, TOK_ID);

    AST *ast = create_ast(AST_PIC, prs->tok->ln, prs->tok->col);
    ast->pic.level = level->constant.i32;
    delete_ast(level);
    ast->pic.name = name;
    ast->pic.is_index = ast->pic.is_fd = ast->pic.is_linkage_src = false;
    ast->pic.count = 0;
    ast->pic.fields = create_astlist();

    eat(prs, TOK_ID); // USAGE
    eat(prs, TOK_ID); // IS

    ast->pic.type.count = ast->pic.count = 0;
    ast->pic.type.comp_type = parse_comptype(prs, NULL);

    // Floats, doubles and pointers don't require PIC, but everything else does.
    if (ast->pic.type.comp_type != COMP_POINTER && ast->pic.type.comp_type != COMP1 && ast->pic.type.comp_type != COMP2) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "missing PIC clause for COMP type '%s' for variable '%s'\n", prs->tok->value, ast->pic.name);
        show_error(prs->file, prs->tok->ln, prs->tok->col);
        ast->pic.type.type = TYPE_SIGNED_NUMERIC;
    }

    eat(prs, TOK_ID);

    if (strcmp(prs->tok->value, "VALUE") == 0) {
        eat(prs, TOK_ID);
        ast->pic.value = parse_value(prs, TYPE_ANY);
    } else
        ast->pic.value = NULL;

    Variable *v = add_variable(ast->file, ast->pic.name, ast->pic.type, ast->pic.count);

    if (prs->cur_sect == SECT_LINKAGE)
        v->is_linkage_src = true;

    return ast;
}

AST *parse_struct_pic(Parser *prs) {
    AST *level = parse_constant(prs);
    unsigned int level_digit = level->constant.i32;
    delete_ast(level);

    Variable *var = find_variable(prs->file, prs->tok->value);

    if (var->used) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "redefinition of variable '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(prs->tok->ln, prs->tok->col);
    }

    AST *ast = create_ast(AST_PIC, prs->tok->ln, prs->tok->col);
    ast->pic.level = level_digit;
    ast->pic.name = mystrdup(prs->tok->value);
    eat(prs, TOK_ID);
    ast->pic.type = (PictureType){ .type = TYPE_POINTER, .count = 0, .places = 0 };
    ast->pic.count = 0;
    ast->pic.is_fd = ast->pic.is_index = ast->pic.is_linkage_src = false;
    ast->pic.value = NULL;
    ast->pic.fields = create_astlist();

    if (strcmp(prs->tok->value, "OCCURS") == 0) {
        eat(prs, TOK_ID);

        if (prs->tok->type != TOK_INT) {
            log_error(prs->file, prs->tok->ln, prs->tok->col);
            fprintf(stderr, "non-integer constant table size %s for variable '%s'\n", tokentype_to_string(prs->tok->type), ast->pic.name);
            show_error(prs->file, prs->tok->ln, prs->tok->col);

            eat_until(prs, TOK_DOT);
        } else {
            AST *size = parse_constant(prs);
            ast->pic.count = size->constant.i32;
            delete_ast(size);

            if (expect_identifier(prs, "TIMES"))
                eat(prs, TOK_ID);
        }
    }

    eat(prs, TOK_DOT);
    var = add_variable(prs->file, ast->pic.name, ast->pic.type, ast->pic.count);

    // Place upcoming lower-level PICs inside this struct.
    while (prs->tok->type == TOK_INT && peek(prs, 1)->type == TOK_ID) {
        size_t before = prs->pos;
        AST *field_level = parse_constant(prs);
        jump_to(prs, before);

        if ((unsigned int)field_level->constant.i32 <= ast->pic.level) {
            delete_ast(field_level);
            break;
        }

        delete_ast(field_level);

        Token *next2 = peek(prs, 2);
        AST *field;

        if (next2->type == TOK_DOT || strcmp(next2->value, "OCCURS") == 0)
            field = parse_struct_pic(prs);
        else if (strcmp(next2->value, "PIC") == 0)
            field = parse_pic(prs);
        else if (strcmp(next2->value, "USAGE") == 0)
            field = parse_comp_pic(prs);
        else {
            assert(false);
            break;
        }

        if (field->pic.value != NULL) {
            log_error(prs->file, field->ln, field->col);
            fprintf(stderr, "assigning a value to group item field '%s' on initialization\n", field->pic.name);
            show_error(prs->file, field->ln, field->col);
        }

        // Make sure this symbol is known to be a field by filling
        // in the struct pointer symbol.
        Variable *fsym = find_variable(field->file, field->pic.name);
        assert(fsym != NULL);
        fsym->struct_sym = var;

        astlist_push(&ast->pic.fields, field);
        eat(prs, TOK_DOT);
    }

    // Add the symbol and point the fields to the struct's fields.
    var->fields = &ast->pic.fields;
    return ast;
}

AST *parse_parens(Parser *prs) {
    AST *ast = create_ast(AST_PARENS, prs->tok->ln, prs->tok->col);
    eat(prs, TOK_LPAREN);
    ast->parens = parse_value(prs, TYPE_ANY);

    if (IS_MATH(prs))
        ast->parens = parse_math(prs, ast->parens, TYPE_SIGNED_NUMERIC);
    else if (IS_CONDITION(prs))
        ast->parens = parse_condition(prs, ast->parens);

    eat(prs, TOK_RPAREN);
    return ast;
}

AST *parse_program_id(Parser *prs) {
    eat(prs, TOK_ID);

    if (prs->tok->type == TOK_DOT)
        eat(prs, TOK_DOT);

    if (expect_identifier(prs, NULL)) {
        strncpy(prs->program_id, prs->tok->value, 64);
        prs->program_id[64] = '\0';
        eat(prs, TOK_ID);
    }

    return NOP(prs->tok->ln, prs->tok->col);
}

void parse_working_storage_section(Parser *prs);

void parse_copybook(Parser *prs, size_t ln, size_t col, char *filename, char *path) {
    // Make sure the path exists before attempting to parse and causing the program to exit
    // due to a missing file.
    FILE *f = fopen(path, "r");

    if (f == NULL) {
        log_error(prs->file, ln, col);
        fprintf(stderr, "no such copybook '%s' exists\n", filename);
        show_error(prs->file, ln, col);

        eat_until(prs, TOK_DOT);
        return;
    }

    // Cache then restore cur_file.
    char *file_cache = mystrdup(cur_file);
    ASTList *root_cache = root_ptr;

    free(cur_file);
    cur_file = mystrdup(filename);

    // This will add symbols to the symbol table and push ASTs
    // to the already assigned root_ptr variable in parse_root().
    Parser cbprs = create_parser(path, NULL);
    parse_working_storage_section(&cbprs);
    delete_parser(&cbprs);

    free(cur_file);
    cur_file = file_cache;
    root_ptr = root_cache;
}

AST *parse_copy(Parser *prs) {
    const size_t ln = prs->tok->ln;
    const size_t col = prs->tok->col;
    eat(prs, TOK_ID);

    if (!expect_identifier(prs, NULL)) {
        eat_until(prs, TOK_DOT);
        return NOP(ln, col);
    }

    char *file = malloc(strlen(cur_dir) + strlen(prs->tok->value) + 5);
    sprintf(file, "%s%s.CPY", cur_dir, prs->tok->value);

    const size_t name_ln = prs->tok->ln;
    const size_t name_col = prs->tok->col;
    eat(prs, TOK_ID);

    parse_copybook(prs, name_ln, name_col, file, file);
    free(file);
    return NOP(ln, col);
}

bool should_break_from(Parser *prs, char *header) {
    return prs->tok->type == TOK_EOF || strcmp(peek(prs, 1)->value, header) == 0;
}

void parse_identification_division(Parser *prs) {
    while (!should_break_from(prs, "DIVISION")) {
        while (prs->tok->type == TOK_DOT)
            eat(prs, TOK_DOT);

        if (should_break_from(prs, "DIVISION"))
            break;

        if (!expect_identifier(prs, NULL)) {
            eat_until(prs, TOK_DOT);

            while (prs->tok->type == TOK_DOT)
                eat(prs, TOK_DOT);

            continue;
        }

        if (should_break_from(prs, "DIVISION"))
            break;

        if (strcmp(prs->tok->value, "PROGRAM-ID") == 0)
            astlist_push(root_ptr, parse_program_id(prs));
        else {
            log_error(prs->file, prs->tok->ln, prs->tok->col);
            fprintf(stderr, "invalid clause '%s' in IDENTIFICATION DIVISION\n", prs->tok->value);
            show_error(prs->file, prs->tok->ln, prs->tok->col);
            eat_until(prs, TOK_DOT);
        }
    }
}

// This function looks pretty much exactly like parse_working_storage_section().
void parse_linkage_section(Parser *prs) {
    while (!should_break_from(prs, "DIVISION")) {
        while (prs->tok->type == TOK_DOT)
            eat(prs, TOK_DOT);

        if (should_break_from(prs, "DIVISION") || should_break_from(prs, "SECTION"))
            break;

        if (prs->tok->type == TOK_INT) {
            // Check if it's a PICTURE clause.
            Token *next = peek(prs, 1);
            Token *ahead = peek(prs, 2);
        
            if (next->type == TOK_ID && (ahead->type == TOK_DOT || strcmp(ahead->value, "OCCURS") == 0))
                astlist_push(root_ptr, parse_struct_pic(prs));
            else if (next->type == TOK_ID) {
                if (strcmp(ahead->value, "PIC") == 0)
                    astlist_push(root_ptr, parse_pic(prs));
                else if (strcmp(ahead->value, "USAGE") == 0)
                    astlist_push(root_ptr, parse_comp_pic(prs));
            }
        } else if (strcmp(prs->tok->value, "COPY") == 0)
            astlist_push(root_ptr, parse_copy(prs));
        else {
            log_error(prs->file, prs->tok->ln, prs->tok->col);
            fprintf(stderr, "invalid clause '%s' in LINKAGE SECTION\n", prs->tok->value);
            show_error(prs->file, prs->tok->ln, prs->tok->col);
            eat_until(prs, TOK_DOT);
        }
    }
}

void parse_working_storage_section(Parser *prs) {
    while (!should_break_from(prs, "DIVISION")) {
        while (prs->tok->type == TOK_DOT)
            eat(prs, TOK_DOT);

        if (should_break_from(prs, "DIVISION") || should_break_from(prs, "SECTION"))
            break;

        if (prs->tok->type == TOK_INT) {
            // Check if it's a PICTURE clause.
            Token *next = peek(prs, 1);
            Token *ahead = peek(prs, 2);
        
            if (next->type == TOK_ID && (ahead->type == TOK_DOT || strcmp(ahead->value, "OCCURS") == 0))
                astlist_push(root_ptr, parse_struct_pic(prs));
            else if (next->type == TOK_ID) {
                if (strcmp(ahead->value, "PIC") == 0)
                    astlist_push(root_ptr, parse_pic(prs));
                else if (strcmp(ahead->value, "USAGE") == 0)
                    astlist_push(root_ptr, parse_comp_pic(prs));
            }
        } else if (strcmp(prs->tok->value, "COPY") == 0)
            astlist_push(root_ptr, parse_copy(prs));
        else {
            log_error(prs->file, prs->tok->ln, prs->tok->col);
            fprintf(stderr, "invalid clause '%s' in WORKING-STORAGE SECTION\n", prs->tok->value);
            show_error(prs->file, prs->tok->ln, prs->tok->col);
            eat_until(prs, TOK_DOT);
        }
    }
}

AST *parse_fd(Parser *prs) {
    eat(prs, TOK_ID);

    // The WORKING-STORAGE SECTION should be parsed before the FILE SECTION,
    // so this should catch any redefinitions.
    Variable *var = find_variable(prs->file, prs->tok->value);

    if (var->used) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "redefinition of variable '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(prs->tok->ln, prs->tok->col);
    }

    AST *ast = create_ast(AST_PIC, prs->tok->ln, prs->tok->col);
    ast->pic.name = mystrdup(prs->tok->value);
    eat(prs, TOK_ID);
    ast->pic.is_index = false;
    ast->pic.level = 1;
    ast->pic.type = (PictureType){ .type = TYPE_UNSIGNED_NUMERIC, .places = 0, .decimal_places = 0, .count = 0 };
    ast->pic.value = create_ast(AST_NULL, prs->tok->ln, prs->tok->col);
    ast->pic.is_index = ast->pic.is_linkage_src = false;
    ast->pic.is_fd = true;
    ast->pic.fields = create_astlist();

    var = add_variable(prs->file, ast->pic.name, ast->pic.type, 0);
    var->is_index = var->is_label = false;
    var->is_fd = true;

    return ast;
}

AST *parse_select(Parser *prs) {
    const size_t ln = prs->tok->ln;
    const size_t col = prs->tok->col;
    eat(prs, TOK_ID);

    Variable *var = find_variable(prs->file, prs->tok->value);

    if (!var->used) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "undefined variable '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(prs->tok->ln, prs->tok->col);
    }

    eat(prs, TOK_ID);

    if (!expect_identifier(prs, "ASSIGN")) {
        eat_until(prs, TOK_DOT);
        return NOP(prs->tok->ln, prs->tok->col);
    }

    eat(prs, TOK_ID);

    if (!expect_identifier(prs, "TO")) {
        eat_until(prs, TOK_DOT);
        return NOP(prs->tok->ln, prs->tok->col);
    }

    eat(prs, TOK_ID);

    /*
    if (prs->tok->type != TOK_STRING) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "expected filename string but found '%s'\n", tokentype_to_string(prs->tok->type));
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        eat_until(prs, TOK_DOT);
        return NOP(prs->tok->ln, prs->tok->col);
    }

    char *filename = mystrdup(prs->tok->value);
    eat(prs, TOK_STRING);
    */
    AST *filename = parse_value(prs, TYPE_ANY);
    unsigned int organization = ORG_NONE;

    if (strcmp(prs->tok->value, "ORGANIZATION") == 0) {
        eat(prs, TOK_ID);

        if (!expect_identifier(prs, "IS")) {
            free(filename);
            eat_until(prs, TOK_DOT);
            return NOP(prs->tok->ln, prs->tok->col);
        }

        eat(prs, TOK_ID);

        if (strcmp(prs->tok->value, "LINE") == 0) {
            eat(prs, TOK_ID);

            if (expect_identifier(prs, "SEQUENTIAL")) {
                eat(prs, TOK_ID);
                organization = ORG_LINE_SEQUENTIAL;
            } else
                eat_until(prs, TOK_DOT);
        } else {
            log_error(prs->file, prs->tok->ln, prs->tok->col);
            fprintf(stderr, "invalid file ORGANIZATION '%s'\n", prs->tok->value);
            show_error(prs->file, prs->tok->ln, prs->tok->col);
            eat_until(prs, TOK_DOT);
        }
    }

    AST *filestatus_var = NULL;

    if (strcmp(prs->tok->value, "FILE") != 0)
        goto build_ast;

    eat(prs, TOK_ID);

    if (!expect_identifier(prs, "STATUS")) {
        eat_until(prs, TOK_DOT);
        goto build_ast;
    }

    eat(prs, TOK_ID);

    if (!expect_identifier(prs, "IS")) {
        eat_until(prs, TOK_DOT);
        goto build_ast;
    }

    eat(prs, TOK_ID);

    Variable *fstat = find_variable(prs->file, prs->tok->value);

    if (!fstat->used) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "undefined variable '%s'\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        filestatus_var = NOP(ln, col);
        eat_until(prs, TOK_DOT);
        goto build_ast;
    } else if (fstat->type.count == 0 || fstat->type.type != TYPE_ALPHANUMERIC) {
        log_error(prs->file, prs->tok->ln, prs->tok->col);
        fprintf(stderr, "file status variable '%s' must have type X(02)\n", prs->tok->value);
        show_error(prs->file, prs->tok->ln, prs->tok->col);

        filestatus_var = NOP(ln, col);
        eat_until(prs, TOK_DOT);
        goto build_ast;
    }

    filestatus_var = create_ast(AST_VAR, prs->tok->ln, prs->tok->col);
    filestatus_var->var.name = mystrdup(prs->tok->value);
    filestatus_var->var.sym = fstat;
    eat(prs, TOK_ID);

build_ast: ;

    AST *ast = create_ast(AST_SELECT, ln, col);
    ast->select.fd_var = create_ast(AST_VAR, ln, col);
    ast->select.fd_var->var.name = mystrdup(var->name);
    ast->select.fd_var->var.sym = var;
    ast->select.filename = filename;
    ast->select.filestatus_var = filestatus_var;
    ast->select.organization = organization;
    return ast;
}

void parse_file_control(Parser *prs) {
    while (!should_break_from(prs, "DIVISION")) {
        while (prs->tok->type == TOK_DOT)
            eat(prs, TOK_DOT);

        if (should_break_from(prs, "DIVISION") || should_break_from(prs, "SECTION"))
            break;

        if (strcmp(prs->tok->value, "SELECT") == 0)
            astlist_push(root_ptr, parse_select(prs));
        else {
            log_error(prs->file, prs->tok->ln, prs->tok->col);
            fprintf(stderr, "invalid clause '%s' in FILE CONTROL\n", prs->tok->value);
            show_error(prs->file, prs->tok->ln, prs->tok->col);
            eat_until(prs, TOK_DOT);
        }
    }
}

void parse_input_output_section(Parser *prs) {
    while (!should_break_from(prs, "DIVISION")) {
        while (prs->tok->type == TOK_DOT)
            eat(prs, TOK_DOT);

        if (should_break_from(prs, "DIVISION") || should_break_from(prs, "SECTION"))
            break;

        if (strcmp(prs->tok->value, "FILE-CONTROL") == 0) {
            eat(prs, TOK_ID);
            parse_file_control(prs);
            return;
        } else {
            log_error(prs->file, prs->tok->ln, prs->tok->col);
            fprintf(stderr, "invalid clause '%s' in INPUT-OUTPUT SECTION\n", prs->tok->value);
            show_error(prs->file, prs->tok->ln, prs->tok->col);
            eat_until(prs, TOK_DOT);
        }
    }
}

void parse_environment_division(Parser *prs) {
    while (!should_break_from(prs, "DIVISION")) {
        while (prs->tok->type == TOK_DOT)
            eat(prs, TOK_DOT);

        if (should_break_from(prs, "DIVISION"))
            break;

        if (!expect_identifier(prs, NULL)) {
            eat_until(prs, TOK_DOT);
            continue;
        }

        if (should_break_from(prs, "DIVISION"))
            break;

        if (strcmp(prs->tok->value, "INPUT-OUTPUT") == 0) {
            eat(prs, TOK_ID);

            if (expect_identifier(prs, "SECTION")) {
                eat(prs, TOK_ID);
                eat(prs, TOK_DOT);
                parse_input_output_section(prs);
            } else
                eat_until(prs, TOK_DOT);
        } else {
            log_error(prs->file, prs->tok->ln, prs->tok->col);
            fprintf(stderr, "invalid clause '%s' in ENVIRONMENT DIVISION\n", prs->tok->value);
            show_error(prs->file, prs->tok->ln, prs->tok->col);
            eat_until(prs, TOK_DOT);
        }
    }
}

void parse_file_section(Parser *prs) {
    while (!should_break_from(prs, "DIVISION")) {
        while (prs->tok->type == TOK_DOT)
            eat(prs, TOK_DOT);

        if (should_break_from(prs, "DIVISION") || should_break_from(prs, "SECTION"))
            break;

        if (strcmp(prs->tok->value, "FD") == 0)
            astlist_push(root_ptr, parse_fd(prs));
        else {
            log_error(prs->file, prs->tok->ln, prs->tok->col);
            fprintf(stderr, "invalid clause '%s' in FILE SECTION\n", prs->tok->value);
            show_error(prs->file, prs->tok->ln, prs->tok->col);
            eat_until(prs, TOK_DOT);
        }
    }
}

/*
void parse_data_division(Parser *prs, bool ignore_working_storage) {
    while (!should_break_from(prs, "DIVISION")) {
        while (prs->tok->type == TOK_DOT)
            eat(prs, TOK_DOT);

        if (should_break_from(prs, "DIVISION"))
            break;

        if (!expect_identifier(prs, NULL)) {
            eat_until(prs, TOK_DOT);
            continue;
        }

        if (should_break_from(prs, "DIVISION"))
            break;

        if (strcmp(prs->tok->value, "WORKING-STORAGE") == 0) {
            if (ignore_working_storage) {

                !!WARNING THIS DOESNT WORK!!

                while (!should_break_from(prs, "DIVISION") && !should_break_from(prs, "SECTION"))
                    eat(prs, prs->tok->type);

                if (strcmp(peek(prs, 1)->value, "DIVISION") == 0)
                    return;

                // Found another section, parse it next.
                continue;
            }

            eat(prs, TOK_ID);

            if (expect_identifier(prs, "SECTION")) {
                eat(prs, TOK_ID);
                eat(prs, TOK_DOT);
                parse_working_storage_section(prs);
            } else
                eat_until(prs, TOK_DOT);
        } elseif (strcmp(prs->tok->value, "FILE") == 0) {
            eat(prs, TOK_ID);

            if (expect_identifier(prs, "SECTION")) {
                eat(prs, TOK_ID);
                eat(prs, TOK_DOT);
                parse_file_section(prs);
            } else
                eat_until(prs, TOK_DOT);
        } else {
            log_error(prs->file, prs->tok->ln, prs->tok->col);
            fprintf(stderr, "invalid clause '%s' in DATA DIVISION\n", prs->tok->value);
            show_error(prs->file, prs->tok->ln, prs->tok->col);
            eat_until(prs, TOK_DOT);
        //}
    }
}
*/

// LINKAGE SECTION variables need to be explicitly used in the
// PROCEDURE DIVISION statement for some reason.
void parse_using_linkages(Parser *prs) {
    eat(prs, TOK_ID);
    size_t count = 0;

    while (prs->tok->type == TOK_COMMA || count == 0) {
        if (count > 0)
            eat(prs, TOK_COMMA);

        if (!expect_identifier(prs, NULL)) {
            eat_until(prs, TOK_DOT);
            return;
        }

        Variable *var = find_variable(prs->file, prs->tok->value);

        if (!var->used) {
            log_error(prs->file, prs->tok->ln, prs->tok->col);
            fprintf(stderr, "undefined variable '%s'\n", prs->tok->value);
            show_error(prs->file, prs->tok->ln, prs->tok->col);
        } else if (!var->is_linkage_src) {
            log_error(prs->file, prs->tok->ln, prs->tok->col);
            fprintf(stderr, "variable '%s' not defined in LINKAGE SECTION\n", prs->tok->value);
            show_error(prs->file, prs->tok->ln, prs->tok->col);
        } else if (var->using_in_proc_div) {
            log_error(prs->file, prs->tok->ln, prs->tok->col);
            fprintf(stderr, "variable '%s' already USING in PROCEDURE DIVISION\n", prs->tok->value);
            show_error(prs->file, prs->tok->ln, prs->tok->col);
        } else
            var->using_in_proc_div = true;

        eat(prs, TOK_ID);
        count++;
    }
}

void parse_procedure_division(Parser *prs) {
    if (strcmp(prs->tok->value, "USING") == 0)
        parse_using_linkages(prs);

    prs->in_main = true;

    while (!should_break_from(prs, "DIVISION")) {
        while (prs->tok->type == TOK_DOT)
            eat(prs, TOK_DOT);

        if (should_break_from(prs, "DIVISION"))
            break;

        if (!expect_identifier(prs, NULL))
            eat_until(prs, TOK_DOT);

        if (should_break_from(prs, "DIVISION"))
            break;

        AST *stmt = parse_procedure_stmt(prs, root_ptr);

        if (validate_stmt(stmt))
            astlist_push(root_ptr, stmt);
    }

    prs->in_main = false;
}

void parse_division(Parser *prs) {
    if (!expect_identifier(prs, NULL)) {
        eat_until(prs, TOK_DOT);
        return;
    }

    char *division_name = mystrdup(prs->tok->value);
    const size_t ln = prs->tok->ln;
    const size_t col = prs->tok->col;
    eat(prs, TOK_ID);

    if (expect_identifier(prs, "DIVISION"))
        eat(prs, TOK_ID);

    if (strcmp(division_name, "IDENTIFICATION") == 0)
        parse_identification_division(prs);
    else if (strcmp(division_name, "ENVIRONMENT") == 0)
        parse_environment_division(prs);
    //else if (strcmp(division_name, "DATA") == 0)
      //  parse_data_division(prs, false);
    else if (strcmp(division_name, "PROCEDURE") == 0)
        parse_procedure_division(prs);
    else {
        log_error(prs->file, ln, col);
        fprintf(stderr, "invalid division '%s'\n", division_name);
        show_error(prs->file, ln, col);
    }

    free(division_name);

    while (prs->tok->type == TOK_DOT)
        eat(prs, TOK_DOT);
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
        case TOK_LPAREN: return parse_parens(prs);
        default: break;
    }

    log_error(prs->file, prs->tok->ln, prs->tok->col);
    fprintf(stderr, "invalid clause '%s'\n", tokentype_to_string(prs->tok->type));
    show_error(prs->file, prs->tok->ln, prs->tok->col);

    eat_until(prs, TOK_DOT); // Next statement.
    return NOP(prs->tok->ln, prs->tok->col);
}

void eat_until_division(Parser *prs, char *name) {
    while (prs->tok->type != TOK_EOF && (strcmp(prs->tok->value, name) != 0 || strcmp(peek(prs, 1)->value, "DIVISION") != 0)) {
        if (prs->tok->type == TOK_ID)
            eat(prs, TOK_ID);
        else
            eat_until(prs, TOK_ID);
    }
}

void eat_until_section(Parser *prs, char *name) {
    while (prs->tok->type != TOK_EOF && (strcmp(prs->tok->value, name) != 0 || strcmp(peek(prs, 1)->value, "SECTION") != 0)) {
        if (prs->tok->type == TOK_ID)
            eat(prs, TOK_ID);
        else
            eat_until(prs, TOK_ID);
    }
}

void parser_reset(Parser *prs) {
    prs->pos = 0;
    prs->tok = &prs->tokens[0];
}

AST *parse_file(char *file, char **main_infiles, bool *out_had_main) {
    *out_had_main = false;
    cur_file = mystrdup(file);
    //delayed_assigns = create_astlist();

    Parser prs = create_parser(file, main_infiles);

    AST *root = create_ast(AST_ROOT, 1, 1);
    root->root = create_astlist();
    root_ptr = &root->root;

    // Parse the IDENTIFICATION DIVISION first for the PROGRAM-ID etc.
    eat_until_division(&prs, "IDENTIFICATION");

    if (prs.tok->type != TOK_EOF) {
        eat(&prs, TOK_ID);
        eat(&prs, TOK_ID);

        prs.cur_div = DIV_IDENTIFICATION;
        parse_identification_division(&prs);
    }

    parser_reset(&prs);

    eat_until_section(&prs, "LINKAGE");

    if (prs.tok->type != TOK_EOF) {
        eat(&prs, TOK_ID);
        eat(&prs, TOK_ID);

        prs.cur_div = DIV_DATA;
        prs.cur_sect = SECT_LINKAGE;
        parse_linkage_section(&prs);
    }

    parser_reset(&prs);

    // Symbols from the WORKING-STORAGE SECTION can be referenced
    // in the DATA DIVISION before the WORKING-STORAGE SECTION,
    // e.g FILE SECTION, so we need to parse this first.
    eat_until_section(&prs, "WORKING-STORAGE");

    if (prs.tok->type != TOK_EOF) {
        eat(&prs, TOK_ID);
        eat(&prs, TOK_ID);

        prs.cur_div = DIV_DATA;
        prs.cur_sect = SECT_WORKING_STORAGE;
        parse_working_storage_section(&prs);
    }

    parser_reset(&prs);

    eat_until_section(&prs, "FILE");

    if (prs.tok->type != TOK_EOF) {
        eat(&prs, TOK_ID);
        eat(&prs, TOK_ID);

        prs.cur_div = DIV_DATA;
        prs.cur_sect = SECT_FILE;
        parse_file_section(&prs);
    }

    parser_reset(&prs);

    // Now make sure we have other symbols from the DATA DIVISION so
    // it can be used in other divisions, like INPUT-OUTPUT SECTION etc.

    // !!!WARNING!!!

    // At the moment, we instead just parse each section from the
    // data division individually, instead of parsing the whole division at once,
    // because redefining symbols are a problem.
    /*
    eat_until_division(&prs, "DATA");

    if (prs.tok->type != TOK_EOF) {
        eat(&prs, TOK_ID);
        eat(&prs, TOK_ID);
        parse_data_division(&prs, true);
    }

    parser_reset(&prs);
    */

    // Now we can parse FILE-CONTROL stuff if it is used.
    eat_until_division(&prs, "ENVIRONMENT");

    if (prs.tok->type != TOK_EOF) {
        eat(&prs, TOK_ID);
        eat(&prs, TOK_ID);

        prs.cur_div = DIV_ENVIRONMENT;
        parse_environment_division(&prs);
    }

    parser_reset(&prs);

    // Finally parse the actual program (PROCEDURE DIVISION is optional).
    eat_until_division(&prs, "PROCEDURE");

    if (prs.tok->type != TOK_EOF) {
        *out_had_main = true;
        prs.cur_div = DIV_PROCEDURE;
        parse_division(&prs);
    }

    delete_parser(&prs);
    free(cur_file);
    return root;
}

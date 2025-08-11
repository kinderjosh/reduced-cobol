#include "ast.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

extern char *cur_file;

AST *create_ast(ASTType type, size_t ln, size_t col) {
    AST *ast = malloc(sizeof(AST));
    ast->type = type;
    ast->ln = ln;
    ast->col = col;
    ast->file = mystrdup(cur_file);
    return ast;
}

void delete_ast(AST *ast) {
    switch (ast->type) {
        case AST_ROOT:
            delete_astlist(&ast->root);
            break;
        case AST_STRING:
            free(ast->constant.string);
            break;
        case AST_VAR:
            free(ast->var.name);
            break;
        case AST_PROC:
            free(ast->proc);
            break;
        case AST_PIC:
            free(ast->pic.name);
            
            if (ast->pic.value != NULL)
                delete_ast(ast->pic.value);
            break;
        case AST_MOVE:
            delete_ast(ast->move.dst);
            delete_ast(ast->move.src);
            break;
        case AST_INTRINSIC:
            if (ast->intrinsic.arg != NULL)
                delete_ast(ast->intrinsic.arg);
            break;
        default: break;
    }

    free(ast->file);
    free(ast);
}

char *asttype_to_string(ASTType type) {
    switch (type) {
        case AST_NOP: return "nop";
        case AST_ROOT: return "root";
        case AST_INT: return "int";
        case AST_FLOAT: return "float";
        case AST_STRING: return "string";
        case AST_VAR: return "variable";
        case AST_PROC: return "procedure";
        case AST_PIC: return "picture";
        case AST_MOVE: return "move";
        case AST_INTRINSIC: return "intrinsic";
        default: break;
    }

    assert(false);
    return "undefined";
}

ASTList create_astlist() {
    return (ASTList){ .items = malloc(16 * sizeof(AST *)), .size = 0, .capacity = 16 };
}

void astlist_push(ASTList *list, AST *item) {
    if (list->size + 1 >= list->capacity) {
        list->capacity *= 2;
        list->items = realloc(list->items, list->capacity * sizeof(AST *));
    }

    list->items[list->size++] = item;
}

void delete_astlist(ASTList *list) {
    for (size_t i = 0; i < list->size; i++)
        delete_ast(list->items[i]);

    free(list->items);
}
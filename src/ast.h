#ifndef AST_H
#define AST_H

#include "token.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TYPE_ANY,
    TYPE_ALPHABETIC,
    TYPE_ALPHANUMERIC,
    TYPE_SIGNED_NUMERIC,
    TYPE_UNSIGNED_NUMERIC,
    TYPE_DECIMAL_NUMERIC
} PictureType;

typedef struct {
    char *file;
    char *name;
    PictureType type;
    unsigned int count;
    bool used;
    bool is_label;
    bool is_index;
} Variable;

typedef enum {
    AST_NOP,
    AST_ROOT,
    AST_INT,
    AST_FLOAT,
    AST_STRING,
    AST_VAR,
    AST_STOP,
    AST_DISPLAY,
    AST_PIC,
    AST_MOVE,
    AST_ARITHMETIC,
    AST_OPER,
    AST_COMPUTE,
    AST_MATH,
    AST_PARENS,
    AST_CONDITION,
    AST_IF,
    AST_NOT,
    AST_LABEL,
    AST_PERFORM,
    AST_PROC,
    AST_BLOCK,
    AST_PERFORM_CONDITION,
    AST_PERFORM_COUNT,
    AST_PERFORM_VARYING,
    AST_PERFORM_UNTIL,
    AST_SUBSCRIPT
} ASTType;

typedef struct AST AST;

typedef struct ASTList {
    AST **items;
    size_t size;
    size_t capacity;
} ASTList;

typedef struct AST {
    ASTType type;
    size_t ln;
    size_t col;
    char *file;

    union {
        ASTList root;
        
        union {
            int32_t i32;
            double f64;
            char *string;
        } constant;

        struct {
            char *name;
            Variable *sym;
        } var;

        struct {
            AST *value;
            bool add_newline;
        } display;

        struct {
            unsigned int level;
            char *name;
            PictureType type;
            AST *value;
            unsigned int count;
            bool is_index;
        } pic;

        struct {
            AST *dst;
            AST *src;
        } move;

        struct {
            char *name;
            AST *left;
            AST *right;
            AST *dst;
            bool implicit_giving;
            bool cloned_left;
            bool cloned_right;
        } arithmetic;

        TokenType oper;

        struct {
            AST *dst;
            AST *math;
        } compute;

        ASTList math;
        AST *parens;
        ASTList condition;

        struct {
            AST *condition;
            ASTList body;
            ASTList else_body;
        } if_stmt;

        AST *not_value;
        char *label;
        AST *perform;

        struct {
            char *name;
            ASTList body;
        } proc;

        ASTList block;

        struct {
            AST *proc;
            AST *condition;
        } perform_condition;

        struct {
            AST *proc;
            unsigned int times;
        } perform_count;

        struct {
            AST *var;
            AST *by;
            AST *from;
            AST *until;
            ASTList body;
        } perform_varying;

        struct {
            AST *until;
            ASTList body;
        } perform_until;

        struct {
            AST *base;
            AST *index;
            AST *value;
        } subscript;
    };
} AST;

AST *create_ast(ASTType type, size_t ln, size_t col);
void delete_ast(AST *ast);
char *asttype_to_string(ASTType type);

ASTList create_astlist();
void delete_astlist(ASTList *list);
void astlist_push(ASTList *list, AST *item);

#endif
#ifndef AST_H
#define AST_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    INTR_STOP,
    INTR_DISPLAY
} IntrinsicType;

typedef enum {
    TYPE_ANY,
    TYPE_ALPHABETIC,
    TYPE_NUMERIC,
    TYPE_ALPHANUMERIC
} PictureType;

typedef struct {
    char *file;
    char *name;
    PictureType type;
    unsigned int count;
    bool used;
} Variable;

typedef enum {
    AST_NOP,
    AST_ROOT,
    AST_INT,
    AST_FLOAT,
    AST_STRING,
    AST_VAR,
    AST_PROC,
    AST_PIC,
    AST_MOVE,
    AST_INTRINSIC
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
        
        struct {
            uint64_t i64;
            double f64;
            char *string;
        } constant;

        char *proc;

        struct {
            char *name;
            Variable *sym;
        } var;

        struct {
            unsigned int level;
            char *name;
            PictureType type;
            AST *value;
            unsigned int count;
        } pic;

        struct {
            AST *dst;
            AST *src;
        } move;

        struct {
            IntrinsicType type;
            AST *arg;
        } intrinsic;
    };
} AST;

AST *create_ast(ASTType type, size_t ln, size_t col);
void delete_ast(AST *ast);
char *asttype_to_string(ASTType type);

ASTList create_astlist();
void delete_astlist(ASTList *list);
void astlist_push(ASTList *list, AST *item);

#endif
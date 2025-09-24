#ifndef AST_H
#define AST_H

#include "token.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define RESERVED_INDEX_PLACES 99

typedef struct {
    enum {
        TYPE_ANY,
        TYPE_ALPHABETIC,
        TYPE_ALPHANUMERIC,
        TYPE_SIGNED_NUMERIC,
        TYPE_UNSIGNED_NUMERIC,
        TYPE_DECIMAL_NUMERIC
    } type;

    unsigned int places;
    unsigned int decimal_places; // Only for floats.
    unsigned int count;
} PictureType;

typedef struct {
    char *file;
    char *name;
    PictureType type;
    unsigned int count;
    bool used;
    bool is_label;
    bool is_index;
    bool is_fd;
} Variable;

typedef enum {
    AST_NOP,
    AST_ROOT,
    AST_INT,
    AST_FLOAT,
    AST_STRING,
    AST_VAR,
    AST_STOP,
    AST_STOP_RUN,
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
    AST_SUBSCRIPT,
    AST_CALL,
    AST_NULL,
    AST_BOOL,
    AST_STRING_BUILDER,
    AST_STRING_SPLITTER,
    AST_OPEN,
    AST_CLOSE,
    AST_SELECT,
    AST_READ,
    AST_WRITE,
    AST_INSPECT,
    AST_ACCEPT,
    AST_ZERO,
    AST_ARGV,
    AST_EXIT
} ASTType;

typedef struct AST AST;

typedef struct ASTList {
    AST **items;
    size_t size;
    size_t capacity;
} ASTList;

typedef struct {
    AST *value;

    enum {
        DELIM_SPACE,
        DELIM_SIZE
    } delimit;
} StringStatement;

typedef struct {
    bool before;
    bool after;
    AST *value;
    AST *modifier;
} StringTallyPhase1;

typedef struct {
    enum {
        TALLY_CHARACTERS,
        TALLY_TOTAL_CHARACTERS,
        TALLY_ALL,
        TALLY_LEADING
    } type;

    AST *output_count;
    StringTallyPhase1 phase;
} StringTally;

typedef struct {
    StringTally *tallies;
    size_t tally_count;
    size_t tally_capacity;
} InspectTallying;

typedef struct {
    enum {
        REPLACING_ALL,
        REPLACING_FIRST,
    } type;

    AST *old;
    AST *new;
    AST *modifier;
    bool before;
    bool after;
} StringReplace;

typedef struct {
    StringReplace *replaces;
    size_t replace_count;
    size_t replace_capacity;
} InspectReplacing;

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
            bool is_fd;
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

        struct {
            char *name;
            ASTList args;
            AST *returning;
        } call;
        
        bool bool_value;

        struct {
            StringStatement base;
            StringStatement *stmts;
            size_t stmt_count;
            size_t stmt_cap;
            AST *into_var;
            AST *with_pointer;
        } string_builder;

        struct {
            StringStatement base;
            ASTList into_vars;
        } string_splitter;

        struct {
            AST *filename;
            
            enum {
                OPEN_INPUT,
                OPEN_OUTPUT,
                OPEN_IO,
                OPEN_EXTEND
            } type;
        } open;

        AST *close_filename;

        struct {
            AST *fd_var;
            AST *filename;
            AST *filestatus_var;

            enum {
                ORG_NONE,
                ORG_LINE_SEQUENTIAL
            } organization;
        } select;

        struct {
            AST *fd;
            AST *into;
            ASTList at_end_stmts;
            ASTList not_at_end_stmts;
        } read;

        struct {
            AST *value;
        } write;

        struct {
            enum {
                INSPECT_TALLYING,
                INSPECT_REPLACING,
                INSPECT_TALLYING_REPLACING,
                INSPECT_CONVERTING
            } type;

            AST *input_string;

            union {
                InspectTallying tallying;
                InspectReplacing replacing;
            };
        } inspect;

        struct {
            AST *dst;
            AST *from;
        } accept;
    };
} AST;

AST *create_ast(ASTType type, size_t ln, size_t col);
void delete_ast(AST *ast);
char *asttype_to_string(ASTType type);

ASTList create_astlist();
void delete_astlist(ASTList *list);
void astlist_push(ASTList *list, AST *item);

#endif
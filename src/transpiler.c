#include "transpiler.h"
#include "ast.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include <inttypes.h>

char *value_to_string(AST *ast) {
    char *string;

    switch (ast->type) {
        case AST_INT:
            string = malloc(32);
            sprintf(string, "%" PRId64, ast->constant.i64);
            return string;
        case AST_STRING: return mystrdup(ast->constant.string);
        default: break;
    }

    assert(false);
    return calloc(1, sizeof(char));
}

char *emit_stmt(AST *ast);

char *emit_root(AST *root) {
    char *code = malloc(1024);
    strcpy(code, "#include <stdio.h>\nint main(void) {\n");

    size_t len = 37;
    size_t cap = 1024;

    for (size_t i = 0; i < root->root.size; i++) {
        char *stmt = emit_stmt(root->root.items[i]);
        const size_t stmt_len = strlen(stmt);

        if (len + stmt_len + 13 >= cap) {
            cap *= 2;
            code = realloc(code, cap);
        }

        strcat(code, stmt);
        free(stmt);
        len += stmt_len;
    }

    strcat(code, "return 0;\n}\n");
    return code;
}

char *emit_stop(AST *ast) {
    (void)ast;
    return mystrdup("return 0;\n");
}

char *emit_display(AST *ast) {
    assert(ast->intrinsic.arg != NULL);

    char *arg;

    // TOFIX: We can't print actual numbers yet.
    if (ast->intrinsic.arg->type == AST_INT) {
        arg = malloc(16);
        sprintf(arg, "%c", (char)ast->intrinsic.arg->constant.i64);
    } else
        arg = value_to_string(ast->intrinsic.arg);

    char *code = malloc(strlen(arg) + 32);
    sprintf(code, "printf(\"%s\\n\");\n", arg);
    free(arg);
    return code;
}

char *emit_intrinsic(AST *ast) {
    switch (ast->intrinsic.type) {
        case INTR_STOP: return emit_stop(ast);
        case INTR_DISPLAY: return emit_display(ast);
        default: break;
    }

    assert(false);
    return calloc(1, sizeof(char));
}

char *emit_stmt(AST *ast) {
    switch (ast->type) {
        case AST_INTRINSIC: return emit_intrinsic(ast);
        default: break;
    }

    assert(false);
    return calloc(1, sizeof(char));
}
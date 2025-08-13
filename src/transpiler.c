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

char *picturetype_to_c(PictureType type) {
    return type == TYPE_NUMERIC ? "double" : "char";
}

char *picturename_to_c(char *name) {
    // C can't have '-' in identifiers so just
    // convert them all to underscores.

    const size_t len = strlen(name);
    char *copy = malloc(len + 2);
    copy[0] = '_';

    for (size_t i = 0; i < len; i++) {
        if (name[i] == '-')
            copy[i + 1] = '_';
        else
            copy[i + 1] = name[i];
    }

    copy[len + 1] = '\0';
    return copy;
}

char *value_to_string(AST *ast) {
    char *string;

    switch (ast->type) {
        case AST_INT:
            string = malloc(32);
            sprintf(string, "%" PRId64, ast->constant.i64);
            return string;
        case AST_STRING:
            string = malloc(strlen(ast->constant.string) + 3);
            sprintf(string, "\"%s\"", ast->constant.string);
            return string;
        case AST_VAR: return picturename_to_c(ast->var.name);
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
    char *arg;
    char *code;

    switch (ast->display.value->type) {
        case AST_STRING:
            arg = value_to_string(ast->display.value);
            code = malloc(strlen(arg) + 42);
            sprintf(code, "fputs(%s, stdout);\n", arg);
            break;
        case AST_VAR:
            arg = value_to_string(ast->display.value);
            code = malloc(strlen(arg) + 42);

            if (ast->display.value->var.sym->type == TYPE_NUMERIC)
                sprintf(code, "printf(\"%%g\", %s);\n", arg);
            // A single character string, not actually treated as a string.
            else if (ast->display.value->var.sym->count == 0)
                sprintf(code, "printf(\"%%c\", %s);\n", arg);
            else
                sprintf(code, "printf(\"%%s\", %s);\n", arg);
            break;
        default:
            assert(false);
            return calloc(1, sizeof(char));
    }

    if (ast->display.add_newline)
        strcat(code, "fputc('\\n', stdout);\n");

    free(arg);
    return code;
}

char *emit_pic(AST *ast) {
    const char *type = picturetype_to_c(ast->pic.type);
    char *name = picturename_to_c(ast->pic.name);
    char *code;

    if (ast->pic.value == NULL) {
        code = malloc(strlen(name) + strlen(type) + 12);

        if (ast->pic.count > 0)
            sprintf(code, "%s %s[%u];\n", type, name, ast->pic.count);
        else
            sprintf(code, "%s %s;\n", type, name);
    } else {
        char *value = value_to_string(ast->pic.value);
        code = malloc(strlen(name) + strlen(value) + strlen(type) + 17);

        if (ast->pic.count > 0)
            sprintf(code, "%s %s[%u] = %s;\n", type, name, ast->pic.count, value);
        else
            sprintf(code, "%s %s = %s;\n", type, name, value);

        free(value);
    }

    free(name);
    return code;
}

char *emit_move(AST *ast) {
    char *dst = value_to_string(ast->move.dst);
    char *src = value_to_string(ast->move.src);
    char *code = malloc(strlen(src) + strlen(dst) + 32);
    sprintf(code, "%s = %s;\n", dst, src);
    free(dst);
    free(src);
    return code;
}

char *emit_arithmetic(AST *ast) {
    char *left = value_to_string(ast->arithmetic.left);
    char *right = value_to_string(ast->arithmetic.right);
    char *dst = value_to_string(ast->arithmetic.dst);
    char *code = malloc(strlen(left) + strlen(right) + strlen(dst) + 27);

    if (strcmp(ast->arithmetic.name, "ADD") == 0)
        sprintf(code, "%s = %s + %s;\n", dst, left, right);
    else if (strcmp(ast->arithmetic.name, "SUBTRACT") == 0)
        sprintf(code, "%s = %s - %s;\n", dst, right, left);
    else if (strcmp(ast->arithmetic.name, "MULTIPLY") == 0)
        sprintf(code, "%s = %s * %s;\n", dst, left, right);
    else if (strcmp(ast->arithmetic.name, "DIVIDE") == 0)
        sprintf(code, "%s = %s / %s;\n", dst, right, left);
    else
        sprintf(code, "%s = (long long)%s %% %s;\n", dst, right, left);

    free(left);
    free(right);
    free(dst);
    return code;
}

char *emit_stmt(AST *ast) {
    switch (ast->type) {
        case AST_STOP: return emit_stop(ast);
        case AST_DISPLAY: return emit_display(ast);
        case AST_PIC: return emit_pic(ast);
        case AST_MOVE: return emit_move(ast);
        case AST_ARITHMETIC: return emit_arithmetic(ast);
        default: break;
    }

    assert(false);
    return calloc(1, sizeof(char));
}
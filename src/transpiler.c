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

char *emit_stmt(AST *ast);

char *value_to_string(AST *ast) {
    char *string;

    switch (ast->type) {
        case AST_NOP:
            assert(false && "got a nop");
            return calloc(1, sizeof(char));
        case AST_INT:
            string = malloc(32);
            sprintf(string, "%" PRId64, ast->constant.i64);
            return string;
        case AST_STRING:
            string = malloc(strlen(ast->constant.string) + 3);
            sprintf(string, "\"%s\"", ast->constant.string);
            return string;
        case AST_VAR: return picturename_to_c(ast->var.name);
        case AST_PARENS: {
            char *value = value_to_string(ast->parens);
            string = malloc(strlen(value) + 3);
            sprintf(string, "(%s)", value);
            free(value);
            return string;
        }
        case AST_MATH:
        case AST_CONDITION:
        case AST_NOT: return emit_stmt(ast);
        default: break;
    }

    assert(false);
    return calloc(1, sizeof(char));
}

char *emit_stmt(AST *ast);

char *emit_list(ASTList *list) {
    char *code = malloc(1024);
    code[0] = '\0';
    size_t len = 0;
    size_t cap = 1024;

    for (size_t i = 0; i < list->size; i++) {
        char *stmt = emit_stmt(list->items[i]);
        const size_t stmt_len = strlen(stmt);

        if (len + stmt_len + 1 >= cap) {
            while (len + stmt_len + 1 >= cap)
                cap *= 2;

            code = realloc(code, cap);
        }

        strcat(code, stmt);
        free(stmt);
        len += stmt_len;
    }

    return code;
}

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
    char *dst;

    if (ast->arithmetic.implicit_giving)
        dst = value_to_string(ast->arithmetic.right);
    else
        dst = value_to_string(ast->arithmetic.dst);

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

char *emit_math(AST *ast) {
    char *values = malloc(32);
    values[0] = '\0';
    size_t values_len = 0;
    size_t values_cap = 32;
    bool has_mod = false;

    for (size_t i = 1; i < ast->math.size; i += 2) {
        if (ast->math.items[i]->oper == TOK_MOD) {
            has_mod = true;
            continue;
        }
    }

    for (size_t i = 0; i < ast->math.size; i++) {
        AST *value = ast->math.items[i];
        char *value_string;
        
        if (value->type == AST_OPER) {
            value_string = malloc(2);

            if (value->oper == TOK_PLUS)
                strcpy(value_string, "+");
            else if (value->oper == TOK_MINUS)
                strcpy(value_string, "-");
            else if (value->oper == TOK_STAR)
                strcpy(value_string, "*");
            else if (value->oper == TOK_SLASH)
                strcpy(value_string, "/");
            else {
                strcpy(value_string, "%");
                has_mod = true;
            }
        } else
            value_string = value_to_string(value);

        const size_t value_len = strlen(value_string);

        if (values_len + value_len + 15 >= values_cap) {
            while (values_len + value_len + 15 >= values_cap)
                values_cap *= 2;

            values = realloc(values, values_cap);
        }

        if (value->type != AST_OPER && has_mod) {
            strcat(values, "(long long)");
            values_len += 12;
        }

        strcat(values, value_string);

        values_len += value_len;
        free(value_string);

        if (i != ast->math.size - 1)
            strcat(values, " ");
    }

    return values;
}

char *emit_compute(AST *ast) {
    char *dst = value_to_string(ast->compute.dst);
    char *math = value_to_string(ast->compute.math);

    char *values = malloc(strlen(dst) + strlen(math) + 10);
    sprintf(values, "%s = %s;\n", dst, math);

    free(dst);
    free(math);
    return values;
}

char *emit_condition(AST *ast) {
    char *values = malloc(32);
    values[0] = '\0';
    size_t values_len = 0;
    size_t values_cap = 32;

    for (size_t i = 0; i < ast->condition.size; i++) {
        AST *value = ast->condition.items[i];
        char *value_string;
        
        if (value->type == AST_OPER) {
            value_string = malloc(3);

            if (value->oper == TOK_EQ)
                strcpy(value_string, "==");
            else if (value->oper == TOK_NEQ)
                strcpy(value_string, "!=");
            else if (value->oper == TOK_LT)
                strcpy(value_string, "<");
            else if (value->oper == TOK_LTE)
                strcpy(value_string, "<=");
            else if (value->oper == TOK_GT)
                strcpy(value_string, ">");
            else
                strcpy(value_string, ">=");
        } else
            value_string = value_to_string(value);

        const size_t value_len = strlen(value_string);

        if (values_len + value_len + 15 >= values_cap) {
            while (values_len + value_len + 15 >= values_cap)
                values_cap *= 2;

            values = realloc(values, values_cap);
        }

        strcat(values, value_string);

        values_len += value_len;
        free(value_string);

        if (i != ast->math.size - 1)
            strcat(values, " ");
    }

    return values;
}

char *emit_if(AST *ast) {
    char *condition = emit_condition(ast->if_stmt.condition);
    char *body = emit_list(&ast->if_stmt.body);
    char *code;
    
    if (ast->if_stmt.else_body.size > 0) {
        char *else_body = emit_list(&ast->if_stmt.else_body);
        code = malloc(strlen(condition) + strlen(body) + strlen(else_body) + 27);
        sprintf(code, "if (%s) {\n%s\n} else {\n%s}\n", condition, body, else_body);
        free(else_body);
    } else {
        code = malloc(strlen(condition) + strlen(body) + 15);
        sprintf(code, "if (%s) {\n%s}\n", condition, body);
    }

    free(body);
    free(condition);
    return code;
}

char *emit_not(AST *ast) {
    char *value = value_to_string(ast->not_value);
    char *code = malloc(strlen(value) + 4);
    sprintf(code, "!(%s)", value);
    free(value);
    return code;
}

char *emit_stmt(AST *ast) {
    switch (ast->type) {
        case AST_STOP: return emit_stop(ast);
        case AST_DISPLAY: return emit_display(ast);
        case AST_PIC: return emit_pic(ast);
        case AST_MOVE: return emit_move(ast);
        case AST_ARITHMETIC: return emit_arithmetic(ast);
        case AST_COMPUTE: return emit_compute(ast);
        case AST_MATH: return emit_math(ast);
        case AST_IF: return emit_if(ast);
        case AST_CONDITION: return emit_condition(ast);
        case AST_NOT: return emit_not(ast);
        default: break;
    }

    assert(false);
    return calloc(1, sizeof(char));
}
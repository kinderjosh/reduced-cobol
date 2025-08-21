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

#define INCLUDE_LIBS "#define _RED_COBOL_SOURCE\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <stdbool.h>\n#include <assert.h>\n#include <stdint.h>\n#include <ctype.h>\n#include <inttypes.h>\n#include <limits.h>\n"
#define INCLUDE_LIBS_LEN 208

static char *globals;
static size_t globals_len;
static size_t globals_cap;

static char *functions;
static size_t functions_len;
static size_t functions_cap;

static char *function_predefs;
static size_t function_predefs_len;
static size_t function_predefs_cap;

char *picturetype_to_c(PictureType type) {
    if (type == TYPE_DECIMAL_NUMERIC)
        return "double";
    else if (type == TYPE_SIGNED_NUMERIC)
        return "int";
    else if (type == TYPE_UNSIGNED_NUMERIC)
        return "unsigned int";

    return "char";
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
            sprintf(string, "%d", ast->constant.i32);
            return string;
        case AST_FLOAT:
            string = malloc(32);
            sprintf(string, "%lf", ast->constant.f64);
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
        case AST_LABEL: return picturename_to_c(ast->label);
        case AST_MATH:
        case AST_CONDITION:
        case AST_SUBSCRIPT:
        case AST_NOT: return emit_stmt(ast);
        default: break;
    }

    printf(">>>%s\n", asttype_to_string(ast->type));

    assert(false);
    return calloc(1, sizeof(char));
}

void append_global(char *global) {
    const size_t len = strlen(global);

    if (globals_len + len + 1 >= globals_cap) {
        while (globals_len + len + 1 >= globals_cap)
            globals_cap *= 2;

        globals = realloc(globals, globals_cap);
    }

    strcat(globals, global);
    globals_len += len;
}

void append_function(char *code) {
    const size_t len = strlen(code);

    if (functions_len + len + 1 >= functions_cap) {
        while (functions_len + len + 1 >= functions_cap)
            functions_cap *= 2;

        functions = realloc(functions, functions_cap);
    }

    strcat(functions, code);
    functions_len += len;
}

void append_function_predef(char *code) {
    const size_t len = strlen(code);

    if (function_predefs_len + len + 1 >= function_predefs_cap) {
        while (function_predefs_len + len + 1 >= function_predefs_cap)
            function_predefs_cap *= 2;

        function_predefs = realloc(function_predefs, function_predefs_cap);
    }

    strcat(function_predefs, code);
    function_predefs_len += len;
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
    strcpy(code, "int main(void) {\n");

    size_t len = 18;
    size_t cap = 1024;

    globals = malloc(1024);
    globals[0] = '\0';
    globals_len = 0;
    globals_cap = 1024;

    functions = malloc(1024);
    functions[0] = '\0';
    functions_len = 0;
    functions_cap = 1024;

    function_predefs = malloc(1024);
    function_predefs[0] = '\0';
    function_predefs_len = 0;
    function_predefs_cap = 1024;

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
    len += 13;

    char *total = malloc(INCLUDE_LIBS_LEN + len + globals_len + functions_len + function_predefs_len + 28);
    sprintf(total, "%s%s%s%s%s", INCLUDE_LIBS, globals, function_predefs, functions, code);
    free(code);
    free(globals);
    free(functions);
    free(function_predefs);
    return total;
}

char *emit_stop(AST *ast) {
    (void)ast;
    return mystrdup("return 0;\n");
}

char *picturetype_to_format_specifier(PictureType type, unsigned int count) {
    if (type == TYPE_DECIMAL_NUMERIC)
        return "%g";
    else if (type == TYPE_SIGNED_NUMERIC)
        return "%d";
    else if (type == TYPE_UNSIGNED_NUMERIC)
        return "%u";
    else if (count == 0)
        return "%c";

    return "%s";
}

char *emit_display(AST *ast) {
    AST *value = ast->display.value;
    char *arg = value_to_string(value);
    char *code = malloc(strlen(arg) + 42);

    switch (value->type) {
        case AST_STRING:
            sprintf(code, "fputs(%s, stdout);\n", arg);
            break;
        case AST_VAR:
            sprintf(code, "printf(\"%s\", %s);\n", picturetype_to_format_specifier(value->var.sym->type, value->var.sym->count), arg);
            break;
        case AST_SUBSCRIPT:
            assert(value->subscript.base->type == AST_VAR);
            sprintf(code, "printf(\"%s\", %s);\n", picturetype_to_format_specifier(value->subscript.base->var.sym->type, 0), arg);
            break;
        default:
            assert(false);
            code[0] = '\0';
            break;
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

    if ((ast->pic.type == TYPE_ALPHABETIC || ast->pic.type == TYPE_ALPHANUMERIC) && ast->pic.count > 0)
        // Account for the null byte.
        ast->pic.count++;

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
    append_global(code);
    free(code);
    return calloc(1, sizeof(char));
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
    strcpy(values, "(");
    size_t values_len = 1;
    size_t values_cap = 32;

    for (size_t i = 0; i < ast->condition.size; i++) {
        AST *value = ast->condition.items[i];
        char *value_string;
        
        if (value->type == AST_OPER) {
            value_string = malloc(3);

            if (value->oper == TOK_EQ || value->oper == TOK_EQUAL)
                strcpy(value_string, "==");
            else if (value->oper == TOK_NEQ)
                strcpy(value_string, "!=");
            else if (value->oper == TOK_LT)
                strcpy(value_string, "<");
            else if (value->oper == TOK_LTE)
                strcpy(value_string, "<=");
            else if (value->oper == TOK_GT)
                strcpy(value_string, ">");
            else if (value->oper == TOK_GTE)
                strcpy(value_string, ">=");
            else if (value->oper == TOK_AND)
                strcpy(value_string, "&&");
            else
                strcpy(value_string, "||");
        } else
            value_string = value_to_string(value);

        const size_t value_len = strlen(value_string);

        if (values_len + value_len + 16 >= values_cap) {
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

    strcat(values, ")");
    return values;
}

char *emit_if(AST *ast) {
    char *condition = emit_condition(ast->if_stmt.condition);
    char *body = emit_list(&ast->if_stmt.body);
    char *code;
    
    if (ast->if_stmt.else_body.size > 0) {
        char *else_body = emit_list(&ast->if_stmt.else_body);
        code = malloc(strlen(condition) + strlen(body) + strlen(else_body) + 27);
        sprintf(code, "if %s {\n%s\n} else {\n%s}\n", condition, body, else_body);
        free(else_body);
    } else {
        code = malloc(strlen(condition) + strlen(body) + 15);
        sprintf(code, "if %s {\n%s}\n", condition, body);
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

char *emit_label(AST *ast) {
    char *name = picturename_to_c(ast->label);
    char *code = malloc(strlen(name) + 4);
    sprintf(code, "%s:\n", name); 
    free(name);
    return code;
}

/*
char *emit_goto(AST *ast) {
    char *label = value_to_string(ast->go);
    char *code = malloc(strlen(label) + 10);
    sprintf(code, "goto %s;\n", label);
    free(label);
    return code;
}
*/

char *emit_perform(AST *ast) {
    char *label = value_to_string(ast->perform);
    char *code = malloc(strlen(label) + 7);
    sprintf(code, "%s();\n", label);
    free(label);
    return code;
}

char *emit_procedure(AST *ast) {
    char *body = emit_list(&ast->proc.body);
    char *name = picturename_to_c(ast->proc.name);
    char *code = malloc(strlen(name) + strlen(body) + 18);
    sprintf(code, "void %s() {\n%s}\n", name, body);
    free(body);
    append_function(code);

    sprintf(code, "void %s();\n", name);
    append_function_predef(code);
    free(code);
    free(name);
    return calloc(1, sizeof(char));
}

char *emit_perform_condition(AST *ast) {
    char *stmt = emit_stmt(ast->perform_condition.proc);
    char *condition = emit_stmt(ast->perform_condition.condition);
    char *code = malloc(strlen(stmt) + strlen(condition) + 17);
    sprintf(code, "while (!(%s))\n%s", condition, stmt);
    free(stmt);
    free(condition);
    return code;
}

char *emit_perform_count(AST *ast) {
    char *stmt = emit_stmt(ast->perform_count.proc);
    char *code = malloc(strlen(stmt) + 41);
    sprintf(code, "for (unsigned int i = 0; i < %u; i++)\n%s", ast->perform_count.times, stmt);
    free(stmt);
    return code;
}

char *emit_perform_varying(AST *ast) {
    char *iter = value_to_string(ast->perform_varying.var);
    char *from = value_to_string(ast->perform_varying.from);
    char *by = value_to_string(ast->perform_varying.by);
    char *condition = value_to_string(ast->perform_varying.until);
    char *body = emit_list(&ast->perform_varying.body);

    char *code = malloc((strlen(iter) * 2) + strlen(from) + strlen(by) + strlen(condition) + strlen(body) + 32);
    sprintf(code, "for (%s = %s; !%s; %s += %s) {\n%s}\n", iter, from, condition, iter, by, body);

    free(body);
    free(condition);
    free(by);
    free(from);
    free(iter);
    return code;
}

char *emit_perform_until(AST *ast) {
    char *condition = emit_stmt(ast->perform_until.until);
    char *body = emit_list(&ast->perform_until.body);

    char *code = malloc(strlen(condition) + strlen(body) + 41);
    sprintf(code, "while (!%s) {\n%s}\n", condition, body);

    free(condition);
    free(body);
    return code;
}

char *emit_subscript(AST *ast) {
    char *base = value_to_string(ast->subscript.base);
    char *index = value_to_string(ast->subscript.index);
    char *code;

    if (ast->subscript.value == NULL) {
        code = malloc(strlen(base) + strlen(index) + 21);
        sprintf(code, "%s[(size_t)(%s - 1)]", base, index);
    } else {
        char *value = value_to_string(ast->subscript.value);
        code = malloc(strlen(base) + strlen(index) + strlen(value) + 28);
        sprintf(code, "%s[(size_t)(%s - 1)] = %s;\n", base, index, value);
        free(value);
    }

    free(base);
    free(index);
    return code;
}

char *emit_call(AST *ast) {
    char *code = malloc(strlen(ast->call.name) + 4);
    sprintf(code, "%s(", ast->call.name);
    size_t len = strlen(code);

    for (size_t i = 0; i < ast->call.args.size; i++) {
        char *arg = value_to_string(ast->call.args.items[i]);
        const size_t arg_len = strlen(arg);

        code = realloc(code, len + arg_len + 6);
        strcat(code, arg);
        free(arg);
        len += arg_len;

        if (i != ast->call.args.size - 1) {
            strcat(code, ", ");
            len += 2;
        }
    }

    strcat(code, ");\n");
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
        case AST_LABEL: return emit_label(ast);
        //case AST_GOTO: return emit_goto(ast);
        case AST_PERFORM: return emit_perform(ast);
        case AST_PROC: return emit_procedure(ast);
        case AST_PERFORM_CONDITION: return emit_perform_condition(ast);
        case AST_PERFORM_COUNT: return emit_perform_count(ast);
        case AST_PERFORM_VARYING: return emit_perform_varying(ast);
        case AST_PERFORM_UNTIL: return emit_perform_until(ast);
        case AST_SUBSCRIPT: return emit_subscript(ast);
        case AST_CALL: return emit_call(ast);
        default: break;
    }

    assert(false);
    return calloc(1, sizeof(char));
}
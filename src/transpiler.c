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

#define INCLUDE_LIBS "#define _RED_COBOL_SOURCE\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <stdbool.h>\n#include <assert.h>\n#include <stdint.h>\n#include <ctype.h>\n#include <inttypes.h>\n#include <limits.h>\n#include <errno.h>\n"
#define INCLUDE_LIBS_LEN strlen(INCLUDE_LIBS)

#define IS_STRING(ttype) ((ttype.type == TYPE_ALPHABETIC || ttype.type == TYPE_ALPHANUMERIC) && ttype.count > 0)

static char *globals;
static size_t globals_len;
static size_t globals_cap;

static char *functions;
static size_t functions_len;
static size_t functions_cap;

static char *function_predefs;
static size_t function_predefs_len;
static size_t function_predefs_cap;

char *picturetype_to_c(PictureType *type) {
    if (type->type == TYPE_DECIMAL_NUMERIC || type->type == TYPE_DECIMAL_SUPRESSED_NUMERIC || type->comp_type == 1 || type->comp_type == 2)
        return type->comp_type == 1 ? "float" : "double";

    if (type->type == TYPE_SIGNED_NUMERIC || type->type == TYPE_SIGNED_SUPRESSED_NUMERIC) {
        if (type->places <= 4)
            return type->comp_type == 5 ? "short" : "int16_t";
        else if (type->places <= 9)
            return type->comp_type == 5 ? "int" : "int32_t";
        else
            return type->comp_type == 5 ? "long long" : "int64_t";
    } else if (type->type == TYPE_UNSIGNED_NUMERIC || type->type == TYPE_UNSIGNED_SUPRESSED_NUMERIC) {
        if (type->places <= 4)
            return type->comp_type == 5 ? "unsigned short" : "uint16_t";
        else if (type->places <= 9)
            return type->comp_type == 5 ? "unsigned int" : "uint32_t";
        else
            return type->comp_type == 5 ? "unsigned long long" : "uint64_t";
    }

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
        case AST_VAR: 
            if (ast->var.sym->is_linkage_src && strlen(ast->var.name) > 3 && ast->var.name[0] == 'L' &&
                ast->var.name[1] == 'S' && ast->var.name[2] == '-')

                // Remove 'LS-' remove linkage variable name.
                return picturename_to_c(ast->var.name + 3);

            return picturename_to_c(ast->var.name);
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
        case AST_BOOL: return mystrdup(ast->bool_value ? "true" : "false");
        case AST_NULL: return mystrdup("NULL");
        case AST_ZERO: return mystrdup("0");
        case AST_LENGTHOF: return emit_stmt(ast);
        default: break;
    }

    printf(">>>%s\n", asttype_to_string(ast->type));

    assert(false);
    return calloc(1, sizeof(char));
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

            assert(ast->subscript.base->type == AST_VAR);

            // A table of strings, the type is still a string, not a character.
            // TOFIX: this is a fucking mess.
            if (ast->subscript.base->var.sym->count > 0 && ast->subscript.base->var.sym->type.count > 0) {
                type.count = ast->subscript.base->var.sym->type.places;
                return type;
            }

            type.count = 0;
            return type;
        }
        // Make LENGTHOF COMP-5 to print as %zu
        case AST_LENGTHOF: return (PictureType){ .type = TYPE_UNSIGNED_SUPRESSED_NUMERIC, .comp_type = 5, .count = 0, .places = 18 };
        default: break;
    }

    printf(">>>%s\n", asttype_to_string(ast->type));
    assert(false);
    return (PictureType){ .type = TYPE_SIGNED_NUMERIC, .count = 0 };
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

char *emit_root(AST *root, bool require_main, char *source_includes) {
    char *code = malloc(1024);
    size_t cap = 1024;
    size_t len;

    if (require_main) {
        strcpy(code, "int main(int argc, char **argv) {\nglobal_argc = argc;\nglobal_argv = argv;\n");
        len = 75;
    } else {
        code[0] = '\0';
        len = 0;
    }

    globals = malloc(1024);
    strcpy(globals, "static char string_builder[4097];\nstatic size_t string_builder_pointer;\nstatic size_t previous_string_statement_size;\nstatic char *read_buffer;\nstatic char file_status[3];\nstatic FILE *last_opened_outfile;\nstatic char *inspect_string;\nstatic size_t inspect_count;\nstatic size_t inspect_string_length;\nstatic bool inspect_found;\nstatic bool inspect_locked;\nstatic char *endptr;\nstatic int global_argc;\nstatic char **global_argv;\n__attribute__((noreturn)) static void cobol_error() {\nfprintf(stderr, \"COBOL: CRITICAL RUNTIME ERROR\\n\");\nexit(EXIT_FAILURE);\n}\n");

    globals_len = strlen(globals);
    globals_cap = 2048;

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

    char *total;

    if (require_main) {
        total = malloc(INCLUDE_LIBS_LEN + len + globals_len + functions_len + function_predefs_len + strlen(source_includes) + 28);
        sprintf(total, "%s%s%s%s%s%s", source_includes, INCLUDE_LIBS, globals, function_predefs, functions, code);
    } else {
        total = malloc(INCLUDE_LIBS_LEN + globals_len + functions_len + function_predefs_len + strlen(source_includes) + 28);
        sprintf(total, "%s%s%s%s%s", source_includes, INCLUDE_LIBS, globals, function_predefs, functions);
    }

    free(code);
    free(globals);
    free(functions);
    free(function_predefs);
    return total;
}

char *emit_stop(AST *ast) {
    (void)ast;
    return mystrdup("return;\n");
}

char *emit_stop_run(AST *ast) {
    (void)ast;
    return mystrdup("return 0;\n");
}

char *picturetype_to_format_specifier(PictureType *type) {
    char *spec = malloc(32);

    if (type->comp_type > 0) {
        if (type->comp_type == 1)
            strcpy(spec, "%f");
        else if (type->comp_type == 2)
            strcpy(spec, "%lf");
        else {
            if (type->places <= 9)
                strcpy(spec, type->type == TYPE_SIGNED_NUMERIC || type->type == TYPE_SIGNED_SUPRESSED_NUMERIC ? "%d" : "%u");
            else
                strcpy(spec, type->type == TYPE_SIGNED_NUMERIC || type->type == TYPE_SIGNED_SUPRESSED_NUMERIC ? "%lld" : "%zu");
        }
    } else if (type->type == TYPE_DECIMAL_NUMERIC)
        sprintf(spec, "%%0%u.%ulf", type->places + type->decimal_places + 1, type->decimal_places);
    else if (type->type == TYPE_SIGNED_NUMERIC)
        sprintf(spec, "%%.%ud", type->places);
    else if (type->type == TYPE_UNSIGNED_NUMERIC)
        sprintf(spec, "%%.%uu", type->places);
    else if (type->type == TYPE_DECIMAL_SUPRESSED_NUMERIC)
        strcpy(spec, "%g");
    else if (type->type == TYPE_SIGNED_SUPRESSED_NUMERIC)
        strcpy(spec, type->places <= 9 ? "%d" : "%lld");
    else if (type->type == TYPE_UNSIGNED_SUPRESSED_NUMERIC)
        strcpy(spec, type->places <= 9 ? "%u" : "%llu");
    else if (type->count == 0)
        strcpy(spec, "%c");
    else
        sprintf(spec, "%%.%us", type->places);

    return spec;
}

char *emit_display(AST *ast) {
    AST *value = ast->display.value;
    char *arg = value_to_string(value);
    PictureType type = get_value_type(ast->display.value);

    /*
    if (ast->display.value->type == AST_SUBSCRIPT)
        type.count = 0;
        */

    char *spec = picturetype_to_format_specifier(&type);
    char *code = malloc(strlen(arg) + strlen(spec) + 42);
    sprintf(code, "printf(\"%s\", %s);\n", spec, arg);
    free(spec);

    if (ast->display.add_newline)
        strcat(code, "fputc('\\n', stdout);\n");

    free(arg);
    return code;
}

char *emit_pic(AST *ast) {
    const char *type = picturetype_to_c(&ast->pic.type);
    char *name;

    if (ast->pic.is_linkage_src && strlen(ast->pic.name) > 3 && ast->pic.name[0] == 'L' &&
        ast->pic.name[1] == 'S' && ast->pic.name[2] == '-') {

        // Remove 'LS-' remove linkage variable name.
        name = picturename_to_c(ast->pic.name + 3);
    } else
        name = picturename_to_c(ast->pic.name);

    char *code;

    if ((ast->pic.type.type == TYPE_ALPHABETIC || ast->pic.type.type == TYPE_ALPHANUMERIC) && ast->pic.type.count > 0)
        // Account for the null byte.
        ast->pic.type.count++;

    if (ast->pic.value == NULL) {
        code = malloc(strlen(name) + strlen(type) + 32);

        if (ast->pic.type.count > 0) {
            if (ast->pic.count > 0)
                sprintf(code, "%s %s[%u][%u];\n", type, name, ast->pic.count, ast->pic.type.count);
            else
                sprintf(code, "%s %s[%u];\n", type, name, ast->pic.type.count);
        } else if (ast->pic.count > 0)
            sprintf(code, "%s %s[%u];\n", type, name, ast->pic.count);
        else
            sprintf(code, "%s %s;\n", type, name);
    } else {
        char *value = value_to_string(ast->pic.value);
        code = malloc(strlen(name) + strlen(value) + strlen(type) + 32);

        if (ast->pic.is_fd)
            sprintf(code, "FILE *%s = NULL;\n", name);
        else if (ast->pic.type.count > 0) {
            if (ast->pic.count > 0)
                sprintf(code, "%s %s[%u][%u];\n", type, name, ast->pic.count, ast->pic.type.count);
            else
                sprintf(code, "%s %s[%u] = %s;\n", type, name, ast->pic.type.count, value);
        } else if (ast->pic.count > 0)
            sprintf(code, "%s %s[%u] = %s;\n", type, name, ast->pic.count, value);
        else
            sprintf(code, "%s %s = %s;\n", type, name, value);

        free(value);
    }

    free(name);

    if (ast->pic.is_linkage_src) {
        char *temp = malloc(strlen(code) + 10);
        sprintf(temp, "extern %s", code);
        free(code);
        code = temp;
    }

    append_global(code);
    free(code);
    return calloc(1, sizeof(char));
}

char *emit_move(AST *ast) {
    char *dst = value_to_string(ast->move.dst);
    char *src = value_to_string(ast->move.src);
    char *code;

    PictureType dst_type = get_value_type(ast->move.dst);
    PictureType src_type = get_value_type(ast->move.src);

    if (IS_STRING(dst_type) && IS_STRING(src_type)) {
        code = malloc(strlen(src) + strlen(dst) + 64);
        sprintf(code, "strncpy(%s, %s, %u);\n", dst, src, dst_type.count + 1);
    } else if (IS_STRING(dst_type)) {
        char *spec = picturetype_to_format_specifier(&src_type);
        code = malloc(strlen(src) + strlen(dst) + strlen(spec) + 64);
        sprintf(code, "snprintf(%s, %u, \"%s\", %s);\n", dst, dst_type.count + 1, spec, src);
        free(spec);
    } else if (IS_STRING(src_type)) {
        char *conv;

        if (dst_type.type == TYPE_SIGNED_NUMERIC)
            conv = "l";
        else if (dst_type.type == TYPE_UNSIGNED_NUMERIC)
            conv = "ul";
        else
            conv = "d";

        code = malloc((strlen(src) * 2) + strlen(dst) + strlen(conv) + 133);
        sprintf(code, "errno = 0;\n"
                      "%s = strto%s(%s, &endptr%s;\n"
                      "if (%s == endptr || *endptr != '\\0' || errno == ERANGE || errno == EINVAL)\ncobol_error();\n", 
                      dst, conv, src, dst_type.type == TYPE_DECIMAL_NUMERIC ? ")" : ", 10)", src);
    } else {
        code = malloc(strlen(src) + strlen(dst) + 10);
        sprintf(code, "%s = %s;\n", dst, src);
    }

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
        sprintf(code, "%s = %s / %s;\n", dst, left, right);
    else
        sprintf(code, "%s = (long long)%s %% %s;\n", dst, left, right);

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

char *oper_to_string(TokenType oper) {
    switch (oper) {
        case TOK_EQ:
        case TOK_EQUAL: return "==";
        case TOK_NEQ: return "!=";
        case TOK_LT: return "<";
        case TOK_LTE: return "<=";
        case TOK_GT: return ">";
        case TOK_GTE: return ">=";
        case TOK_AND: return "&&";
        default: break;
    }

    return "||";
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
            strcpy(value_string, oper_to_string(value->oper));
        } else {
            PictureType type = get_value_type(value);

            // Check for when we need to do strcmp().
            if (i + 2 < ast->condition.size && 
                    (type.type == TYPE_ALPHABETIC || type.type == TYPE_ALPHANUMERIC) && type.count > 0) {

                char *lhs = value_to_string(value);
                char *oper = oper_to_string(ast->condition.items[i + 1]->oper);
                char *rhs = value_to_string(ast->condition.items[i + 2]);

                value_string = malloc(strlen(lhs) + strlen(rhs) + strlen(oper) + 20);
                sprintf(value_string, "strcmp(%s, %s) %s 0", lhs, rhs, oper);

                free(lhs);
                free(rhs);

                i += 2; // Skip the rest of the condition values as we did them here.
            } else
                value_string = value_to_string(value);
        }

        const size_t value_len = strlen(value_string);

        if (values_len + value_len + 16 >= values_cap) {
            while (values_len + value_len + 15 >= values_cap)
                values_cap *= 2;

            values = realloc(values, values_cap);
        }

        strcat(values, value_string);
        free(value_string);
        values_len += value_len;

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
        sprintf(code, "if %s {\n%s} else {\n%s}\n", condition, body, else_body);
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
    char *code;
    
    if (ast->call.returning == NULL) {
        code = malloc(strlen(ast->call.name) + 8);
        sprintf(code, "%s(", ast->call.name);
    } else {
        char *returning = value_to_string(ast->call.returning);
        code = malloc(strlen(returning) + strlen(ast->call.name) + 32);
        sprintf(code, "%s = %s(", returning, ast->call.name);
        free(returning);
    }

    size_t len = strlen(code);

    for (size_t i = 0; i < ast->call.args.size; i++) {
        char *arg = value_to_string(ast->call.args.items[i]);
        const size_t arg_len = strlen(arg);

        code = realloc(code, len + arg_len + 7);
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

void emit_string_stmt_previous_size(StringStatement *stmt, char *value, char **increment, char **size, bool stmt_already_loaded) {
    // stmt_already_loaded means that the string value is already in string_builder,
    // we don't want to check stmt->value for constants.

    *increment = malloc((strlen(value) * 2) + 256);
    *size = malloc(strlen(value) + 128);

    if (!stmt_already_loaded && stmt->value->type == AST_STRING) {
        // Strings can be done at compile time.

        if (stmt->delimit == DELIM_SIZE) {
            sprintf(*increment, "previous_string_statement_size = %zu;\n", strlen(stmt->value->constant.string));
            sprintf(*size, "%zu", strlen(stmt->value->constant.string));
        } else {
            sprintf(*increment, "previous_string_statement_size = %zu;\n", strcspn(stmt->value->constant.string, " "));

            char *temp = strchr(stmt->value->constant.string, ' ');

            if (temp != NULL)
                sprintf(*size, "%zu", (size_t)(temp - stmt->value->constant.string));
            else
                sprintf(*size, "%zu", strlen(stmt->value->constant.string));
        }
    } else if (stmt->delimit == DELIM_SIZE) {
        sprintf(*increment, "previous_string_statement_size = strlen(%s);\n", value);
        sprintf(*size, "strlen(%s)", value);
    } else {
        // The +1 at the end of the increment is important because it skips the space character,
        // otherwise the next search would start at the space character, and return 0 size.
        sprintf(*increment, "endptr = strchr(string_builder + string_builder_pointer, ' ');\n"
                            "if (endptr != NULL)\nprevious_string_statement_size = endptr - (string_builder + string_builder_pointer);\n"
                            "else\nprevious_string_statement_size = strlen(%s);\n", value);

        strcpy(*size, "previous_string_statement_size");
    }
}

char *emit_string_stmt(StringStatement *stmt) {
    char *value = value_to_string(stmt->value);
    char *increment = NULL;
    char *size = NULL;
    emit_string_stmt_previous_size(stmt, value, &increment, &size, false);

    assert(increment != NULL);
    assert(size != NULL);

    char *code = malloc(strlen(value) + strlen(increment) + strlen(size) + 36);

    // Avoid stringop-overflow error from using strncat with string literals delimited by size.
    if (stmt->value->type == AST_STRING && stmt->delimit == DELIM_SIZE)
        sprintf(code, "strcat(string_builder, %s);\n"
                      "%s", value, increment);
    // Some size statements require setting up the endptr variable for searching for a space.
    else if (stmt->delimit == DELIM_SPACE)
        sprintf(code, "%s"
                      "strncat(string_builder, %s, %s);\n", increment, value, size);
    else
        sprintf(code, "strncat(string_builder, %s, %s);\n"
                      "%s", value, size, increment);

    free(value);
    free(increment);
    free(size);
    return code;
}

char *emit_unstring(AST *ast) {
    char *base = value_to_string(ast->string_splitter.base.value);
    char *code = malloc(strlen(base) + 77);
    sprintf(code, "string_builder[0] = string_builder_pointer = 0;\n"
                  "strcpy(string_builder, %s);\n", base);

    size_t code_len = strlen(code);

    char *increment;
    char *size;
    emit_string_stmt_previous_size(&ast->string_splitter.base, base, &increment, &size, true);
    assert(increment != NULL);
    assert(size != NULL);

    for (size_t i = 0; i < ast->string_splitter.into_vars.size; i++) {
        char *var = value_to_string(ast->string_splitter.into_vars.items[i]);
        PictureType type = get_value_type(ast->string_splitter.into_vars.items[i]);
        char *into = malloc(strlen(var) + strlen(increment) + (strlen(size) * 2) + 420);
        sprintf(into, "%s"
                      "strncpy(%s, string_builder + string_builder_pointer, %s >= %u ? %u : %s);\n"
                      "string_builder_pointer += previous_string_statement_size + 1;\n", increment, var, size, type.count + 1, type.count + 1, size);

        free(var);
        const size_t len = strlen(into);

        code = realloc(code, code_len + len + 1);
        strcat(code, into);
        free(into);
        code_len += len;
    }

    free(base);
    free(size);
    free(increment);
    return code;
}

char *emit_string_builder(AST *ast) {
    char *base = emit_string_stmt(&ast->string_builder.base);
    char *code = malloc(strlen(base) + 156);
    sprintf(code, "string_builder[0] = string_builder_pointer = 0;\n"
                  "%s"
                  "string_builder_pointer += previous_string_statement_size;\n"
                  "string_builder[string_builder_pointer] = '\\0';\n", base);

    free(base);
    size_t code_len = strlen(code);

    for (size_t i = 0; i < ast->string_builder.stmt_count; i++) {
        char *stmt = emit_string_stmt(&ast->string_builder.stmts[i]);
        const size_t len = strlen(stmt);

        code = realloc(code, code_len + len + 207);
        strcat(code, stmt);
        free(stmt);

        strcat(code, "string_builder_pointer += previous_string_statement_size;\n"
                     "string_builder[string_builder_pointer] = '\\0';\n");
        code_len += len + 106;
    }
    
    char *var = value_to_string(ast->string_builder.into_var);
    PictureType vartype = get_value_type(ast->string_builder.into_var);
    char *into = malloc(strlen(var) + 64);

    sprintf(into, "strncpy(%s, string_builder, %u);\n", var, vartype.count + 1);
    free(var);
    size_t len = strlen(into);

    code = realloc(code, code_len + len + 1);
    strcat(code, into);
    free(into);

    if (ast->string_builder.with_pointer == NULL)
        return code;

    char *pointer = value_to_string(ast->string_builder.with_pointer);
    code = realloc(code, code_len + len + strlen(pointer) + 28);
    strcat(code, pointer);
    free(pointer);
    strcat(code, " = string_builder_pointer;\n");
    return code;
}

char *emit_open(AST *ast) {
    char *var = value_to_string(ast->open.filename);
    char *name = picturename_to_c(ast->open.filename->var.name);
    char *mode;

    if (ast->open.type == OPEN_INPUT)
        mode = "r";
    else if (ast->open.type == OPEN_OUTPUT)
        mode = "w";
    else if (ast->open.type == OPEN_IO)
        mode = "w+";
    else
        mode = "a";

    // TODO: Implement all file status errors, 37 is just for
    // FILE NOT OPEN, which is usually for wrong modes, but there are others.
    char *code = malloc(strlen(var) + strlen(mode) + (strlen(name) * 4) + 101);

    if (ast->open.type == OPEN_INPUT)
        sprintf(code, "%s = fopen(%sFILENAME, \"%s\");\n"
                      "strcpy(%sSTATUS, %s != NULL ? \"00\" : \"37\");\n", name, var, mode, name, name);

    // Need to assign the last opened output file for WRITEs with OUTPUT, IO or EXTEND.
    else
        sprintf(code, "%s = fopen(%sFILENAME, \"%s\");\n"
                      "strcpy(%sSTATUS, %s != NULL ? \"00\" : \"37\");\n"
                      "last_opened_outfile = %s;\n", name, var, mode, name, name, name);

    free(var);
    free(name);
    return code;
}

char *emit_close(AST *ast) {
    char *name = picturename_to_c(ast->close_filename->var.name);
    char *code = malloc(strlen(name) + 17);
    sprintf(code, "fclose(%s);\n", name);
    free(name);
    return code;
}

char *emit_select(AST *ast) {
    char *name = picturename_to_c(ast->select.fd_var->var.name);
    char *filestatus = ast->select.filestatus_var == NULL ? calloc(1, sizeof(char)) : picturename_to_c(ast->select.filestatus_var->var.name);
    char *filename = value_to_string(ast->select.filename);

    char *code = malloc((strlen(name) * 2) + strlen(filename) + strlen(filestatus) + 52);

    if (ast->select.filestatus_var == NULL)
    // Copy into a junk buffer since a file status variable wasn't specified.
        sprintf(code, "#define %sFILENAME %s\n"
                      "#define %sSTATUS file_status\n", name, filename, name);
    else
        sprintf(code, "#define %sFILENAME %s\n"
                      "#define %sSTATUS %s\n", name, filename, name, filestatus);

    free(filename);
    free(name);
    free(filestatus);
    append_global(code);

    free(code);
    return calloc(1, sizeof(char));
}

char *emit_read(AST *ast) {
    char *fd = picturename_to_c(ast->read.fd->var.name);
    char *into = picturename_to_c(ast->read.into->var.name);
    char *at_end = emit_list(&ast->read.at_end_stmts);
    char *not_at_end = emit_list(&ast->read.not_at_end_stmts);
    char *code = malloc(strlen(fd) + (strlen(into) * 4) + strlen(at_end) + strlen(not_at_end) + 109);

    // Note: we also do strcspn() which removes any trailing newlines if present.

    if (ast->read.at_end_stmts.size == 0) {
        if (ast->read.not_at_end_stmts.size == 0)
            sprintf(code, "read_buffer = fgets(%s, sizeof(%s) - 1, %s);\n"
                           "%s[strcspn(%s, \"\\n\\r\")] = '\\0';\n", into, into, fd, into, into);
        else
            sprintf(code, "read_buffer = fgets(%s, sizeof(%s) - 1, %s);\n"
                           "%s[strcspn(%s, \"\\n\\r\")] = '\\0';\n"
                          "if (read_buffer != NULL) {\n%s}\n", into, into, fd, into, into, not_at_end);
    } else {
        if (ast->read.not_at_end_stmts.size != 0)
            sprintf(code, "read_buffer = fgets(%s, sizeof(%s) - 1, %s);\n"
                           "%s[strcspn(%s, \"\\n\\r\")] = '\\0';\n"
                          "if (read_buffer == NULL) {\n%s} else {\n%s}\n", into, into, fd, into, into, at_end, not_at_end);
        else
            sprintf(code, "read_buffer = fgets(%s, sizeof(%s) - 1, %s);\n"
                           "%s[strcspn(%s, \"\\n\\r\")] = '\\0';\n"
                          "if (read_buffer == NULL) {\n%s}\n", into, into, fd, into, into, at_end);
    }

    free(fd);
    free(into);
    free(at_end);
    free(not_at_end);
    return code;
}

char *emit_write(AST *ast) {
    char *value = value_to_string(ast->write.value);
    PictureType type = get_value_type(ast->write.value);
    char *spec = picturetype_to_format_specifier(&type);

    char *code = malloc(strlen(value) + strlen(spec) + 41);
    sprintf(code, "fprintf(last_opened_outfile, \"%s\", %s);\n", spec, value);

    free(spec);
    free(value);
    return code;
}

char *emit_stringtally_phase1(StringTallyPhase1 *phase) {
    char *value = value_to_string(phase->value);
    char *modifier = phase->modifier == NULL ? mystrdup(value) : value_to_string(phase->modifier);
    char *code = malloc(strlen(value) + strlen(modifier) + 128);

    if (phase->before)
        sprintf(code, "if (inspect_char == %s)\nbreak;\nelse if (inspect_char == %s)\ninspect_count++;\n", modifier, value);
    else
        sprintf(code, "if (inspect_char == %s && !inspect_found)\ninspect_found = true;\nif (inspect_char == %s && inspect_found)\ninspect_count++;\n", modifier, value);

    free(value);
    free(modifier);
    return code;
}

char *emit_stringtally_for_all(StringTally *tally) {
    char *phases = emit_stringtally_phase1(&tally->phase);
    char *code = malloc(strlen(phases) + 126);

    if (tally->phase.after)
        sprintf(code, "inspect_found = false;\nfor (size_t i = 0; i < inspect_string_length; i++) {\nconst char inspect_char = inspect_string[i];\n%s}\n", phases);
    else
        sprintf(code, "for (size_t i = 0; i < inspect_string_length; i++) {\nconst char inspect_char = inspect_string[i];\n%s}\n", phases);

    free(phases);
    return code;
}

char *emit_stringtally(StringTally *tally) {
    if (tally->type == TALLY_CHARACTERS)
        return mystrdup("inspect_count += inspect_string_length;\n");

    char *code;

    if (tally->type == TALLY_ALL)
        code = emit_stringtally_for_all(tally);
    else {
        assert(false);
        return calloc(1, sizeof(char));
    }

    char *output = value_to_string(tally->output_count);
    code = realloc(code, (strlen(code) + strlen(output) + 20));
    strcat(code, output);
    strcat(code, " = inspect_count;\n");
    free(output);
    return code;
}

char *emit_stringreplace(StringReplace *replace) {
    char *old = value_to_string(replace->old);
    char *new = value_to_string(replace->new);
    char *modifier = replace->modifier == NULL ? calloc(1, sizeof(char)) : value_to_string(replace->modifier);
    char *code = malloc(strlen(old) + strlen(new) + strlen(modifier) + 163);

    if (replace->type == REPLACING_FIRST) {
        if (replace->before)
            sprintf(code, "if (inspect_char == %s)\ninspect_found = true;\nelse if (!inspect_found && !inspect_locked && inspect_char == %s) {\ninspect_string[i] = %s;\ninspect_locked = true;\n}\n", modifier, old, new);
        else if (replace->after)
            sprintf(code, "if (inspect_char == %s)\ninspect_found = true;\nelse if (inspect_found && !inspect_locked && inspect_char == %s) {\ninspect_string[i] = %s;\ninspect_locked = true;\n}\n", modifier, old, new);
        else
            sprintf(code, "if (inspect_char == %s && !inspect_locked) {\ninspect_string[i] = %s;\ninspect_locked = true;\n}\n", old, new);
    } else {
        if (replace->before)
            sprintf(code, "if (inspect_char == %s)\ninspect_found = true;\nelse if (!inspect_found && inspect_char == %s)\ninspect_string[i] = %s;\n", modifier, old, new);
        else if (replace->after)
            sprintf(code, "if (inspect_char == %s)\ninspect_found = true;\nelse if (inspect_found && inspect_char == %s)\ninspect_string[i] = %s;\n", modifier, old, new);
        else
            sprintf(code, "if (inspect_char == %s)\ninspect_string[i] = %s;\n", old, new);
    }

    free(old);
    free(new);
    free(modifier);
    return code;
}

char *emit_inspect_combine(AST *ast, char *stmt_code) {
    char *input_string = value_to_string(ast->inspect.input_string);
    char *code = malloc(strlen(input_string) + strlen(stmt_code) + 91);
    sprintf(code, "inspect_string = %s;\n"
                  "inspect_string_length = strlen(inspect_string);\n"
                  "inspect_count = 0;\n"
                  "%s", input_string, stmt_code);

    free(stmt_code);
    free(input_string);
    return code;
}

char *emit_inspect_tallying(AST *ast) {
    char *tallies = calloc(1, sizeof(char));
    size_t tally_len = 0;

    for (size_t i = 0; i < ast->inspect.tallying.tally_count; i++) {
        char *tally = emit_stringtally(&ast->inspect.tallying.tallies[i]);
        const size_t len = strlen(tally);

        tallies = realloc(tallies, tally_len + len + 1);
        strcat(tallies, tally);
        free(tally);
        tally_len += len;
    }

    return emit_inspect_combine(ast, tallies);
}

char *emit_inspect_replacing(AST *ast) {
    char *replaces = calloc(1, sizeof(char));
    size_t replaces_len = 0;

    for (size_t i = 0; i < ast->inspect.replacing.replace_count; i++) {
        char *replace = emit_stringreplace(&ast->inspect.replacing.replaces[i]);
        const size_t len = strlen(replace);

        replaces = realloc(replaces, replaces_len + len + 1);
        strcat(replaces, replace);
        free(replace);
        replaces_len += len;
    }

    char *code = malloc(strlen(replaces) + 143);
    sprintf(code, "inspect_found = inspect_locked = false;\nfor (size_t i = 0; i < inspect_string_length; i++) {\nconst char inspect_char = inspect_string[i];\n%s}\n", replaces);
    free(replaces);
    return emit_inspect_combine(ast, code);
}

char *emit_inspect(AST *ast) {
    if (ast->inspect.type == INSPECT_TALLYING)
        return emit_inspect_tallying(ast);
    else if (ast->inspect.type == INSPECT_REPLACING)
        return emit_inspect_replacing(ast);

    assert(false);
    return calloc(1, sizeof(char));
}

char *emit_accept_argv(AST *ast) {
    char *dst = value_to_string(ast->accept.dst);
    char *code = malloc((strlen(dst) * 3) + 116);

    sprintf(code, "strcpy(%s, global_argv[0]);\n"
                  "for (int i = 1; i < global_argc; i++) {\n"
                  "strcat(%s, \" \");\n"
                  "strcat(%s, global_argv[i]);\n"
                  "}\n", dst, dst, dst);

    free(dst);
    return code;
}

char *emit_accept(AST *ast) {
    assert(ast->accept.dst->type == AST_VAR);

    if (ast->accept.from != NULL && ast->accept.from->type == AST_ARGV)
        return emit_accept_argv(ast);

    PictureType type = get_value_type(ast->accept.dst);
    char *dst = value_to_string(ast->accept.dst);
    char *code = malloc((strlen(dst) * 3) + 238);

    // Also removes the trailing newline if found.
    // TODO: Use read_buffer to check for shit?
    if ((type.type == TYPE_ALPHABETIC || type.type == TYPE_ALPHANUMERIC) && type.count > 0)
        sprintf(code, "read_buffer = fgets(%s, %u, stdin);\n"
                      "%s[strcspn(%s, \"\\n\")] = '\\0';\n", dst, type.count + 1, dst, dst);
    else if (type.type == TYPE_DECIMAL_NUMERIC)
        sprintf(code, "read_buffer = fgets(string_builder, 4095, stdin);\n"
                      "string_builder[strcspn(string_builder, \"\\n\")] = '\\0';\n"
                      "%s = strtold(string_builder, &endptr);\n"
                      "if (string_builder == endptr || *endptr != '\\0' || errno == ERANGE || errno == EINVAL)\ncobol_error();\n", dst);
    else
        sprintf(code, "read_buffer = fgets(string_builder, 4095, stdin);\n"
                      "string_builder[strcspn(string_builder, \"\\n\")] = '\\0';\n"
                      "%s = strto%s(string_builder, &endptr, 10);\n"
                      "if (string_builder == endptr || *endptr != '\\0' || errno == ERANGE || errno == EINVAL)\ncobol_error();\n", dst, type.type == TYPE_SIGNED_NUMERIC ? "l" : "ul");


    free(dst);
    return code;
}

char *emit_lengthof(AST *ast) {
    char *value = value_to_string(ast->lengthof_value);
    char *code = malloc(strlen(value) + 11);
    sprintf(code, "strlen(%s)", value);
    free(value);
    return code;
}

char *emit_stmt(AST *ast) {
    switch (ast->type) {
        case AST_NOP: return calloc(1, sizeof(char));
        case AST_STOP_RUN: return emit_stop_run(ast);
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
        case AST_PERFORM: return emit_perform(ast);
        case AST_PROC: return emit_procedure(ast);
        case AST_PERFORM_CONDITION: return emit_perform_condition(ast);
        case AST_PERFORM_COUNT: return emit_perform_count(ast);
        case AST_PERFORM_VARYING: return emit_perform_varying(ast);
        case AST_PERFORM_UNTIL: return emit_perform_until(ast);
        case AST_SUBSCRIPT: return emit_subscript(ast);
        case AST_CALL: return emit_call(ast);
        case AST_STRING_BUILDER: return emit_string_builder(ast);
        case AST_STRING_SPLITTER: return emit_unstring(ast);
        case AST_OPEN: return emit_open(ast);
        case AST_CLOSE: return emit_close(ast);
        case AST_SELECT: return emit_select(ast);
        case AST_READ: return emit_read(ast);
        case AST_WRITE: return emit_write(ast);
        case AST_INSPECT: return emit_inspect(ast);
        case AST_ACCEPT: return emit_accept(ast);
        case AST_EXIT: return mystrdup("exit(EXIT_SUCCESS);\n");
        case AST_LENGTHOF: return emit_lengthof(ast);
        default: break;
    }

    printf(">>>>%s\n", asttype_to_string(ast->type));
    assert(false);
    return calloc(1, sizeof(char));
}
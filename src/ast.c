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
        case AST_DISPLAY:
            delete_ast(ast->display.value);
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
        case AST_ARITHMETIC:
            if (!ast->arithmetic.cloned_left)
                delete_ast(ast->arithmetic.left);

            if (!ast->arithmetic.cloned_right)
                delete_ast(ast->arithmetic.right);

            if (!ast->arithmetic.implicit_giving || strcmp(ast->arithmetic.name, "REMAINDER") == 0)
                delete_ast(ast->arithmetic.dst);

            free(ast->arithmetic.name);
            break;
        case AST_COMPUTE:
            delete_ast(ast->compute.dst);
            delete_ast(ast->compute.math);
            break;
        case AST_MATH:
            delete_astlist(&ast->math);
            break;
        case AST_PARENS:
            delete_ast(ast->parens);
            break;
        case AST_CONDITION:
            delete_astlist(&ast->condition);
            break;
        case AST_IF:
            delete_ast(ast->if_stmt.condition);
            delete_astlist(&ast->if_stmt.body);
            delete_astlist(&ast->if_stmt.else_body);
            break;
        case AST_NOT:
            delete_ast(ast->not_value);
            break;
        case AST_LABEL:
            free(ast->label);
            break;
        case AST_PERFORM:
            delete_ast(ast->perform);
            break;
        case AST_PROC:
            free(ast->proc.name);
            delete_astlist(&ast->proc.body);
            break;
        case AST_BLOCK:
            delete_astlist(&ast->block);
            break;
        case AST_PERFORM_CONDITION:
            delete_ast(ast->perform_condition.proc);
            delete_ast(ast->perform_condition.condition);
            break;
        case AST_PERFORM_COUNT:
            delete_ast(ast->perform_count.proc);
            break;
        case AST_PERFORM_VARYING:
            delete_ast(ast->perform_varying.var);
            delete_ast(ast->perform_varying.by);
            delete_ast(ast->perform_varying.from);
            delete_ast(ast->perform_varying.until);
            delete_astlist(&ast->perform_varying.body);
            break;
        case AST_PERFORM_UNTIL:
            delete_ast(ast->perform_until.until);
            delete_astlist(&ast->perform_until.body);
            break;
        case AST_SUBSCRIPT:
            delete_ast(ast->subscript.base);
            delete_ast(ast->subscript.index);

            if (ast->subscript.value != NULL)
                delete_ast(ast->subscript.value);
            break;
        case AST_CALL:
            free(ast->call.name);
            delete_astlist(&ast->call.args);

            if (ast->call.returning != NULL)
                delete_ast(ast->call.returning);
            break;
        case AST_STRING_BUILDER:
            delete_ast(ast->string_builder.base.value);

            for (size_t i = 0; i < ast->string_builder.stmt_count; i++)
                delete_ast(ast->string_builder.stmts[i].value);

            free(ast->string_builder.stmts);
            delete_ast(ast->string_builder.into_var);

            if (ast->string_builder.with_pointer != NULL)
                delete_ast(ast->string_builder.with_pointer);
            break;
        case AST_STRING_SPLITTER:
            delete_ast(ast->string_splitter.base.value);
            delete_astlist(&ast->string_splitter.into_vars);
            break;
        case AST_OPEN:
            delete_ast(ast->open.filename);
            break;
        case AST_CLOSE:
            delete_ast(ast->close_filename);
            break;
        case AST_SELECT:
            delete_ast(ast->select.fd_var);
            delete_ast(ast->select.filename);
            
            if (ast->select.filestatus_var != NULL)
                delete_ast(ast->select.filestatus_var);
            break;
        case AST_READ:
            delete_ast(ast->read.fd);
            delete_ast(ast->read.into);
            delete_astlist(&ast->read.at_end_stmts);
            delete_astlist(&ast->read.not_at_end_stmts);
            break;
        case AST_WRITE:
            delete_ast(ast->write.value);
            break;
        case AST_INSPECT:
            delete_ast(ast->inspect.input_string);

            if (ast->inspect.type == INSPECT_TALLYING) {
                for (size_t i = 0; i < ast->inspect.tallying.tally_count; i++) {
                    StringTally *tally = &ast->inspect.tallying.tallies[i];

                    delete_ast(tally->phase.value);

                    if (tally->phase.modifier != NULL)
                        delete_ast(tally->phase.modifier);

                    delete_ast(tally->output_count);
                }

                free(ast->inspect.tallying.tallies);
            } else if (ast->inspect.type == INSPECT_REPLACING) {
                for (size_t i = 0; i < ast->inspect.replacing.replace_count; i++) {
                    StringReplace *replace = &ast->inspect.replacing.replaces[i];

                    delete_ast(replace->old);
                    delete_ast(replace->new);

                    if (replace->modifier != NULL)
                        delete_ast(replace->modifier);
                }

                free(ast->inspect.replacing.replaces);
            }
            break;
        case AST_ACCEPT:
            delete_ast(ast->accept.dst);

            if (ast->accept.from != NULL)
                delete_ast(ast->accept.from);
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
        case AST_STOP_RUN:
        case AST_STOP: return "stop";
        case AST_DISPLAY: return "display";
        case AST_PIC: return "picture";
        case AST_MOVE: return "move";
        case AST_ARITHMETIC: return "arithmetic";
        case AST_OPER: return "operator";
        case AST_COMPUTE: return "compute";
        case AST_MATH: return "math";
        case AST_PARENS: return "parentheses";
        case AST_CONDITION: return "condition";
        case AST_IF: return "if";
        case AST_NOT: return "not";
        case AST_LABEL: return "label";
        case AST_PERFORM: return "perform";
        case AST_PERFORM_CONDITION: return "perform until";
        case AST_PERFORM_COUNT: return "perform times";
        case AST_PERFORM_VARYING: return "perform varying";
        case AST_PERFORM_UNTIL: return "perform until";
        case AST_PROC: return "procedure";
        case AST_BLOCK: return "block";
        case AST_SUBSCRIPT: return "subscript";
        case AST_CALL: return "call";
        case AST_NULL: return "null";
        case AST_BOOL: return "bool";
        case AST_STRING_BUILDER: return "string";
        case AST_STRING_SPLITTER: return "unstring";
        case AST_OPEN: return "open";
        case AST_CLOSE: return "close";
        case AST_SELECT: return "select";
        case AST_READ: return "read";
        case AST_WRITE: return "write";
        case AST_INSPECT: return "inspect";
        case AST_ACCEPT: return "accept";
        case AST_ZERO: return "zero";
        case AST_ARGV: return "argv";
        case AST_EXIT: return "exit";
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
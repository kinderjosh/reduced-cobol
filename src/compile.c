#include "compile.h"
#include "utils.h"
#include "parser.h"
#include "ast.h"
#include "transpiler.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

extern char *cc_path;

int compile(char *infile, char *outfile, unsigned int flags, char *libs, char *source_includes) {
    AST *root = parse_file(infile);

    if (error_count() > 0) {
        delete_ast(root);
        return EXIT_FAILURE;
    }

    char *code = emit_root(root, !(flags & COMP_NO_MAIN), source_includes);
    delete_ast(root);

    char *outc = replace_file_extension((flags & COMP_SOURCE_ONLY) && !(flags & COMP_OUTFILE_SPECIFIED) ? infile : outfile, "c", true);

    FILE *out = fopen(outc, "w");

    if (out == NULL) {
        log_error(infile, 0, 0);
        fprintf(stderr, "failed to write to file '%s'\n", outc);
        free(outc);
        free(code);
        return EXIT_FAILURE;
    }

    fputs(code, out);
    fclose(out);
    free(code);

    if (flags & COMP_SOURCE_ONLY) {
        free(outc);
        return EXIT_SUCCESS;
    }

    char *cmd;

    if (flags & COMP_OBJECT) {
        char *objfile = (flags & COMP_OUTFILE_SPECIFIED) ? mystrdup(outfile) : replace_file_extension(infile, "o", true);
        cmd = malloc(strlen(cc_path) + strlen(objfile) + strlen(outc) + strlen(libs) + 24);
        sprintf(cmd, "%s -c -o %s %s %s", cc_path, objfile, outc, libs);
        free(objfile);
    } else {
        cmd = malloc(strlen(cc_path) + strlen(outfile) + strlen(outc) + strlen(libs) + 21);
        sprintf(cmd, "%s -o %s %s %s", cc_path, outfile, outc, libs);
    }

    int status = system(cmd);

    if (remove(outc) != 0) {
        log_error(infile, 0, 0);
        fprintf(stderr, "failed to remove '%s'\n", outc);
        free(outc);
        free(cmd);
        return status;
    }

    free(outc);

    if (status != 0) {
        log_error(infile, 0, 0);
        fprintf(stderr, "failed to compile\n");
        free(cmd);
        return status;
    }

    if (!(flags & COMP_RUN)) {
        free(cmd);
        return EXIT_SUCCESS;
    }

    sprintf(cmd, "./%s", outfile);
    status = system(cmd);
    free(cmd);
    return status;
}
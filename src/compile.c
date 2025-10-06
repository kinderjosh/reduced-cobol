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
#include <assert.h>

#define RELEASE_CFLAGS "-std=c99 -O1"
#define DEBUG_CFLAGS "-std=c99 -g"

extern char *cc_path;

char *cur_dir;

void extract_cur_dir_and_basefile(char *path, char **out_filename);

int compile(char *infile, char *outfile, unsigned int flags, char *libs, char *source_includes) {
    char *basefile = NULL;
    extract_cur_dir_and_basefile(infile, &basefile);
    assert(basefile != NULL);

    AST *root = parse_file(infile);

    if (error_count() > 0) {
        delete_ast(root);
        free(basefile);
        return EXIT_FAILURE;
    }

    char *code = emit_root(root, !(flags & COMP_NO_MAIN), source_includes);
    delete_ast(root);

    char *outc = replace_file_extension((flags & COMP_SOURCE_ONLY) && !(flags & COMP_OUTFILE_SPECIFIED) ? basefile : outfile, "c", true);

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
        free(basefile);
        return EXIT_SUCCESS;
    }

    char *cmd;
    char *cflags = (flags & COMP_DEBUG) ? DEBUG_CFLAGS : RELEASE_CFLAGS;

    if (flags & COMP_OBJECT) {
        char *objfile = (flags & COMP_OUTFILE_SPECIFIED) ? mystrdup(outfile) : replace_file_extension(basefile, "o", true);
        cmd = malloc(strlen(cc_path) + strlen(cflags) + strlen(objfile) + strlen(outc) + strlen(libs) + 24);
        sprintf(cmd, "%s %s -c -o %s %s %s", cc_path, cflags, objfile, outc, libs);
        free(objfile);
    } else {
        cmd = malloc(strlen(cc_path) + strlen(cflags) + strlen(outfile) + strlen(outc) + strlen(libs) + 18);
        sprintf(cmd, "%s %s -o %s %s %s", cc_path, cflags, outfile, outc, libs);
    }

    free(basefile);

    int status = system(cmd);

    if (!(flags & COMP_DEBUG) && remove(outc) != 0) {
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

#ifdef _WIN32
    sprintf(cmd, ".\\%s", outfile);
#else
    sprintf(cmd, "./%s", outfile);
#endif

    status = system(cmd);
    free(cmd);
    //printf("%scobc: %srun: %s%s exited with code %d\n", ESC_BOLD, ESC_MAGENTA, ESC_NORMAL, outfile, status);
    return status;
}

void extract_cur_dir_and_basefile(char *path, char **out_filename) {
    char *copy = mystrdup(path);
    char *delim;

#ifdef _WIN32
    delim = "\\";
#else
    delim = "/";
#endif

    char *tok = strtok(copy, delim);
    char *prev_tok = tok;
    cur_dir = calloc(1, sizeof(char));

    while (tok != NULL) {
        prev_tok = tok;
        tok = strtok(NULL, delim);

        if (tok != NULL) {
            cur_dir = realloc(cur_dir, strlen(cur_dir) + strlen(prev_tok) + 2);
            strcat(cur_dir, prev_tok);
            strcat(cur_dir, delim);
        }
    }
    
    *out_filename = mystrdup(prev_tok);
    free(copy);
}
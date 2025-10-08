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

int compile_one_file(AST *root, char *basefile, char *infile, char *outfile, unsigned int flags, char *libs, char *source_includes, char **out_finalfile);

int compile(char **infiles, size_t infile_count, char *outfile, unsigned int flags, char *libs, char *source_includes) {
    int status = EXIT_SUCCESS;
    bool source_only = (flags & COMP_SOURCE_ONLY);
    char *cmd;
    size_t cmd_len;
    char *rm_cmd;
    size_t rm_cmd_len;
    bool found_main = false;

    bool run_exec = (flags & COMP_RUN);
    flags &= ~COMP_RUN;

    if (!source_only) {
        // Compile all files to objects then link them all together for the final exectuable.
        flags |= COMP_OBJECT;

        cmd = malloc(strlen(outfile) + 10);
        sprintf(cmd, "gcc -o %s", outfile);
        cmd_len = strlen(cmd);

#ifdef _WIN32
        rm_cmd = mystrdup("del");
        rm_cmd_len = 3;
#else
        rm_cmd = mystrdup("rm");
        rm_cmd_len = 2;
#endif
    }

    if (source_only || (flags & COMP_DEBUG))
        // Don't name sources 'a.c'.
        flags &= ~COMP_OUTFILE_SPECIFIED;

    for (size_t i = 0; i < infile_count; i++) {
        char *basefile = NULL;
        extract_cur_dir_and_basefile(infiles[i], &basefile);
        assert(basefile != NULL);

        AST *root = parse_file(infiles[i], infiles, &found_main);
        free(cur_dir);

        char *finalfile = NULL;

        if (found_main)
            status += compile_one_file(root, basefile, infiles[i], outfile, flags, libs, source_includes, &finalfile);
        else
            status += compile_one_file(root, basefile, infiles[i], outfile, flags | COMP_NO_MAIN, libs, source_includes, &finalfile);

        assert(finalfile != NULL);

        if (source_only) {
            free(finalfile);
            continue;
        }

        const size_t len = strlen(finalfile);

        cmd = realloc(cmd, cmd_len + len + 2);
        strcat(cmd, " ");
        strcat(cmd, finalfile);
        cmd_len += len + 1;

        rm_cmd = realloc(rm_cmd, rm_cmd_len + len + 2);
        strcat(rm_cmd, " ");
        strcat(rm_cmd, finalfile);
        rm_cmd_len += len + 1;

        free(finalfile);
    }

    if (source_only)
        return status;

    if (system(cmd) != 0) {
        log_error(NULL, 0, 0);
        fprintf(stderr, "failed to compile\n");
        status = EXIT_FAILURE;
    }

    free(cmd);

    if (system(rm_cmd) != 0) {
        log_error(NULL, 0, 0);
        fprintf(stderr, "failed to remove objects\n");
        status = EXIT_FAILURE;
    }

    free(rm_cmd);

    if (!run_exec)
        return status;

    cmd = malloc(strlen(outfile) + 3);

#ifdef _WIN32
    sprintf(cmd, ".\\%s", outfile);
#else
    sprintf(cmd, "./%s", outfile);
#endif

    int run_status = system(cmd);
    (void)run_status; // Don't care about the result.
    free(cmd);
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

int compile_one_file(AST *root, char *basefile, char *infile, char *outfile, unsigned int flags, char *libs, char *source_includes, char **out_finalfile) {
    if (error_count() > 0) {
        delete_ast(root);
        *out_finalfile = basefile;
        return EXIT_FAILURE;
    }

    char *code = emit_root(root, !(flags & COMP_NO_MAIN), source_includes);
    delete_ast(root);

    char *outc = replace_file_extension(!(flags & COMP_OUTFILE_SPECIFIED) ? basefile : outfile, "c", true);

    FILE *out = fopen(outc, "w");

    if (out == NULL) {
        log_error(infile, 0, 0);
        fprintf(stderr, "failed to write to file '%s'\n", outc);
        free(code);
        *out_finalfile = outc;
        return EXIT_FAILURE;
    }

    fputs(code, out);
    fclose(out);
    free(code);

    if (flags & COMP_SOURCE_ONLY) {
        free(basefile);
        *out_finalfile = outc;
        return EXIT_SUCCESS;
    }

    char *cmd;
    char *cflags = (flags & COMP_DEBUG) ? DEBUG_CFLAGS : RELEASE_CFLAGS;

    if (flags & COMP_OBJECT) {
        char *objfile = (flags & COMP_OUTFILE_SPECIFIED) ? mystrdup(outfile) : replace_file_extension(basefile, "o", true);
        cmd = malloc(strlen(cc_path) + strlen(cflags) + strlen(objfile) + strlen(outc) + strlen(libs) + 24);
        sprintf(cmd, "%s %s -c -o %s %s %s", cc_path, cflags, objfile, outc, libs);
        *out_finalfile = objfile;
    } else {
        cmd = malloc(strlen(cc_path) + strlen(cflags) + strlen(outfile) + strlen(outc) + strlen(libs) + 18);
        sprintf(cmd, "%s %s -o %s %s %s", cc_path, cflags, outfile, outc, libs);
        *out_finalfile = mystrdup(outfile);
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
    return status;
}
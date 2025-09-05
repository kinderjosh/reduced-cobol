#include "compile.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

char *cc_path = "gcc";

void usage(const char *prog) {
    printf("usage: %s <command> [options] <input file>\n"
           "commands:\n"
           "    build               produce an executable\n"
           "    object              produce an object file\n"
           "    source              produce a c file\n"
           "    run                 build and run the executable\n"
           "options:\n"
           "    -g                  build with debugging information\n"
           "    -include <header>   include a c header\n"
           "    -l <library>        link with a c library\n"
           "    -no-main            don't add a main function\n"
           "    -o <output file>    specify the output filename\n", prog);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char *command = argv[1];
    unsigned int flags = 0;

    if (strcmp(command, "--help") == 0) {
        usage(argv[0]);
        return EXIT_SUCCESS;
    } else if (strcmp(command, "source") == 0)
        flags |= COMP_SOURCE_ONLY;
    else if (strcmp(command, "object") == 0)
        flags |= COMP_OBJECT;
    else if (strcmp(command, "run") == 0)
        flags |= COMP_RUN;
    else if (strcmp(command, "build") != 0) {
        log_error(NULL, 0, 0);
        fprintf(stderr, "unknown command '%s'\n", command);
        return EXIT_FAILURE;
    }

#ifdef _WIN32
    char *outfile = "a.exe";
#else
    char *outfile = "a.out";
#endif

    char *infile = NULL;
    char *libs = calloc(1, sizeof(char));
    size_t libs_len = 0;
    char *source_includes = calloc(1, sizeof(char));
    size_t source_includes_len = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(command, "--help") == 0) {
            usage(argv[0]);
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "-g") == 0)
            flags |= COMP_DEBUG;
        else if (strcmp(argv[i], "-l") == 0) {
            if (i == argc - 1) {
                log_error(NULL, 0, 0);
                fprintf(stderr, "missing library name\n");
                free(libs);
                free(source_includes);
                return EXIT_FAILURE;
            }

            const size_t len = strlen(argv[++i]);
            libs = realloc(libs, libs_len + len + 5);

            if (libs_len > 0) {
                strcat(libs, " ");
                libs_len++;
            }

            strcat(libs, "-l");
            strcat(libs, argv[i]);
            libs_len += len + 2;
        } else if (strcmp(argv[i], "-include") == 0) {
            if (i == argc - 1) {
                log_error(NULL, 0, 0);
                fprintf(stderr, "missing include header\n");
                free(libs);
                free(source_includes);
                return EXIT_FAILURE;
            }

            const size_t len = strlen(argv[++i]);
            source_includes = realloc(source_includes, source_includes_len + len + 17);

            strcat(source_includes, "#include <");
            strcat(source_includes, argv[i]);
            strcat(source_includes, ".h>\n");
            source_includes_len += len + 15;
        } else if (strcmp(argv[i], "-no-main") == 0)
            flags |= COMP_NO_MAIN;
        else if (strcmp(argv[i], "-o") == 0) {
            if (i == argc - 1) {
                log_error(NULL, 0, 0);
                fprintf(stderr, "missing output filename\n");
                free(libs);
                free(source_includes);
                return EXIT_FAILURE;
            }

            outfile = argv[++i];
            flags |= COMP_OUTFILE_SPECIFIED;
        } else if (i == argc - 1)
            infile = argv[i];
        else {
            log_error(NULL, 0, 0);
            fprintf(stderr, "unknown option '%s'\n", argv[i]);
            free(libs);
            free(source_includes);
            return EXIT_FAILURE;
        }
    }

    if (infile == NULL) {
        log_error(NULL, 0, 0);
        fprintf(stderr, "missing input file\n");
        free(libs);
        free(source_includes);
        return EXIT_FAILURE;
    }
    
    int status = compile(infile, outfile, flags, libs, source_includes);
    free(libs);
    free(source_includes);
    return status;
}
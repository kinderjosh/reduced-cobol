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
           "    -o <output file>    specify the output filename\n", prog);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char *command = argv[1];
    unsigned int flags = 0;

    if (strcmp(command, "source") == 0)
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

    char *infile = NULL;
    char *outfile = "a.out";

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i == argc - 1) {
                log_error(NULL, 0, 0);
                fprintf(stderr, "missing output filename\n");
                return EXIT_FAILURE;
            }

            outfile = argv[++i];
            flags |= COMP_OUTFILE_SPECIFIED;
        } else if (i == argc - 1)
            infile = argv[i];
        else {
            log_error(NULL, 0, 0);
            fprintf(stderr, "unknown option '%s'\n", argv[i]);
            return EXIT_FAILURE;
        }
    }

    if (infile == NULL) {
        log_error(NULL, 0, 0);
        fprintf(stderr, "missing input file\n");
        return EXIT_FAILURE;
    }
    
    return compile(infile, outfile, flags);
}
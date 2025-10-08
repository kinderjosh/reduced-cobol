#ifndef COMPILE_H
#define COMPILE_H

#define COMP_SOURCE_ONLY (0x01)
#define COMP_RUN (0x02)
#define COMP_OUTFILE_SPECIFIED (0x04)
#define COMP_OBJECT (0x08)
#define COMP_NO_MAIN (0x10)
#define COMP_DEBUG (0x20)

#include <stdio.h>

int compile(char **infiles, size_t infile_count, char *outfile, unsigned int flags, char *libs, char *source_includes);

#endif
#ifndef COMPILE_H
#define COMPILE_H

#define COMP_SOURCE_ONLY (0x01)
#define COMP_RUN (0x02)
#define COMP_OUTFILE_SPECIFIED (0x04)
#define COMP_OBJECT (0x08)

int compile(char *infile, char *outfile, unsigned int flags);

#endif
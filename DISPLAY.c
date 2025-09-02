#define _RED_COBOL_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
static char string_builder[4097];
static size_t string_builder_pointer;
static char *read_buffer;
static char file_status[3];
FILE *last_opened_outfile;
unsigned int _MY_NUMBER = 50;
char _MY_CHARACTER = 88;
char _MY_STRING[33] = "Hello!";
char _MY_MIXED_STRING[33] = "ABC123";
unsigned int _HALF;
int main(void) {
printf("%c", "Hello, World!");
fputc('\n', stdout);
printf("%.2u", _MY_NUMBER);
fputc('\n', stdout);
printf("%c", _MY_CHARACTER);
fputc('\n', stdout);
printf("%.32s", _MY_STRING);
fputc('\n', stdout);
printf("%.32s", _MY_MIXED_STRING);
fputc('\n', stdout);
_HALF = _MY_NUMBER / 2;
printf("%c", "Half of ");
printf("%.2u", _MY_NUMBER);
printf("%c", " is ");
printf("%.1u", _HALF);
printf("%c", "\n");
return 0;
return 0;
}

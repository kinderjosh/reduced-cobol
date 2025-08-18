#include "error.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <assert.h>

#define ERROR_CAP 11

static size_t errors = 0;

void log_error(char *file, size_t ln, size_t col) {
    errors++;

    if (errors == ERROR_CAP) {
        fprintf(stderr, ESC_BOLD "cobc: " ESC_RED "error: " ESC_NORMAL ESC_BOLD "maximum of %d errors logged, aborting\n" ESC_NORMAL, (int)errors - 1);
        exit(EXIT_FAILURE);
    }

    if (file == NULL)
        fprintf(stderr, ESC_BOLD "cobc: " ESC_RED "error: " ESC_NORMAL);
    else if (ln != 0 && col != 0)
        fprintf(stderr, ESC_BOLD "%s:%zu:%zu: " ESC_RED "error: " ESC_NORMAL, file, ln, col);
    else
        fprintf(stderr, ESC_BOLD "%s: " ESC_RED "error: " ESC_NORMAL, file);
}

char *get_error_line(char *file, size_t ln) {
    FILE *f = fopen(file, "r");
    assert(f != NULL);

    char line[1024];
    char *lines = NULL;
    size_t cur_ln = 0;

    while (fgets(line, 1023, f) != NULL && cur_ln < ln) {
        if (++cur_ln == ln) {
            lines = mystrdup(line);
            break;
        }
    }

    fclose(f);

    // Couldn't find the line, just abort.
    if (lines == NULL)
        return NULL;

    if (lines[strlen(lines) - 1] == '\n')
        lines[strlen(lines) - 1] = '\0';

    return lines;
}

void show_error(char *file, size_t ln, size_t col) {
    char *full_line = get_error_line(file, ln);

    if (full_line == NULL)
        return;

    char *ptr = full_line;

    while (*ptr && isspace(*ptr)) {
        ptr++;
        col--;
    }

    assert(*ptr != '\0');

    char *line = mystrdup(ptr);
    free(full_line);

    char gutter[32];
    sprintf(gutter, " %zu |", ln);
    size_t gutter_size = strlen(gutter);

    fputs(ESC_BOLD ESC_CYAN, stderr);

    for (size_t i = 1; i < gutter_size; i++)
        fputc(' ', stderr);

    fputc('|', stderr);

    if (col - 1 >= strlen(line)) {
        fprintf(stderr, "\n%s    %s\n" ESC_NORMAL, gutter, line);
        free(line);
        return;
    }

    fprintf(stderr, "\n%s    ", gutter);

    char c;
    char last;
    bool done = false;
    bool in_pattern = false;
    char pattern_end = 0;
    size_t highlighted_char_count = 1;

    for (size_t i = 0; i < strlen(line); i++) {
        c = line[i];

        if (i + 1 == col || in_pattern) {
            fprintf(stderr, ESC_RED "%c" ESC_CYAN, c);

            if (c == '"' || (c == pattern_end && i > col)) {
                if (pattern_end == 0) {
                    pattern_end = c;
                    done = true;
                }// else
                    //highlighted_char_count++;

                in_pattern = !in_pattern;
            }// else
                //highlighted_char_count++;

            continue;
        } else if (done || i == 0 || isspace(c) || i < col - 1) {
            fputc(c, stderr);

            if (i > col - 1)
                done = true;

            continue;
        }

        last = line[i - 1];
        bool valid = false;

        // Digits/identifiers.
        if (((c == '_' || c == '-') && isalnum(last)) || (isalnum(last) && isalnum(c)) ||
                (isalnum(c) && (last == '-' || last == '_')))
            valid = true;
        else if ((c == '.' && isdigit(last)) || (last == '.' && isdigit(c)))
            valid = true;
        else {
            switch (c) {
                case '=':
                    if (last == '=' || last == '!' || last == '<' || last == '>')
                        valid = true;
                    break;
                case '<':
                case '>':
                    if (last == c)
                        valid = true;
                    break;
                default:
                    done = true;
                    break;
            }
        }

        if (valid) {
            fprintf(stderr, ESC_RED "%c" ESC_CYAN, c);
            highlighted_char_count++;
        } else
            fputc(c, stderr);
    }

    free(line);
    fputc('\n', stderr);

    for (size_t i = 1; i < gutter_size; i++)
        fputc(' ', stderr);

    fputs(ESC_CYAN "|\n" ESC_NORMAL, stderr);
}

size_t error_count() {
    return errors;
}
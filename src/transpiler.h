#ifndef TRANSPILER_H
#define TRANSPILER_H

#include "ast.h"
#include <stdbool.h>

char *emit_root(AST *root, bool require_main, char *source_includes);

#endif
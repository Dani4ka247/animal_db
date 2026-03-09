#ifndef TOKENS_H
#define TOKENS_H

#include <stddef.h>

typedef struct {
    char **argv;
    size_t argc;
} Tokens;

int tokens_parse(const char *line, Tokens *out);
void tokens_free(Tokens *t);

#endif

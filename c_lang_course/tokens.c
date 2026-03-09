#include "tokens.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int push(char ***v, size_t *n, size_t *cap, char *s) {
    if (*n == *cap) {
        size_t ncap = (*cap == 0) ? 8 : (*cap * 2);
        char **nv = realloc(*v, ncap * sizeof(char*));
        if (!nv) return -1;
        *v = nv;
        *cap = ncap;
    }
    (*v)[(*n)++] = s;
    return 0;
}

void tokens_free(Tokens *t) {
    if (!t) return;
    for (size_t i = 0; i < t->argc; i++) free(t->argv[i]);
    free(t->argv);
    t->argv = NULL;
    t->argc = 0;
}

int tokens_parse(const char *line, Tokens *out) {
    out->argv = NULL;
    out->argc = 0;

    size_t cap = 0;
    size_t i = 0;
    size_t n = strlen(line);

    while (1) {
        while (i < n && isspace((unsigned char)line[i])) i++;
        if (i >= n) break;

        char *buf = malloc(n - i + 1);
        if (!buf) { tokens_free(out); return -1; }
        size_t k = 0;

        if (line[i] == '"') {
            i++;
            int closed = 0;
            while (i < n) {
                unsigned char c = (unsigned char)line[i++];
                if (c == '"') { closed = 1; break; }
                if (c == '\\' && i < n) {
                    unsigned char e = (unsigned char)line[i++];
                    if (e == '"' || e == '\\') buf[k++] = (char)e;
                    else { buf[k++] = '\\'; buf[k++] = (char)e; }
                } else {
                    buf[k++] = (char)c;
                }
            }
            if (!closed) { free(buf); tokens_free(out); return -1; }
        } else {
            while (i < n && !isspace((unsigned char)line[i])) {
                unsigned char c = (unsigned char)line[i++];
                if (c == '\\' && i < n) {
                    unsigned char e = (unsigned char)line[i++];
                    if (e == '"' || e == '\\') buf[k++] = (char)e;
                    else { buf[k++] = '\\'; buf[k++] = (char)e; }
                } else {
                    buf[k++] = (char)c;
                }
            }
        }

        buf[k] = '\0';

        char *tok = realloc(buf, k + 1);
        if (!tok) { free(buf); tokens_free(out); return -1; }

        if (push(&out->argv, &out->argc, &cap, tok) < 0) {
            free(tok);
            tokens_free(out);
            return -1;
        }
    }

    return 0;
}

#ifndef ANIMAL_H
#define ANIMAL_H

#include <time.h>
#include <stddef.h>

#define MAX_NAME    64
#define MAX_SPECIES 32
#define MAX_BREED   64

typedef struct {
    unsigned int id;
    char name[MAX_NAME];
    char species[MAX_SPECIES];
    char breed[MAX_BREED];
    int age;
    char gender;
    struct tm admission;
} Animal;

int parse_date_ddmmyyyy(const char *s, struct tm *out);
int format_date_ddmmyyyy(const struct tm *t, char *buf, size_t bufsize);

#endif

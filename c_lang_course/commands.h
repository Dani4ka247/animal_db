#ifndef COMMANDS_H
#define COMMANDS_H

#include "db.h"
#include <stddef.h>

typedef struct {
    Database db;
} App;


int app_init(App *app, const char *filename);
void app_free(App *app);

typedef int (*CmdFn)(App *app, size_t argc, char **argv);

typedef struct {
    const char *name;
    CmdFn fn;
    const char *usage;
    const char *descr;
} Command;

int commands_execute(App *app, size_t argc, char **argv);

#endif

#ifndef COMMANDS_H
#define COMMANDS_H
#define HISTORY_FILE ".history"
#define HISTORY_MAX  1000
#define HISTORY_LINE 512

#include "db.h"
#include <stddef.h>

typedef struct {
    char **items;
    size_t count;
    size_t cap;
    char filename[256];
} History;

typedef enum {
    UNDO_NONE = 0,
    UNDO_ADD,
    UNDO_EDIT,
    UNDO_DEL
} UndoType;

typedef struct {
    UndoType type;
    Animal before;
    Animal after;
    size_t pos;
    int valid;
} UndoState;

typedef struct {
    Database db;
    UndoState undo;
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

#ifndef DB_H
#define DB_H

#include "animal.h"
#include <stddef.h>

typedef struct {
    Animal *items;
    size_t count;
    size_t cap;
    unsigned int next_id;

    char filename[256];
    int dirty;
} Database;

int db_init(Database *db, const char *filename);
void db_free(Database *db);
void db_clear(Database *db);

int db_add(Database *db, const Animal *a);
void db_list(const Database *db, size_t limit);

int db_save_csv(const Database *db, const char *filename);
int db_load_csv(Database *db, const char *filename);

Animal* db_find_by_id(Database *db, unsigned int id);
int db_delete_by_id(Database *db, unsigned int id);


#endif

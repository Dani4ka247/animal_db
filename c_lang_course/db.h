#ifndef DB_H
#define DB_H

#include "animal.h"
#include <stddef.h>

typedef struct {
    unsigned int key;
    size_t value;
    int state; /* 0=empty, 1=used */
} IdIndexEntry;

typedef struct {
    IdIndexEntry *entries;
    size_t cap;
    size_t count;
} IdIndex;

typedef struct {
    Animal *items;
    size_t count;
    size_t cap;
    unsigned int next_id;
    int dirty;
    char filename[256];

    IdIndex id_index;
} Database;

int db_init(Database *db, const char *filename);
void db_free(Database *db);
void db_clear(Database *db);

int db_add(Database *db, const Animal *a);
void db_list(const Database *db, size_t limit);

int db_save_csv(const Database *db, const char *filename);
int db_load_csv(Database *db, const char *filename);

int db_save_bin(const Database *db, const char *filename);
int db_load_bin(Database *db, const char *filename);

Animal* db_find_by_id(Database *db, unsigned int id);
int db_delete_by_id(Database *db, unsigned int id);

int db_rebuild_id_index(Database *db);
int db_id_index_slot(const Database *db, unsigned int id, size_t *out_slot);

int db_insert_raw(Database *db, const Animal *a, size_t pos);

#endif

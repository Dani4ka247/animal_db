#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static int grow(Database *db) {
    size_t ncap = db->cap ? db->cap * 2 : 16;
    Animal *p = realloc(db->items, ncap * sizeof(Animal));
    if (!p) return -1;
    db->items = p;
    db->cap = ncap;
    return 0;
}

int db_init(Database *db, const char *filename) {
    memset(db, 0, sizeof(*db));
    db->next_id = 1;
    if (filename && *filename) {
        strncpy(db->filename, filename, sizeof(db->filename) - 1);
        db->filename[sizeof(db->filename) - 1] = '\0';
    } else {
        strcpy(db->filename, "shelter.csv");
    }
    return 0;
}

void db_free(Database *db) {
    free(db->items);
    db->items = NULL;
    db->count = db->cap = 0;
}

void db_clear(Database *db) {
    db->count = 0;
    db->next_id = 1;
    db->dirty = 0;
}

int db_add(Database *db, const Animal *a) {
    if (db->count == db->cap && grow(db) < 0) return -1;
    Animal x = *a;
    x.id = db->next_id++;
    db->items[db->count++] = x;
    db->dirty = 1;
    return 0;
}

void db_list(const Database *db, size_t limit) {
    if (limit == 0 || limit > db->count) limit = db->count;

    printf("%-4s %-10s %-10s %-10s %4s %-2s %-10s\n",
           "ID", "NAME", "SPECIES", "BREED", "AGE", "G", "DATE");

    for (size_t i = 0; i < limit; i++) {
        char datebuf[32] = {0};
        format_date_ddmmyyyy(&db->items[i].admission, datebuf, sizeof datebuf);

        printf("%-4u %-10.10s %-10.10s %-10.10s %4d %-2c %-10s\n",
               db->items[i].id,
               db->items[i].name,
               db->items[i].species,
               db->items[i].breed,
               db->items[i].age,
               db->items[i].gender,
               datebuf);
    }
}


int db_save_csv(const Database *db, const char *filename) {
    if (!filename || filename[0] == '\0') {
        errno = EINVAL;
        return -1;
    }

    FILE *f = fopen(filename, "w");
    if (!f) return -1;

    fprintf(f, "id,name,species,breed,age,gender,admission_date\n");
    for (size_t i = 0; i < db->count; i++) {
        char datebuf[32];
        if (format_date_ddmmyyyy(&db->items[i].admission, datebuf, sizeof datebuf) < 0)
            strcpy(datebuf, "01.01.1970");
        fprintf(f, "%u,%s,%s,%s,%d,%c,%s\n",
                db->items[i].id,
                db->items[i].name,
                db->items[i].species,
                db->items[i].breed,
                db->items[i].age,
                db->items[i].gender,
                datebuf);
    }

    if (fclose(f) != 0) return -1;   
    return 0;
}

int db_load_csv(Database *db, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return -1;

    db_clear(db);

    char line[512];
    if (!fgets(line, sizeof line, f)) { fclose(f); return 0; }

    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\n")] = 0;

        char *id_s = strtok(line, ",");
        char *name = strtok(NULL, ",");
        char *species = strtok(NULL, ",");
        char *breed = strtok(NULL, ",");
        char *age_s = strtok(NULL, ",");
        char *gender_s = strtok(NULL, ",");
        char *date_s = strtok(NULL, ",");

        if (!id_s || !name || !species || !breed || !age_s || !gender_s || !date_s) continue;

        Animal a;
        memset(&a, 0, sizeof a);

        a.id = (unsigned int)strtoul(id_s, NULL, 10);
        strncpy(a.name, name, MAX_NAME - 1);
        strncpy(a.species, species, MAX_SPECIES - 1);
        strncpy(a.breed, breed, MAX_BREED - 1);
        a.age = (int)strtol(age_s, NULL, 10);
        a.gender = gender_s[0];

        if (parse_date_ddmmyyyy(date_s, &a.admission) < 0) continue;

        if (db->count == db->cap && grow(db) < 0) { fclose(f); return -1; }
        db->items[db->count++] = a;
        if (a.id >= db->next_id) db->next_id = a.id + 1;
    }

    fclose(f);
    db->dirty = 0;
    return 0;
}

Animal* db_find_by_id(Database *db, unsigned int id) {
    for (size_t i = 0; i < db->count; i++) {
        if (db->items[i].id == id) return &db->items[i];
    }
    return NULL;
}

int db_delete_by_id(Database *db, unsigned int id) {
    for (size_t i = 0; i < db->count; i++) {
        if (db->items[i].id == id) {
            db->items[i] = db->items[db->count - 1];             db->count--;
            db->dirty = 1;
            return 0;
        }
    }
    return -1;
}


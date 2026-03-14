#include "db.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DB_BIN_MAGIC "ANDB"
#define DB_BIN_VERSION 1u

static void id_index_free(IdIndex *idx);
static int id_index_put(IdIndex *idx, unsigned int key, size_t value);

static int grow(Database *db) {
    size_t ncap = db->cap ? db->cap * 2 : 16;
    Animal *p = realloc(db->items, ncap * sizeof(Animal));
    if (!p)
        return -1;
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

    return db_rebuild_id_index(db);
}

void db_free(Database *db) {
    free(db->items);
    db->items = NULL;
    db->count = db->cap = 0;
    id_index_free(&db->id_index);
}

void db_clear(Database *db) {
    db->count = 0;
    db->next_id = 1;
    db->dirty = 0;
    id_index_free(&db->id_index);
    db_rebuild_id_index(db);
}


static int write_exact(const void *ptr, size_t size, size_t n, FILE *f) {
    return fwrite(ptr, size, n, f) == n ? 0 : -1;
}

static int read_exact(void *ptr, size_t size, size_t n, FILE *f) {
    return fread(ptr, size, n, f) == n ? 0 : -1;
}

int db_add(Database *db, const Animal *a) {
    if (db->count == db->cap && grow(db) < 0) return -1;

    Animal x = *a;
    x.id = db->next_id++;
    db->items[db->count++] = x;
    db->dirty = 1;

    if (db_rebuild_id_index(db) < 0) return -1;
    return 0;
}

void db_list(const Database *db, size_t limit) {
    if (limit == 0 || limit > db->count)
        limit = db->count;

    printf("%-4s %-10s %-10s %-10s %4s %-2s %-10s\n", "ID", "NAME", "SPECIES",
           "BREED", "AGE", "G", "DATE");

    for (size_t i = 0; i < limit; i++) {
        char datebuf[32] = {0};
        format_date_ddmmyyyy(&db->items[i].admission, datebuf, sizeof datebuf);
        size_t slot = 0;
        char hidx_buf[32];

        if (db_id_index_slot(db, db->items[i].id, &slot) == 0)
            snprintf(hidx_buf, sizeof hidx_buf, "%zu", slot);
        else
            snprintf(hidx_buf, sizeof hidx_buf, "-");

        printf("%-4u %-10.10s %-10.10s %-10.10s %4d %-2c %-10s <%5s>\n",
               db->items[i].id,
               db->items[i].name,
               db->items[i].species,
               db->items[i].breed,
               db->items[i].age,
               db->items[i].gender,
               datebuf,
               hidx_buf);
    }
}

static void id_index_free(IdIndex *idx) {
    free(idx->entries);
    idx->entries = NULL;
    idx->cap = 0;
    idx->count = 0;
}

static size_t hash_u32(unsigned int x) { return (size_t)(x * 2654435761u); }

static int id_index_init(IdIndex *idx, size_t want_cap) {
    size_t cap = 16;
    while (cap < want_cap)
        cap <<= 1;

    idx->entries = calloc(cap, sizeof(IdIndexEntry));
    if (!idx->entries)
        return -1;

    idx->cap = cap;
    idx->count = 0;
    return 0;
}

static int id_index_put(IdIndex *idx, unsigned int key, size_t value) {
    if (!idx->entries || idx->cap == 0)
        return -1;

    size_t mask = idx->cap - 1;
    size_t pos = hash_u32(key) & mask;

    while (idx->entries[pos].state == 1) {
        if (idx->entries[pos].key == key) {
            idx->entries[pos].value = value;
            return 0;
        }
        pos = (pos + 1) & mask;
    }

    idx->entries[pos].state = 1;
    idx->entries[pos].key = key;
    idx->entries[pos].value = value;
    idx->count++;
    return 0;
}

static int id_index_find_slot(const IdIndex *idx, unsigned int key, size_t *out_pos) {
    if (!idx || !out_pos) return -1;
    if (!idx->entries || idx->cap == 0) return -1;

    size_t mask = idx->cap - 1;
    size_t pos = hash_u32(key) & mask;
    size_t start = pos;

    while (idx->entries[pos].state != 0) {
        if (idx->entries[pos].state == 1 && idx->entries[pos].key == key) {
            *out_pos = pos;
            return 0;
        }
        pos = (pos + 1) & mask;
        if (pos == start) break;
    }

    return -1;
}

static int id_index_get(const IdIndex *idx, unsigned int key, size_t *out_value) {
    size_t pos;
    if (!out_value) return -1;
    if (id_index_find_slot(idx, key, &pos) < 0) return -1;
    *out_value = idx->entries[pos].value;
    return 0;
}

int db_id_index_slot(const Database *db, unsigned int id, size_t *out_slot) {
    if (!db || !out_slot) return -1;
    return id_index_find_slot(&db->id_index, id, out_slot);
}


int db_rebuild_id_index(Database *db) {
    IdIndex new_idx = {0};

    if (id_index_init(&new_idx, db->count * 2 + 16) < 0)
        return -1;

    for (size_t i = 0; i < db->count; i++) {
        if (id_index_put(&new_idx, db->items[i].id, i) < 0) {
            id_index_free(&new_idx);
            return -1;
        }
    }

    id_index_free(&db->id_index);
    db->id_index = new_idx;
    return 0;
}

int db_save_bin(const Database *db, const char *filename) {
    if (!filename || filename[0] == '\0') {
        errno = EINVAL;
        return -1;
    }

    FILE *f = fopen(filename, "wb");
    if (!f)
        return -1;

    char magic[4] = DB_BIN_MAGIC;
    uint32_t version = DB_BIN_VERSION;
    uint32_t count = (uint32_t)db->count;
    uint32_t next_id = db->next_id;

    if (write_exact(magic, 1, 4, f) < 0 ||
        write_exact(&version, sizeof(version), 1, f) < 0 ||
        write_exact(&count, sizeof(count), 1, f) < 0 ||
        write_exact(&next_id, sizeof(next_id), 1, f) < 0) {
        fclose(f);
        return -1;
    }

    for (size_t i = 0; i < db->count; i++) {
        const Animal *a = &db->items[i];

        uint32_t id = a->id;
        int32_t age = a->age;
        char gender = a->gender;
        int32_t year = a->admission.tm_year + 1900;
        int32_t month = a->admission.tm_mon + 1;
        int32_t day = a->admission.tm_mday;

        if (write_exact(&id, sizeof(id), 1, f) < 0 ||
            write_exact(a->name, sizeof(a->name), 1, f) < 0 ||
            write_exact(a->species, sizeof(a->species), 1, f) < 0 ||
            write_exact(a->breed, sizeof(a->breed), 1, f) < 0 ||
            write_exact(&age, sizeof(age), 1, f) < 0 ||
            write_exact(&gender, sizeof(gender), 1, f) < 0 ||
            write_exact(&year, sizeof(year), 1, f) < 0 ||
            write_exact(&month, sizeof(month), 1, f) < 0 ||
            write_exact(&day, sizeof(day), 1, f) < 0) {
            fclose(f);
            return -1;
        }
    }

    if (fclose(f) != 0)
        return -1;
    return 0;
}

int db_load_bin(Database *db, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f)
        return -1;

    char magic[4];
    uint32_t version = 0;
    uint32_t count = 0;
    uint32_t next_id = 1;

    if (read_exact(magic, 1, 4, f) < 0 ||
        read_exact(&version, sizeof(version), 1, f) < 0 ||
        read_exact(&count, sizeof(count), 1, f) < 0 ||
        read_exact(&next_id, sizeof(next_id), 1, f) < 0) {
        fclose(f);
        return -1;
    }

    if (memcmp(magic, DB_BIN_MAGIC, 4) != 0 || version != DB_BIN_VERSION) {
        fclose(f);
        errno = EINVAL;
        return -1;
    }

    db_clear(db);

    for (uint32_t i = 0; i < count; i++) {
        Animal a;
        memset(&a, 0, sizeof a);

        uint32_t id = 0;
        int32_t age = 0;
        int32_t year = 0, month = 0, day = 0;
        char gender = 0;

        if (read_exact(&id, sizeof(id), 1, f) < 0 ||
            read_exact(a.name, sizeof(a.name), 1, f) < 0 ||
            read_exact(a.species, sizeof(a.species), 1, f) < 0 ||
            read_exact(a.breed, sizeof(a.breed), 1, f) < 0 ||
            read_exact(&age, sizeof(age), 1, f) < 0 ||
            read_exact(&gender, sizeof(gender), 1, f) < 0 ||
            read_exact(&year, sizeof(year), 1, f) < 0 ||
            read_exact(&month, sizeof(month), 1, f) < 0 ||
            read_exact(&day, sizeof(day), 1, f) < 0) {
            fclose(f);
            return -1;
        }

        a.id = id;
        a.age = age;
        a.gender = gender;
        a.admission.tm_year = year - 1900;
        a.admission.tm_mon = month - 1;
        a.admission.tm_mday = day;
        a.name[MAX_NAME - 1] = '\0';
        a.species[MAX_SPECIES - 1] = '\0';
        a.breed[MAX_BREED - 1] = '\0';

        if (db->count == db->cap && grow(db) < 0) {
            fclose(f);
            return -1;
        }

        db->items[db->count++] = a;
    }

    fclose(f);
    db->next_id = next_id;
    if (db->next_id == 0)
        db->next_id = 1;
    db->dirty = 0;
    if (db_rebuild_id_index(db) < 0) {
        return -1;
    }
    return 0;
}

int db_save_csv(const Database *db, const char *filename) {
    if (!filename || filename[0] == '\0') {
        errno = EINVAL;
        return -1;
    }

    FILE *f = fopen(filename, "w");
    if (!f)
        return -1;

    fprintf(f, "id,name,species,breed,age,gender,admission_date\n");
    for (size_t i = 0; i < db->count; i++) {
        char datebuf[32];
        if (format_date_ddmmyyyy(&db->items[i].admission, datebuf,
                                 sizeof datebuf) < 0)
            strcpy(datebuf, "01.01.1970");
        fprintf(f, "%u,%s,%s,%s,%d,%c,%s\n", db->items[i].id, db->items[i].name,
                db->items[i].species, db->items[i].breed, db->items[i].age,
                db->items[i].gender, datebuf);
    }

    if (fclose(f) != 0)
        return -1;
    
    return 0;
}

int db_load_csv(Database *db, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f)
        return -1;

    db_clear(db);

    char line[512];
    if (!fgets(line, sizeof line, f)) {
        fclose(f);
        return 0;
    }

    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\n")] = 0;

        char *id_s = strtok(line, ",");
        char *name = strtok(NULL, ",");
        char *species = strtok(NULL, ",");
        char *breed = strtok(NULL, ",");
        char *age_s = strtok(NULL, ",");
        char *gender_s = strtok(NULL, ",");
        char *date_s = strtok(NULL, ",");

        if (!id_s || !name || !species || !breed || !age_s || !gender_s ||
            !date_s)
            continue;

        Animal a;
        memset(&a, 0, sizeof a);

        a.id = (unsigned int)strtoul(id_s, NULL, 10);
        strncpy(a.name, name, MAX_NAME - 1);
        strncpy(a.species, species, MAX_SPECIES - 1);
        strncpy(a.breed, breed, MAX_BREED - 1);
        a.age = (int)strtol(age_s, NULL, 10);
        a.gender = gender_s[0];

        if (parse_date_ddmmyyyy(date_s, &a.admission) < 0)
            continue;

        if (db->count == db->cap && grow(db) < 0) {
            fclose(f);
            return -1;
        }
        db->items[db->count++] = a;
        if (a.id >= db->next_id)
            db->next_id = a.id + 1;
    }

    fclose(f);
    db->dirty = 0;
    if (db_rebuild_id_index(db) < 0) {
        return -1;
    }
    return 0;
}

Animal* db_find_by_id(Database *db, unsigned int id) {
    size_t pos;
    if (id_index_get(&db->id_index, id, &pos) == 0) {
        if (pos < db->count && db->items[pos].id == id)
            return &db->items[pos];
    }
    return NULL;
}


int db_delete_by_id(Database *db, unsigned int id) {
    Animal *a = db_find_by_id(db, id);
    if (!a) return -1;

    size_t pos = (size_t)(a - db->items);
    db->items[pos] = db->items[db->count - 1];
    db->count--;
    db->dirty = 1;

    return db_rebuild_id_index(db);
}

int db_insert_raw(Database *db, const Animal *a, size_t pos) {
    if (!db || !a) return -1;

    if (pos > db->count)
        pos = db->count;

    if (db->count == db->cap && grow(db) < 0)
        return -1;

    if (pos < db->count) {
        memmove(&db->items[pos + 1],
                &db->items[pos],
                (db->count - pos) * sizeof(Animal));
    }

    db->items[pos] = *a;
    db->count++;

    if (a->id >= db->next_id)
        db->next_id = a->id + 1;

    db->dirty = 1;

    return db_rebuild_id_index(db);
}

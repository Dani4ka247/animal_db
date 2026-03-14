#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "db.h"

static Animal make_animal(const char *name, const char *species,
                          const char *breed, int age, char gender,
                          const char *date)
{
    Animal a;
    memset(&a, 0, sizeof(a));

    strncpy(a.name, name, MAX_NAME - 1);
    strncpy(a.species, species, MAX_SPECIES - 1);
    strncpy(a.breed, breed, MAX_BREED - 1);
    a.age = age;
    a.gender = gender;
    assert(parse_date_ddmmyyyy(date, &a.admission) == 0);

    return a;
}

static void test_add_and_find(void) {
    Database db;
    assert(db_init(&db, "test.csv") == 0);

    Animal a = make_animal("bobr", "rodent", "common", 5, 'M', "01.01.2024");
    assert(db_add(&db, &a) == 0);
    assert(db.count == 1);

    Animal *p = db_find_by_id(&db, 1);
    assert(p != NULL);
    assert(strcmp(p->name, "bobr") == 0);
    assert(strcmp(p->species, "rodent") == 0);

    db_free(&db);
}

static void test_delete(void) {
    Database db;
    assert(db_init(&db, "test.csv") == 0);

    Animal a1 = make_animal("a1", "cat", "x", 1, 'M', "01.01.2024");
    Animal a2 = make_animal("a2", "dog", "y", 2, 'F', "02.01.2024");

    assert(db_add(&db, &a1) == 0);
    assert(db_add(&db, &a2) == 0);
    assert(db.count == 2);

    assert(db_delete_by_id(&db, 1) == 0);
    assert(db.count == 1);
    assert(db_find_by_id(&db, 1) == NULL);
    assert(db_find_by_id(&db, 2) != NULL);

    db_free(&db);
}

static void test_insert_raw(void) {
    Database db;
    assert(db_init(&db, "test.csv") == 0);

    Animal a1 = make_animal("a1", "cat", "x", 1, 'M', "01.01.2024");
    Animal a2 = make_animal("a2", "dog", "y", 2, 'F', "02.01.2024");
    Animal a3 = make_animal("a3", "bird", "z", 3, 'M', "03.01.2024");

    assert(db_add(&db, &a1) == 0);
    assert(db_add(&db, &a2) == 0);

    a3.id = 99;
    assert(db_insert_raw(&db, &a3, 1) == 0);

    assert(db.count == 3);
    assert(db.items[1].id == 99);
    assert(strcmp(db.items[1].name, "a3") == 0);
    assert(db_find_by_id(&db, 99) != NULL);

    db_free(&db);
}

static void test_save_load_csv(void) {
    Database db;
    assert(db_init(&db, "test.csv") == 0);

    Animal a1 = make_animal("bobr", "rodent", "common", 5, 'M', "01.01.2024");
    Animal a2 = make_animal("lisa", "fox", "red", 7, 'F', "02.02.2024");

    assert(db_add(&db, &a1) == 0);
    assert(db_add(&db, &a2) == 0);

    assert(db_save_csv(&db, "tmp_test.csv") == 0);
    db_free(&db);

    Database db2;
    assert(db_init(&db2, "tmp_test.csv") == 0);
    assert(db_load_csv(&db2, "tmp_test.csv") == 0);

    assert(db2.count == 2);
    assert(db_find_by_id(&db2, 1) != NULL);
    assert(db_find_by_id(&db2, 2) != NULL);
    assert(strcmp(db2.items[0].name, "bobr") == 0);

    db_free(&db2);
    remove("tmp_test.csv");
}

static void test_save_load_bin(void) {
    Database db;
    assert(db_init(&db, "test.bin") == 0);

    Animal a1 = make_animal("bobr", "rodent", "common", 5, 'M', "01.01.2024");
    Animal a2 = make_animal("lisa", "fox", "red", 7, 'F', "02.02.2024");

    assert(db_add(&db, &a1) == 0);
    assert(db_add(&db, &a2) == 0);

    assert(db_save_bin(&db, "tmp_test.bin") == 0);
    db_free(&db);

    Database db2;
    assert(db_init(&db2, "tmp_test.bin") == 0);
    assert(db_load_bin(&db2, "tmp_test.bin") == 0);

    assert(db2.count == 2);
    assert(db_find_by_id(&db2, 1) != NULL);
    assert(db_find_by_id(&db2, 2) != NULL);
    assert(strcmp(db2.items[1].name, "lisa") == 0);

    db_free(&db2);
    remove("tmp_test.bin");
}

int main(void) {
    test_add_and_find();
    test_delete();
    test_insert_raw();
    test_save_load_csv();
    test_save_load_bin();

    printf("ALL TESTS PASSED\n");
    return 0;
}

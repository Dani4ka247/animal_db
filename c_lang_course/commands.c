#include "commands.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>

int app_init(App *app, const char *filename) { return db_init(&app->db, filename); }
void app_free(App *app) { db_free(&app->db); }

static int cmd_help(App *app, size_t argc, char **argv);
static int cmd_add(App *app, size_t argc, char **argv);
static int cmd_list(App *app, size_t argc, char **argv);
static int cmd_save(App *app, size_t argc, char **argv);
static int cmd_load(App *app, size_t argc, char **argv);
static int cmd_del(App *app, size_t argc, char **argv);
static int cmd_edit(App *app, size_t argc, char **argv);
static int cmd_sort(App *app, size_t argc, char **argv);
static int cmd_find(App *app, size_t argc, char **argv);
static int cmd_stats(App *app, size_t argc, char **argv);



static int has_csv_ext(const char *s) {
    const char *dot = strrchr(s, '.');
    return dot && strcasecmp(dot, ".csv") == 0;
}

static int make_csv_name(const char *in, char *out, size_t outsz) {
    if (!in || !in[0]) return -1;
    if (has_csv_ext(in)) {
        int need = snprintf(out, outsz, "%s", in);
        return (need < 0 || (size_t)need >= outsz) ? -1 : 0;
    }
    int need = snprintf(out, outsz, "%s.csv", in);
    return (need < 0 || (size_t)need >= outsz) ? -1 : 0;
}


static const Command table[] = {
    {"help", cmd_help, "help [command]", "Выводит описание команды"},
    {"add",  cmd_add,  "add <name> <species> <breed> <age> <M/F|Male/Female>", "Добавляет новый объект в базу данных (дата ставится автоматически)"},
    {"show", cmd_list, "show [n]", "Выводит содержимое коллекции (первые n записей)"},
    {"save", cmd_save, "save [файл|файл.csv]", "Сохраняет текущую базу в файл (.csv добавится автоматически)"},
    {"load", cmd_load, "load <файл|файл.csv>", "Загружает базу из файла (.csv добавится автоматически)"},
    {"del",  cmd_del,  "del <id>", "Удаляет запись по ID (с подтверждением)"},
    {"edit", cmd_edit, "edit <id> <field>=<value> ...", "Редактирует поля записи по ID; можно также field = value"},
    {"sort", cmd_sort, "sort <field> [asc|desc] <field> [asc|desc] ...",
     "Сортирует по нескольким полям (по умолчанию asc). Поля: id,name,species,breed,age,gender,date"},
    {"find", cmd_find, "find <field> <value>", "Поиск записей по полю (строки — подстрока без регистра)"},
    {"stats", cmd_stats, "stats", "Показывает статистику по текущей базе"},
    {NULL, NULL, NULL, NULL}
};

static int contains_icase_ascii(const char *hay, const char *needle) {
    if (!hay || !needle) return 0;
    if (*needle == '\0') return 1;

    for (size_t i = 0; hay[i]; i++) {
        size_t j = 0;
        while (hay[i + j] && needle[j]) {
            unsigned char a = (unsigned char)hay[i + j];
            unsigned char b = (unsigned char)needle[j];
            if (tolower(a) != tolower(b)) break;
            j++;
        }
        if (needle[j] == '\0') return 1;
    }
    return 0;
}

static const Command* find_cmd(const char *name) {
    for (size_t i = 0; table[i].name; i++) {
        if (strcasecmp(name, table[i].name) == 0) return &table[i];
    }
    return NULL;
}

static int cmd_find(App *app, size_t argc, char **argv) {
    if (argc < 3) {
        printf("Использование: find <field> <value>\n");
        printf("Поля: id,name,species,breed,age,gender,date\n");
        return 0;
    }

    const char *field = argv[1];
    const char *value = argv[2];

    size_t found = 0;

    for (size_t i = 0; i < app->db.count; i++) {
        Animal *a = &app->db.items[i];
        int ok = 0;

        if (strcasecmp(field, "id") == 0) {
            unsigned int id = (unsigned int)strtoul(value, NULL, 10);
            ok = (a->id == id);
        } else if (strcasecmp(field, "age") == 0) {
            int age = (int)strtol(value, NULL, 10);
            ok = (a->age == age);
        } else if (strcasecmp(field, "gender") == 0) {
            char g = value[0];
            if (g == 'm' || g == 'M') g = 'M';
            if (g == 'f' || g == 'F') g = 'F';
            ok = (a->gender == g);
        } else if (strcasecmp(field, "date") == 0) {
            struct tm t;
            if (parse_date_ddmmyyyy(value, &t) < 0) {
                printf("bad date, expected dd.mm.yyyy\n");
                return 0;
            }
            ok = (a->admission.tm_year == t.tm_year &&
                  a->admission.tm_mon  == t.tm_mon &&
                  a->admission.tm_mday == t.tm_mday);
        } else if (strcasecmp(field, "name") == 0) {
            ok = contains_icase_ascii(a->name, value);
        } else if (strcasecmp(field, "species") == 0) {
            ok = contains_icase_ascii(a->species, value);
        } else if (strcasecmp(field, "breed") == 0) {
            ok = contains_icase_ascii(a->breed, value);
        } else {
            printf("Неизвестное поле: %s\n", field);
            return 0;
        }

        if (ok) {
            if (found == 0) {
                printf("%-4s %-10s %-10s %-10s %4s %-2s %-10s\n",
                       "ID", "NAME", "SPECIES", "BREED", "AGE", "G", "DATE");
            }

            char datebuf[32] = {0};
            format_date_ddmmyyyy(&a->admission, datebuf, sizeof datebuf);

            printf("%-4u %-10.10s %-10.10s %-10.10s %4d %-2c %-10s\n",
                   a->id, a->name, a->species, a->breed, a->age, a->gender, datebuf);

            found++;
        }
    }

    printf("Найдено: %zu\n", found);
    return 0;
}

static int cmd_stats(App *app, size_t argc, char **argv) {
    (void)argc; (void)argv;

    size_t n = app->db.count;
    printf("Записей: %zu\n", n);

    if (n == 0) return 0;

    size_t male = 0, female = 0, other = 0;
    long long sum_age = 0;

    int min_age = app->db.items[0].age;
    int max_age = app->db.items[0].age;

    for (size_t i = 0; i < n; i++) {
        Animal *a = &app->db.items[i];

        if (a->gender == 'M') male++;
        else if (a->gender == 'F') female++;
        else other++;

        if (a->age < min_age) min_age = a->age;
        if (a->age > max_age) max_age = a->age;

        sum_age += a->age;
    }

    double avg_age = (double)sum_age / (double)n;

    printf("Пол: M=%zu, F=%zu", male, female);
    if (other) printf(", other=%zu", other);
    printf("\n");

    printf("Возраст: min=%d, max=%d, avg=%.2f\n", min_age, max_age, avg_age);
    return 0;
}


static int cmd_help(App *app, size_t argc, char **argv) {
    (void)app;

    if (argc >= 2) {
        const Command *c = find_cmd(argv[1]);
        if (!c) {
            printf("Неизвестная команда: %s\n", argv[1]);
            return 0;
        }
        printf("%s\n", c->descr);
        printf("Использование: %s\n", c->usage);
        return 0;
    }

    puts("Команды:");
    for (size_t i = 0; table[i].name; i++) {
        printf("  %-8s - %s\n", table[i].name, table[i].descr);
    }
    puts("Подсказка: help <команда>");
    return 0;
}

static int cmd_list(App *app, size_t argc, char **argv) {
    size_t limit = 0;

    if (argc >= 2) {
        char *end = NULL;
        unsigned long x = strtoul(argv[1], &end, 10);
        if (end == argv[1] || *end != '\0') {
            printf("Ошибка: n должно быть числом\n");
            return 0;
        }
        limit = (size_t)x;
    }

    db_list(&app->db, limit);
    return 0;
}



static int cmd_save(App *app, size_t argc, char **argv) {
    const char *arg = (argc >= 2) ? argv[1] : app->db.filename;
    if (!arg || arg[0] == '\0') {
        printf("Ошибка: не задано имя файла\n");
        return 0;
    }

    char file[256];
    if (make_csv_name(arg, file, sizeof file) < 0) {
        printf("Ошибка: слишком длинное имя файла\n");
        return 0;
    }

    if (argc < 2) printf("Сохраняю в текущий файл: %s\n", file);

    errno = 0;
    if (db_save_csv(&app->db, file) < 0) {
        perror("save");
        return 0;
    }

    if (strcmp(app->db.filename, file) != 0) {
        strncpy(app->db.filename, file, sizeof(app->db.filename)-1);
        app->db.filename[sizeof(app->db.filename)-1] = '\0';
    }

    app->db.dirty = 0;
    printf("saved: %s\n", app->db.filename);
    return 0;
}

static int cmd_load(App *app, size_t argc, char **argv) {
    if (argc < 2) {
        printf("%s\n", find_cmd("load")->usage);
        return 0;
    }

    char file[256];
    if (make_csv_name(argv[1], file, sizeof file) < 0) {
        printf("Ошибка: слишком длинное имя файла\n");
        return 0;
    }

    if (db_load_csv(&app->db, file) < 0) {
        perror("load");
        return 0;
    }

    strncpy(app->db.filename, file, sizeof(app->db.filename)-1);
    app->db.filename[sizeof(app->db.filename)-1] = '\0';
    printf("loaded: %s\n", app->db.filename);
    return 0;
}


static int cmd_add(App *app, size_t argc, char **argv) {
    if (argc != 6) {
        printf("%s\n", find_cmd("add")->usage);
        return 0;
    }

    Animal a;
    memset(&a, 0, sizeof a);

    strncpy(a.name, argv[1], MAX_NAME-1);
    strncpy(a.species, argv[2], MAX_SPECIES-1);
    strncpy(a.breed, argv[3], MAX_BREED-1);

    a.age = (int)strtol(argv[4], NULL, 10);

    const char *gender_s = argv[5];
    if (strcasecmp(gender_s, "M") == 0 || strcasecmp(gender_s, "MALE") == 0 || strcmp(gender_s, "м") == 0)
        a.gender = 'M';
    else if (strcasecmp(gender_s, "F") == 0 || strcasecmp(gender_s, "FEMALE") == 0 || strcmp(gender_s, "ж") == 0)
        a.gender = 'F';
    else { printf("bad gender, use M/F/Male/Female/м/ж\n"); return 0; }

    if (a.age < 0 || a.age > 100) { printf("bad age\n"); return 0; }

    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    if (!lt) { printf("Ошибка времени\n"); return 0; }
    a.admission = *lt;

    if (db_add(&app->db, &a) < 0) printf("add failed (no memory)\n");
    else printf("added id=%u\n", app->db.next_id - 1);
    return 0;
}


static int cmd_del(App *app, size_t argc, char **argv) {
    if (argc < 2) { printf("Использование: del <id>\n"); return 0; }

    unsigned int id = (unsigned int)strtoul(argv[1], NULL, 10);
    Animal *a = db_find_by_id(&app->db, id);
    if (!a) { printf("Не найдено: id=%u\n", id); return 0; }

    printf("Удалить id=%u (%s, %s, %s)? [y/n] ", a->id, a->name, a->species, a->breed);
    char ans[16];
    if (!fgets(ans, sizeof ans, stdin)) return 0;
    if (!(ans[0] == 'y' || ans[0] == 'Y')) { printf("Отменено\n"); return 0; }

    if (db_delete_by_id(&app->db, id) < 0) printf("Ошибка удаления\n");
    else printf("Удалено id=%u\n", id);
    return 0;
}


static int take_assignment(size_t argc, char **argv, size_t *i,
                           const char **field, const char **value,
                           char *tmp, size_t tmpsz)
{
    char *eq = strchr(argv[*i], '=');
    if (eq) {
        *eq = '\0';
        *field = argv[*i];
        *value = eq + 1;
        if (**value == '\0') return -1;
        return 0;
    }

    if (*i + 2 < argc && strcmp(argv[*i + 1], "=") == 0) {
        int need = snprintf(tmp, tmpsz, "%s=%s", argv[*i], argv[*i + 2]);
        if (need < 0 || (size_t)need >= tmpsz) return -1;

        char *eq2 = strchr(tmp, '=');
        *eq2 = '\0';
        *field = tmp;
        *value = eq2 + 1;

        *i += 2;
        return 0;
    }

    return -1;
}

static int cmd_edit(App *app, size_t argc, char **argv) {
    if (argc < 3) {
        printf("Использование: edit <id> <field>=<value> ...\n");
        printf("Пример: edit 1 name=\"Бобёр\" age=5 gender=M date=01.01.2024\n");
        printf("Также можно: edit 1 name = \"Бобёр\"\n");
        return 0;
    }

    unsigned int id = (unsigned int)strtoul(argv[1], NULL, 10);
    Animal *a = db_find_by_id(&app->db, id);
    if (!a) { printf("Не найдено: id=%u\n", id); return 0; }

    int changed = 0;

    for (size_t i = 2; i < argc; i++) {
        const char *field = NULL;
        const char *value = NULL;
        char tmp[256];

        if (take_assignment(argc, argv, &i, &field, &value, tmp, sizeof tmp) < 0) {
            printf("Плохой аргумент: %s\n", argv[i]);
            continue;
        }

        if (strcasecmp(field, "age") == 0) {
            int age = (int)strtol(value, NULL, 10);
            if (age < 0 || age > 100) { printf("bad age\n"); continue; }
            a->age = age;
            changed = 1;
        } else if (strcasecmp(field, "breed") == 0) {
            strncpy(a->breed, value, MAX_BREED-1);
            a->breed[MAX_BREED-1] = '\0';
            changed = 1;
        } else if (strcasecmp(field, "name") == 0) {
            strncpy(a->name, value, MAX_NAME-1);
            a->name[MAX_NAME-1] = '\0';
            changed = 1;
        } else if (strcasecmp(field, "species") == 0) {
            strncpy(a->species, value, MAX_SPECIES-1);
            a->species[MAX_SPECIES-1] = '\0';
            changed = 1;
        } else if (strcasecmp(field, "gender") == 0 || strcasecmp(field, "sex") == 0) {
            if (strcasecmp(value, "M") == 0 || strcasecmp(value, "MALE") == 0 || strcmp(value, "м") == 0)
                a->gender = 'M';
            else if (strcasecmp(value, "F") == 0 || strcasecmp(value, "FEMALE") == 0 || strcmp(value, "ж") == 0)
                a->gender = 'F';
            else { printf("bad gender\n"); continue; }
            changed = 1;
        } else if (strcasecmp(field, "date") == 0 || strcasecmp(field, "admission") == 0) {
            struct tm t;
            if (parse_date_ddmmyyyy(value, &t) < 0) { printf("bad date\n"); continue; }
            a->admission = t;
            changed = 1;
        } else {
            printf("Неизвестное поле: %s\n", field);
        }
    }

    if (changed) {
        app->db.dirty = 1;
        printf("Обновлено id=%u\n", id);
    } else {
        printf("Нечего менять\n");
    }
    return 0;
}


typedef enum { S_ID, S_NAME, S_SPECIES, S_BREED, S_AGE, S_GENDER, S_DATE } SortKey;

typedef struct {
    SortKey key;
    int desc;
} SortSpec;

static SortSpec g_specs[8];
static size_t g_specs_n = 0;

static int cmp_u(unsigned int a, unsigned int b) { return (a > b) - (a < b); }
static int cmp_i(int a, int b) { return (a > b) - (a < b); }

static int cmp_tm_date(const struct tm *a, const struct tm *b) {
    if (a->tm_year != b->tm_year) return (a->tm_year < b->tm_year) ? -1 : 1;
    if (a->tm_mon  != b->tm_mon)  return (a->tm_mon  < b->tm_mon)  ? -1 : 1;
    if (a->tm_mday != b->tm_mday) return (a->tm_mday < b->tm_mday) ? -1 : 1;
    return 0;
}

static int cmp_by_key(const Animal *a, const Animal *b, SortKey key) {
    switch (key) {
        case S_ID:      return cmp_u(a->id, b->id);
        case S_NAME:    return strcasecmp(a->name, b->name);
        case S_SPECIES: return strcasecmp(a->species, b->species);
        case S_BREED:   return strcasecmp(a->breed, b->breed);
        case S_AGE:     return cmp_i(a->age, b->age);
        case S_GENDER:  return (a->gender > b->gender) - (a->gender < b->gender);
        case S_DATE:    return cmp_tm_date(&a->admission, &b->admission);
    }
    return 0;
}

static int animal_cmp_multi(const void *pa, const void *pb) {
    const Animal *a = (const Animal*)pa;
    const Animal *b = (const Animal*)pb;

    for (size_t i = 0; i < g_specs_n; i++) {
        int r = cmp_by_key(a, b, g_specs[i].key);
        if (r != 0) return g_specs[i].desc ? -r : r;
    }
    return 0;
}

static int parse_sort_key(const char *s, SortKey *out) {
    if (strcasecmp(s, "id") == 0) *out = S_ID;
    else if (strcasecmp(s, "name") == 0) *out = S_NAME;
    else if (strcasecmp(s, "species") == 0) *out = S_SPECIES;
    else if (strcasecmp(s, "breed") == 0) *out = S_BREED;
    else if (strcasecmp(s, "age") == 0) *out = S_AGE;
    else if (strcasecmp(s, "gender") == 0) *out = S_GENDER;
    else if (strcasecmp(s, "date") == 0) *out = S_DATE;
    else return -1;
    return 0;
}

static int is_dir_token(const char *s) {
    return strcasecmp(s, "asc") == 0 || strcasecmp(s, "desc") == 0;
}

static const char* sortkey_name(SortKey k) {
    switch (k) {
        case S_ID: return "id";
        case S_NAME: return "name";
        case S_SPECIES: return "species";
        case S_BREED: return "breed";
        case S_AGE: return "age";
        case S_GENDER: return "gender";
        case S_DATE: return "date";
    }
    return "?";
}

static int cmd_sort(App *app, size_t argc, char **argv) {
    if (argc < 2) {
        printf("%s\n", find_cmd("sort")->usage);
        return 0;
    }

    g_specs_n = 0;

    for (size_t i = 1; i < argc; i++) {
        if (g_specs_n >= 8) {
            printf("Слишком много ключей сортировки (макс 8)\n");
            return 0;
        }

        SortKey key;
        if (parse_sort_key(argv[i], &key) < 0) {
            printf("Неизвестное поле сортировки: %s\n", argv[i]);
            return 0;
        }

        int desc = 0;
        if (i + 1 < argc && is_dir_token(argv[i + 1])) {
            desc = (strcasecmp(argv[i + 1], "desc") == 0);
            i++;
        }

        g_specs[g_specs_n++] = (SortSpec){ .key = key, .desc = desc };
    }

    qsort(app->db.items, app->db.count, sizeof(Animal), animal_cmp_multi);
    app->db.dirty = 1;

    printf("Отсортировано по: ");
    for (size_t i = 0; i < g_specs_n; i++) {
        printf("%s %s%s",
               sortkey_name(g_specs[i].key),
               g_specs[i].desc ? "desc" : "asc",
               (i + 1 < g_specs_n) ? ", " : "\n");
    }

    return 0;
}

int commands_execute(App *app, size_t argc, char **argv) {
    if (argc == 0) return 0;

    const Command *c = find_cmd(argv[0]);
    if (!c) {
        printf("unknown command: %s\n", argv[0]);
        return 0;
    }

    return c->fn(app, argc, argv);
}

#include "cli.h"
#include "commands.h"
#include "tokens.h"
#include <stdio.h>
#include <strings.h>

typedef struct {
    App app;
    Cli *cli;
} Ctx;

static int save_current(Ctx *ctx) {
    if (db_save_csv(&ctx->app.db, ctx->app.db.filename) < 0) {
        perror("save");
        return -1;
    }
    ctx->app.db.dirty = 0;
    printf("saved: %s\n", ctx->app.db.filename);
    return 0;
}

static int on_line(void *p, const char *line) {
    Ctx *ctx = (Ctx*)p;

    Tokens t;
    if (tokens_parse(line, &t) < 0) {
        printf("Ошибка парсинга (проверь кавычки)\n");
        return 0;
    }
    if (t.argc == 0) { tokens_free(&t); return 0; }

    if (strcasecmp(t.argv[0], "exit") == 0 || strcasecmp(t.argv[0], "quit") == 0) {
        if (ctx->app.db.dirty) {
            int yes = cli_confirm(ctx->cli, "Есть несохранённые изменения. Сохранить?");
            if (yes) {
                if (save_current(ctx) < 0) { tokens_free(&t); return 0; }
            }
        }
        tokens_free(&t);
        return 1;
    }

    if (strcasecmp(t.argv[0], "load") == 0 && ctx->app.db.dirty) {
        int yes = cli_confirm(ctx->cli, "Есть несохранённые изменения. Сохранить перед загрузкой?");
        if (yes) {
            if (save_current(ctx) < 0) { tokens_free(&t); return 0; }
        }
    }

    commands_execute(&ctx->app, t.argc, t.argv);

    tokens_free(&t);
    return 0;
}

int main(void) {
    Ctx ctx;
    if (app_init(&ctx.app, "shelter.csv") < 0) {
        fprintf(stderr, "app_init failed\n");
        return 1;
    }

    ctx.cli = cli_create();
    if (!ctx.cli) {
        fprintf(stderr, "cli_create failed\n");
        app_free(&ctx.app);
        return 1;
    }

    int rc = cli_run(ctx.cli, &ctx, on_line);

    cli_free(ctx.cli);
    app_free(&ctx.app);
    return (rc < 0) ? 1 : 0;
}

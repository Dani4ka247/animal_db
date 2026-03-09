#include "cli.h"
#include <histedit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

struct Cli {
    EditLine *el;
    History *hist;
};

static const char *commands[] = {
    "help", "history", "exit", "quit",
    "add", "show", "save", "load",
    NULL
};

static char *prompt(EditLine *e) { (void)e; return "> "; }

static const char *gen_text;
static size_t gen_len;
static int gen_idx;

static const char *gen_cmd(void) {
    while (commands[gen_idx]) {
        const char *name = commands[gen_idx++];
        if (strncasecmp(name, gen_text, gen_len) == 0) return name;
    }
    return NULL;
}

static unsigned char complete(EditLine *e, int ch) {
    (void)ch;
    const LineInfo *li = el_line(e);
    int cursor = (int)(li->cursor - li->buffer);

    int start = cursor;
    while (start > 0 && li->buffer[start - 1] != ' ' && li->buffer[start - 1] != '\t')
        start--;
    if (start != 0) return CC_ERROR;

    gen_text = li->buffer;
    gen_len  = (size_t)cursor;
    gen_idx  = 0;

    const char *first = gen_cmd();
    if (!first) return CC_ERROR;

    const char *second = gen_cmd();
    if (second) {
        fputc('\n', stdout);
        printf("%s\n%s\n", first, second);
        const char *name;
        while ((name = gen_cmd())) printf("%s\n", name);
        return CC_REDISPLAY;
    }

    if (strlen(first) > gen_len) el_insertstr(e, first + gen_len);
    return CC_REFRESH;
}

static void print_history(History *hist) {
    HistEvent ev;
    int i = 1;
    if (history(hist, &ev, H_FIRST) == 0) {
        printf("%d  %s\n", i++, ev.str);
        while (history(hist, &ev, H_NEXT) == 0) printf("%d  %s\n", i++, ev.str);
    }
}

Cli* cli_create(void) {
    Cli *cli = calloc(1, sizeof(*cli));
    if (!cli) return NULL;

    cli->el = el_init("shelter", stdin, stdout, stderr);
    if (!cli->el) { free(cli); return NULL; }

    cli->hist = history_init();
    if (!cli->hist) { el_end(cli->el); free(cli); return NULL; }

    HistEvent ev;
    history(cli->hist, &ev, H_SETSIZE, 1000);

    el_set(cli->el, EL_PROMPT, prompt);
    el_set(cli->el, EL_EDITOR, "emacs");
    el_set(cli->el, EL_HIST, history, cli->hist);

    el_set(cli->el, EL_ADDFN, "shelter-complete", "command completion", complete);
    el_set(cli->el, EL_BIND, "^I", "shelter-complete", NULL);

    return cli;
}

void cli_free(Cli *cli) {
    if (!cli) return;
    if (cli->hist) history_end(cli->hist);
    if (cli->el) el_end(cli->el);
    free(cli);
}

int cli_run(Cli *cli, void *ctx, CliOnLine on_line) {
    while (1) {
        int count = 0;
        const char *line = el_gets(cli->el, &count);
        if (!line) return 0;

        char *s = strndup(line, (size_t)count);
        if (!s) return -1;
        if (count > 0 && s[count - 1] == '\n') s[count - 1] = '\0';

        if (s[0] != '\0') {
            HistEvent ev;
            history(cli->hist, &ev, H_ENTER, s);
        }

        if (strcasecmp(s, "history") == 0) {
            print_history(cli->hist);
            free(s);
            continue;
        }

        int rc = on_line ? on_line(ctx, s) : 0;
        free(s);

        if (rc == 1) return 0;
        if (rc < 0) return rc;
    }
}

int cli_confirm(Cli *cli, const char *question) {
    while (1) {
        printf("%s [y/n] ", question);
        fflush(stdout);

        int count = 0;
        const char *line = el_gets(cli->el, &count);
        if (!line) return 0;

        char *s = strndup(line, (size_t)count);
        if (!s) return 0;
        if (count > 0 && s[count - 1] == '\n') s[count - 1] = '\0';

        char *p = s;
        while (*p == ' ' || *p == '\t') p++;

        if (*p == 'y' || *p == 'Y') { free(s); return 1; }
        if (*p == 'n' || *p == 'N') { free(s); return 0; }

        printf("Введите y или n\n");
        free(s);
    }
}


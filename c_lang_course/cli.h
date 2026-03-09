#ifndef CLI_H
#define CLI_H

typedef struct Cli Cli;

typedef int (*CliOnLine)(void *ctx, const char *line);
Cli* cli_create(void);
void cli_free(Cli *cli);

int cli_run(Cli *cli, void *ctx, CliOnLine on_line);
int cli_confirm(Cli *cli, const char *question);
#endif

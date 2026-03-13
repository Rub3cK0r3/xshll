#ifndef XSHLL_H
#define XSHLL_H

#include <stdbool.h>
#include <stddef.h>

/*
 * xshll: minimal educational shell.
 *
 * Design notes:
 * - Input is read with getline() to avoid fixed-size buffers.
 * - Parsing is separate from execution: parser builds a Pipeline, pipeline.c runs it.
 * - Built-ins run in the parent process when possible (e.g. cd/export/unset).
 */

// ---------- History ----------
typedef struct History {
  char **items;
  size_t len;
  size_t cap;
} History;

void history_init(History *h);
void history_free(History *h);
int history_add(History *h, const char *line);
const char *history_get(const History *h, size_t idx_1based); // 1-based
void history_print(const History *h);

/*
 * History expansion rules:
 * - "!!" expands to the previous command
 * - "!n" expands to history entry n (1-based)
 * Returns malloc()'d string on success (caller frees), or NULL if no expansion.
 */
char *history_expand(const History *h, const char *line, int *err);

// ---------- Parsing / AST ----------
typedef enum RedirType {
  REDIR_IN,
  REDIR_OUT_TRUNC,
  REDIR_OUT_APPEND
} RedirType;

typedef struct Redir {
  RedirType type;
  char *path; // owned
} Redir;

typedef struct Command {
  char **argv;   // NULL-terminated, owned
  size_t argc;   // not counting NULL
  Redir *redirs; // owned array
  size_t nredirs;
} Command;

typedef struct Pipeline {
  Command *cmds; // owned array
  size_t ncmds;
  bool background; // trailing '&'
} Pipeline;

void pipeline_free(Pipeline *p);

/*
 * Parse a full command line into a Pipeline:
 * - Supports quoting with single quotes (no expansions) and double quotes (env expansion).
 * - Supports environment variable expansion: $VAR and ${VAR} in unquoted/double-quoted text.
 * - Supports redirections: <, >, >> (can appear on any command in a pipeline).
 * - Supports pipes: cmd1 | cmd2 | cmd3 ...
 * - Supports background: trailing '&' (outside quotes) runs without blocking the shell.
 */
int parse_line(const char *line, Pipeline *out, char **err_msg);

// ---------- Built-ins ----------
/*
 * Runs a built-in command if argv[0] matches.
 * Returns:
 * -1: not a built-in
 *  0..255: built-in exit status
 */
int builtin_run(char **argv, History *history);
void builtin_print_help(void);

// ---------- Execution ----------
/*
 * Executes a parsed pipeline.
 *
 * Signal behavior:
 * - The interactive shell ignores SIGINT/SIGTSTP so Ctrl+C / Ctrl+Z does not kill the shell.
 * - Child processes restore default signal handlers so foreground jobs behave normally.
 *
 * Background behavior:
 * - If pipeline->background is true, returns immediately after spawning.
 */
int execute_pipeline(const Pipeline *p);

// ---------- UI ----------
void welcome_screen(void);
void print_prompt(int last_status);

// ---------- Signals ----------
int install_shell_signal_handlers(void);

#endif // XSHLL_H

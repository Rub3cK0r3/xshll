#include "xshll.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int bi_cd(char **argv) {
  // cd [DIR] ; if DIR omitted, go to $HOME.
  const char *target = argv[1];
  if (!target) target = getenv("HOME");
  if (!target) {
    fprintf(stderr, "xshll: cd: HOME not set\n");
    return 1;
  }
  if (chdir(target) != 0) {
    fprintf(stderr, "xshll: cd: %s: %s\n", target, strerror(errno));
    return 1;
  }
  return 0;
}

static int bi_pwd(void) {
  char *cwd = getcwd(NULL, 0);
  if (!cwd) {
    fprintf(stderr, "xshll: pwd: %s\n", strerror(errno));
    return 1;
  }
  puts(cwd);
  free(cwd);
  return 0;
}

static int bi_echo(char **argv) {
  // echo [TEXT...] ; minimal (no flags).
  for (int i = 1; argv[i]; i++) {
    fputs(argv[i], stdout);
    if (argv[i + 1]) fputc(' ', stdout);
  }
  fputc('\n', stdout);
  return 0;
}

static int bi_export(char **argv) {
  // export KEY=VALUE [KEY=VALUE...]
  if (!argv[1]) {
    fprintf(stderr, "xshll: export: expected KEY=VALUE\n");
    return 2;
  }
  for (int i = 1; argv[i]; i++) {
    char *eq = strchr(argv[i], '=');
    if (!eq || eq == argv[i]) {
      fprintf(stderr, "xshll: export: invalid: %s\n", argv[i]);
      return 2;
    }
    *eq = '\0';
    const char *key = argv[i];
    const char *val = eq + 1;
    if (setenv(key, val, 1) != 0) {
      fprintf(stderr, "xshll: export: %s: %s\n", key, strerror(errno));
      *eq = '=';
      return 1;
    }
    *eq = '=';
  }
  return 0;
}

static int bi_unset(char **argv) {
  // unset KEY [KEY...]
  if (!argv[1]) {
    fprintf(stderr, "xshll: unset: expected KEY\n");
    return 2;
  }
  for (int i = 1; argv[i]; i++) {
    if (unsetenv(argv[i]) != 0) {
      fprintf(stderr, "xshll: unset: %s: %s\n", argv[i], strerror(errno));
      return 1;
    }
  }
  return 0;
}

void builtin_print_help(void) {
  puts("XSHLL - A minimal educational shell");
  puts("");
  puts("Built-ins:");
  puts("  xhelp                 Display this help message");
  puts("  exit                  Exit the shell");
  puts("  cd [DIR]              Change directory (default: $HOME)");
  puts("  pwd                   Print current directory");
  puts("  echo [TEXT...]        Print arguments");
  puts("  export KEY=VALUE ...  Set environment variable(s)");
  puts("  unset KEY ...         Unset environment variable(s)");
  puts("  history               Show command history");
  puts("");
  puts("Notes:");
  puts("  - Quotes: 'single' (literal), \"double\" (allows $VAR expansion)");
  puts("  - Redirection: <, >, >> ; Pipes: | ; Background: trailing &");
  puts("  - History expansion: !! and !n (1-based)");
}

int builtin_run(char **argv, History *history) {
  if (!argv || !argv[0]) return -1;

  if (strcmp(argv[0], "exit") == 0) {
    exit(0);
  }
  if (strcmp(argv[0], "xhelp") == 0) return (builtin_print_help(), 0);
  if (strcmp(argv[0], "cd") == 0) return bi_cd(argv);
  if (strcmp(argv[0], "pwd") == 0) return bi_pwd();
  if (strcmp(argv[0], "echo") == 0) return bi_echo(argv);
  if (strcmp(argv[0], "export") == 0) return bi_export(argv);
  if (strcmp(argv[0], "unset") == 0) return bi_unset(argv);
  if (strcmp(argv[0], "history") == 0) {
    (void)history;
    if (history) history_print(history);
    return 0;
  }

  return -1;
}


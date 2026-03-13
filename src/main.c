#include "xshll.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

void welcome_screen(void) {
  puts("                .__     .__   .__   ");
  puts("___  ___  ______|  |__  |  |  |  |  ");
  puts("\\  \\/  / /  ___/|  |  \\ |  |  |  |  ");
  puts(" >    <  \\___ \\ |   Y  \\|  |__|  |__");
  puts("/__/\\_ \\/____  >|___|  /|____/|____/");
  puts("      \\/     \\/      \\/             ");
}

void print_prompt(int last_status) {
  /*
   * ANSI prompt:
   * - Shows user@host:cwd and last exit status.
   * - Uses getcwd(NULL,0) to avoid fixed buffers.
   */
  char hostname[256] = "unknown";
  if (gethostname(hostname, sizeof(hostname)) != 0) {
    strncpy(hostname, "unknown", sizeof(hostname));
    hostname[sizeof(hostname) - 1] = '\0';
  }

  const char *user = getlogin();
  if (!user) user = "unknown";

  char *cwd = getcwd(NULL, 0);
  if (!cwd) cwd = strdup("unknown");

  const char *home = getenv("HOME");
  const char *cwd_disp = cwd;
  char *tmp = NULL;
  if (home && cwd && strstr(cwd, home) == cwd) {
    size_t n = strlen(cwd) - strlen(home) + 2;
    tmp = malloc(n);
    if (tmp) {
      snprintf(tmp, n, "~%s", cwd + strlen(home));
      cwd_disp = tmp;
    }
  }

  // Colors: green user/host, blue path, dim status.
  fprintf(stdout, "\033[2m[%d]\033[0m \033[32m%s@%s\033[0m:\033[34m%s\033[0m$ ",
          last_status, user, hostname, cwd_disp);
  fflush(stdout);

  free(tmp);
  free(cwd);
}

static void reap_background_children(void) {
  int st;
  while (waitpid(-1, &st, WNOHANG) > 0) {
    // Optional notification point (kept silent by default).
  }
}

int main(void) {
  welcome_screen();

  if (install_shell_signal_handlers() != 0) {
    fprintf(stderr, "xshll: failed to install signal handlers\n");
  }

  History hist;
  history_init(&hist);

  char *line = NULL;
  size_t cap = 0;
  int last_status = 0;

  for (;;) {
    reap_background_children();
    print_prompt(last_status);

    errno = 0;
    ssize_t nread = getline(&line, &cap, stdin);
    if (nread < 0) {
      if (feof(stdin)) break;
      if (errno == EINTR) {
        clearerr(stdin);
        fputc('\n', stdout);
        continue;
      }
      perror("xshll: getline");
      break;
    }

    // Strip trailing newline
    if (nread > 0 && line[nread - 1] == '\n') line[nread - 1] = '\0';

    // Skip empty
    char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0') continue;

    int exp_err = 0;
    char *expanded = history_expand(&hist, line, &exp_err);
    const char *to_run = expanded ? expanded : line;
    if (exp_err != 0) {
      fprintf(stderr, "xshll: history expansion failed\n");
      free(expanded);
      last_status = 1;
      continue;
    }

    // Add the final command line to history (after expansion).
    history_add(&hist, to_run);

    Pipeline pl = {0};
    char *perr = NULL;
    if (parse_line(to_run, &pl, &perr) != 0) {
      fprintf(stderr, "xshll: %s\n", perr ? perr : "parse error");
      free(perr);
      pipeline_free(&pl);
      free(expanded);
      last_status = 2;
      continue;
    }

    if (pl.ncmds == 0) {
      pipeline_free(&pl);
      free(expanded);
      continue;
    }

    // Built-ins only run directly when it's a single command and foreground.
    if (pl.ncmds == 1 && !pl.background) {
      int bi = builtin_run(pl.cmds[0].argv, &hist);
      if (bi >= 0) {
        last_status = bi;
        pipeline_free(&pl);
        free(expanded);
        continue;
      }
    }

    last_status = execute_pipeline(&pl);
    pipeline_free(&pl);
    free(expanded);
  }

  free(line);
  history_free(&hist);
  fputc('\n', stdout);
  return 0;
}


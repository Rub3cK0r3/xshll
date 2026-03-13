#include "xshll.h"

#include <signal.h>
#include <string.h>

/*
 * Interactive shell signal behavior:
 * - Ignore SIGINT (Ctrl+C) so the shell prompt survives.
 * - Ignore SIGTSTP (Ctrl+Z) so the shell itself is not suspended.
 *
 * Foreground/background jobs:
 * - Child processes restore default handlers in `execute_pipeline()`, so Ctrl+C
 *   and Ctrl+Z behave normally for programs launched by the shell.
 */
int install_shell_signal_handlers(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_IGN;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  if (sigaction(SIGINT, &sa, NULL) != 0) return -1;
  if (sigaction(SIGTSTP, &sa, NULL) != 0) return -1;
  return 0;
}


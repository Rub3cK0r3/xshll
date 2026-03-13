#include "xshll.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void restore_child_signals(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_DFL;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTSTP, &sa, NULL);
}

static int apply_redirections(const Command *c) {
  for (size_t i = 0; i < c->nredirs; i++) {
    const Redir *r = &c->redirs[i];
    int fd = -1;

    if (r->type == REDIR_IN) {
      fd = open(r->path, O_RDONLY);
      if (fd < 0) {
        fprintf(stderr, "xshll: %s: %s\n", r->path, strerror(errno));
        return 1;
      }
      if (dup2(fd, STDIN_FILENO) < 0) {
        fprintf(stderr, "xshll: dup2: %s\n", strerror(errno));
        close(fd);
        return 1;
      }
      close(fd);
    } else if (r->type == REDIR_OUT_TRUNC) {
      fd = open(r->path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd < 0) {
        fprintf(stderr, "xshll: %s: %s\n", r->path, strerror(errno));
        return 1;
      }
      if (dup2(fd, STDOUT_FILENO) < 0) {
        fprintf(stderr, "xshll: dup2: %s\n", strerror(errno));
        close(fd);
        return 1;
      }
      close(fd);
    } else { // REDIR_OUT_APPEND
      fd = open(r->path, O_WRONLY | O_CREAT | O_APPEND, 0644);
      if (fd < 0) {
        fprintf(stderr, "xshll: %s: %s\n", r->path, strerror(errno));
        return 1;
      }
      if (dup2(fd, STDOUT_FILENO) < 0) {
        fprintf(stderr, "xshll: dup2: %s\n", strerror(errno));
        close(fd);
        return 1;
      }
      close(fd);
    }
  }
  return 0;
}

int execute_pipeline(const Pipeline *p) {
  if (!p || p->ncmds == 0) return 0;

  size_t n = p->ncmds;
  pid_t *pids = calloc(n, sizeof(pid_t));
  if (!pids) {
    fprintf(stderr, "xshll: out of memory\n");
    return 1;
  }

  int prev_read = -1;
  int status_last = 0;

  /*
   * Pipeline strategy:
   * - For each stage i, create a pipe for i->i+1 (except last stage).
   * - Fork child, wire stdin/stdout with dup2, close all unrelated fds.
   * - Parent closes pipe ends it no longer needs.
   * - Parent waits for all children unless background.
   */
  for (size_t i = 0; i < n; i++) {
    int pipefd[2] = {-1, -1};
    if (i + 1 < n) {
      if (pipe(pipefd) < 0) {
        fprintf(stderr, "xshll: pipe: %s\n", strerror(errno));
        free(pids);
        if (prev_read >= 0) close(prev_read);
        return 1;
      }
    }

    pid_t pid = fork();
    if (pid < 0) {
      fprintf(stderr, "xshll: fork: %s\n", strerror(errno));
      if (pipefd[0] >= 0) close(pipefd[0]);
      if (pipefd[1] >= 0) close(pipefd[1]);
      free(pids);
      if (prev_read >= 0) close(prev_read);
      return 1;
    }

    if (pid == 0) {
      restore_child_signals();

      if (prev_read >= 0) {
        if (dup2(prev_read, STDIN_FILENO) < 0) {
          fprintf(stderr, "xshll: dup2: %s\n", strerror(errno));
          _exit(1);
        }
      }

      if (i + 1 < n) {
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
          fprintf(stderr, "xshll: dup2: %s\n", strerror(errno));
          _exit(1);
        }
      }

      if (prev_read >= 0) close(prev_read);
      if (pipefd[1] >= 0) close(pipefd[1]);

      int redir_rc = apply_redirections(&p->cmds[i]);
      if (redir_rc != 0) _exit(redir_rc);

      execvp(p->cmds[i].argv[0], p->cmds[i].argv);
      fprintf(stderr, "xshll: %s: %s\n", p->cmds[i].argv[0], strerror(errno));
      _exit(127);
    }

    pids[i] = pid;

    if (prev_read >= 0) close(prev_read);
    prev_read = -1;
    if (i + 1 < n) {
      close(pipefd[1]);
      prev_read = pipefd[0];
    }
  }

  if (prev_read >= 0) close(prev_read);

  if (p->background) {
    // Non-blocking: caller can optionally track pids, we just return success.
    free(pids);
    return 0;
  }

  // Wait for all; report exit status of last command in pipeline.
  for (size_t i = 0; i < n; i++) {
    int st = 0;
    if (waitpid(pids[i], &st, 0) < 0) {
      fprintf(stderr, "xshll: waitpid: %s\n", strerror(errno));
      status_last = 1;
      continue;
    }
    if (i + 1 == n) {
      if (WIFEXITED(st)) status_last = WEXITSTATUS(st);
      else if (WIFSIGNALED(st)) status_last = 128 + WTERMSIG(st);
      else status_last = 1;
    }
  }

  free(pids);
  return status_last;
}


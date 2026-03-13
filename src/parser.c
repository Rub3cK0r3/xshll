#include "xshll.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct StrVec {
  char **items;
  size_t len;
  size_t cap;
} StrVec;

static void sv_free(StrVec *v) {
  if (!v) return;
  for (size_t i = 0; i < v->len; i++) free(v->items[i]);
  free(v->items);
  v->items = NULL;
  v->len = 0;
  v->cap = 0;
}

static int sv_push(StrVec *v, char *s_owned) {
  if (v->len + 1 > v->cap) {
    size_t ncap = v->cap ? v->cap * 2 : 8;
    char **nitems = realloc(v->items, ncap * sizeof(*nitems));
    if (!nitems) return -1;
    v->items = nitems;
    v->cap = ncap;
  }
  v->items[v->len++] = s_owned;
  return 0;
}

typedef struct Buf {
  char *data;
  size_t len;
  size_t cap;
} Buf;

static void buf_free(Buf *b) {
  free(b->data);
  b->data = NULL;
  b->len = 0;
  b->cap = 0;
}

static int buf_reserve(Buf *b, size_t need) {
  if (need <= b->cap) return 0;
  size_t ncap = b->cap ? b->cap : 64;
  while (ncap < need) ncap *= 2;
  char *ndata = realloc(b->data, ncap);
  if (!ndata) return -1;
  b->data = ndata;
  b->cap = ncap;
  return 0;
}

static int buf_putc(Buf *b, char c) {
  if (buf_reserve(b, b->len + 2) != 0) return -1;
  b->data[b->len++] = c;
  b->data[b->len] = '\0';
  return 0;
}

static int buf_puts(Buf *b, const char *s) {
  size_t n = strlen(s);
  if (buf_reserve(b, b->len + n + 1) != 0) return -1;
  memcpy(b->data + b->len, s, n + 1);
  b->len += n;
  return 0;
}

static char *xstrdup(const char *s) {
  if (!s) return NULL;
  size_t n = strlen(s) + 1;
  char *p = malloc(n);
  if (!p) return NULL;
  memcpy(p, s, n);
  return p;
}

static int is_var_start(int c) { return isalpha(c) || c == '_'; }
static int is_var_char(int c) { return isalnum(c) || c == '_'; }

static int expand_var_at(const char *line, size_t *i, Buf *out) {
  // line[*i] == '$' on entry. Updates *i to last consumed char.
  size_t p = *i + 1;
  if (line[p] == '{') {
    p++;
    size_t start = p;
    while (line[p] && line[p] != '}') p++;
    if (line[p] != '}') return -1;
    size_t len = p - start;
    char name[256];
    if (len == 0 || len >= sizeof(name)) return -1;
    memcpy(name, line + start, len);
    name[len] = '\0';
    const char *val = getenv(name);
    if (val && buf_puts(out, val) != 0) return -1;
    *i = p; // points to '}'
    return 0;
  }

  if (!is_var_start((unsigned char)line[p])) {
    // Treat '$' literally if not a valid var name start.
    return buf_putc(out, '$');
  }

  size_t start = p;
  while (line[p] && is_var_char((unsigned char)line[p])) p++;
  size_t len = p - start;
  char name[256];
  if (len >= sizeof(name)) return -1;
  memcpy(name, line + start, len);
  name[len] = '\0';
  const char *val = getenv(name);
  if (val && buf_puts(out, val) != 0) return -1;
  *i = p - 1;
  return 0;
}

/*
 * Tokenizer:
 * - Splits on whitespace unless inside quotes.
 * - Recognizes operators |, <, >, >>, & as separate tokens when unquoted.
 * - Performs env expansion in unquoted and double-quoted contexts.
 * - Single quotes are literal (no expansion).
 */
static int tokenize(const char *line, StrVec *tokens, char **err) {
  enum { Q_NONE, Q_SINGLE, Q_DOUBLE } q = Q_NONE;
  Buf cur = {0};

  for (size_t i = 0; line[i]; i++) {
    char c = line[i];

    if (q == Q_NONE) {
      if (isspace((unsigned char)c)) {
        if (cur.len) {
          if (sv_push(tokens, xstrdup(cur.data)) != 0) goto oom;
          cur.len = 0;
          if (cur.data) cur.data[0] = '\0';
        }
        continue;
      }

      if (c == '\'') {
        q = Q_SINGLE;
        continue;
      }
      if (c == '"') {
        q = Q_DOUBLE;
        continue;
      }

      // Operators
      if (c == '|' || c == '<' || c == '&') {
        if (cur.len) {
          if (sv_push(tokens, xstrdup(cur.data)) != 0) goto oom;
          cur.len = 0;
          if (cur.data) cur.data[0] = '\0';
        }
        char op[2] = {c, '\0'};
        if (sv_push(tokens, xstrdup(op)) != 0) goto oom;
        continue;
      }
      if (c == '>') {
        if (cur.len) {
          if (sv_push(tokens, xstrdup(cur.data)) != 0) goto oom;
          cur.len = 0;
          if (cur.data) cur.data[0] = '\0';
        }
        if (line[i + 1] == '>') {
          if (sv_push(tokens, xstrdup(">>")) != 0) goto oom;
          i++;
        } else {
          if (sv_push(tokens, xstrdup(">")) != 0) goto oom;
        }
        continue;
      }

      if (c == '$') {
        if (expand_var_at(line, &i, &cur) != 0) goto badvar;
        continue;
      }

      if (buf_putc(&cur, c) != 0) goto oom;
      continue;
    }

    if (q == Q_SINGLE) {
      if (c == '\'') {
        q = Q_NONE;
        continue;
      }
      if (buf_putc(&cur, c) != 0) goto oom;
      continue;
    }

    // Q_DOUBLE
    if (c == '"') {
      q = Q_NONE;
      continue;
    }
    if (c == '$') {
      if (expand_var_at(line, &i, &cur) != 0) goto badvar;
      continue;
    }
    if (buf_putc(&cur, c) != 0) goto oom;
  }

  if (q != Q_NONE) {
    *err = xstrdup("unterminated quote");
    buf_free(&cur);
    return -1;
  }

  if (cur.len) {
    if (sv_push(tokens, xstrdup(cur.data)) != 0) goto oom;
  }
  buf_free(&cur);
  return 0;

oom:
  *err = xstrdup("out of memory");
  buf_free(&cur);
  return -1;
badvar:
  *err = xstrdup("invalid environment variable expansion");
  buf_free(&cur);
  return -1;
}

static void cmd_free(Command *c) {
  if (!c) return;
  if (c->argv) {
    for (size_t i = 0; c->argv[i]; i++) free(c->argv[i]);
    free(c->argv);
  }
  for (size_t i = 0; i < c->nredirs; i++) free(c->redirs[i].path);
  free(c->redirs);
  memset(c, 0, sizeof(*c));
}

void pipeline_free(Pipeline *p) {
  if (!p) return;
  for (size_t i = 0; i < p->ncmds; i++) cmd_free(&p->cmds[i]);
  free(p->cmds);
  memset(p, 0, sizeof(*p));
}

static int cmd_add_arg(Command *c, const char *s) {
  char **nargv = realloc(c->argv, (c->argc + 2) * sizeof(char *));
  if (!nargv) return -1;
  c->argv = nargv;
  c->argv[c->argc] = xstrdup(s);
  if (!c->argv[c->argc]) return -1;
  c->argc++;
  c->argv[c->argc] = NULL;
  return 0;
}

static int cmd_add_redir(Command *c, RedirType t, const char *path) {
  Redir *nr = realloc(c->redirs, (c->nredirs + 1) * sizeof(Redir));
  if (!nr) return -1;
  c->redirs = nr;
  c->redirs[c->nredirs].type = t;
  c->redirs[c->nredirs].path = xstrdup(path);
  if (!c->redirs[c->nredirs].path) return -1;
  c->nredirs++;
  return 0;
}

int parse_line(const char *line, Pipeline *out, char **err_msg) {
  if (err_msg) *err_msg = NULL;
  if (!out) return -1;
  memset(out, 0, sizeof(*out));

  StrVec toks = {0};
  char *tok_err = NULL;
  if (tokenize(line, &toks, &tok_err) != 0) {
    if (err_msg) *err_msg = tok_err ? tok_err : xstrdup("parse error");
    sv_free(&toks);
    return -1;
  }

  // Empty line
  if (toks.len == 0) {
    sv_free(&toks);
    return 0;
  }

  // Background marker must be last token: &
  if (toks.len >= 1 && strcmp(toks.items[toks.len - 1], "&") == 0) {
    out->background = true;
    free(toks.items[toks.len - 1]);
    toks.len--;
  }

  if (toks.len == 0) {
    if (err_msg) *err_msg = xstrdup("empty command before '&'");
    sv_free(&toks);
    return -1;
  }

  // Build commands split by pipes
  Command cur = {0};
  bool have_any = false;

  for (size_t i = 0; i < toks.len; i++) {
    const char *t = toks.items[i];

    if (strcmp(t, "|") == 0) {
      if (cur.argc == 0) {
        if (err_msg) *err_msg = xstrdup("empty command in pipeline");
        cmd_free(&cur);
        pipeline_free(out);
        sv_free(&toks);
        return -1;
      }
      Command *ncmds = realloc(out->cmds, (out->ncmds + 1) * sizeof(Command));
      if (!ncmds) goto oom;
      out->cmds = ncmds;
      out->cmds[out->ncmds++] = cur;
      memset(&cur, 0, sizeof(cur));
      have_any = true;
      continue;
    }

    if (strcmp(t, "<") == 0 || strcmp(t, ">") == 0 || strcmp(t, ">>") == 0) {
      if (i + 1 >= toks.len) {
        if (err_msg) *err_msg = xstrdup("redirection missing path");
        cmd_free(&cur);
        pipeline_free(out);
        sv_free(&toks);
        return -1;
      }
      const char *path = toks.items[++i];
      RedirType rt = REDIR_IN;
      if (strcmp(t, "<") == 0) rt = REDIR_IN;
      else if (strcmp(t, ">") == 0) rt = REDIR_OUT_TRUNC;
      else rt = REDIR_OUT_APPEND;
      if (cmd_add_redir(&cur, rt, path) != 0) goto oom;
      continue;
    }

    if (cmd_add_arg(&cur, t) != 0) goto oom;
  }

  if (cur.argc == 0 && (have_any || out->ncmds > 0)) {
    if (err_msg) *err_msg = xstrdup("empty command at end of pipeline");
    cmd_free(&cur);
    pipeline_free(out);
    sv_free(&toks);
    return -1;
  }

  if (cur.argc) {
    Command *ncmds = realloc(out->cmds, (out->ncmds + 1) * sizeof(Command));
    if (!ncmds) goto oom;
    out->cmds = ncmds;
    out->cmds[out->ncmds++] = cur;
    memset(&cur, 0, sizeof(cur));
  } else {
    cmd_free(&cur);
  }

  // Cleanup tokens
  sv_free(&toks);
  return 0;

oom:
  if (err_msg) *err_msg = xstrdup("out of memory");
  cmd_free(&cur);
  pipeline_free(out);
  sv_free(&toks);
  return -1;
}


#include "xshll.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *xstrdup(const char *s) {
  if (!s) return NULL;
  size_t n = strlen(s) + 1;
  char *p = malloc(n);
  if (!p) return NULL;
  memcpy(p, s, n);
  return p;
}

void history_init(History *h) { memset(h, 0, sizeof(*h)); }

void history_free(History *h) {
  if (!h) return;
  for (size_t i = 0; i < h->len; i++) free(h->items[i]);
  free(h->items);
  memset(h, 0, sizeof(*h));
}

int history_add(History *h, const char *line) {
  if (!h || !line) return -1;
  if (h->len + 1 > h->cap) {
    size_t ncap = h->cap ? h->cap * 2 : 64;
    char **nitems = realloc(h->items, ncap * sizeof(*nitems));
    if (!nitems) return -1;
    h->items = nitems;
    h->cap = ncap;
  }
  h->items[h->len] = xstrdup(line);
  if (!h->items[h->len]) return -1;
  h->len++;
  return 0;
}

const char *history_get(const History *h, size_t idx_1based) {
  if (!h || idx_1based == 0) return NULL;
  size_t i = idx_1based - 1;
  if (i >= h->len) return NULL;
  return h->items[i];
}

void history_print(const History *h) {
  if (!h) return;
  for (size_t i = 0; i < h->len; i++) {
    printf("%zu  %s\n", i + 1, h->items[i]);
  }
}

static int parse_num(const char *s, long *out) {
  if (!s || !*s) return -1;
  char *end = NULL;
  errno = 0;
  long v = strtol(s, &end, 10);
  if (errno != 0 || end == s || *end != '\0' || v <= 0) return -1;
  *out = v;
  return 0;
}

char *history_expand(const History *h, const char *line, int *err) {
  if (err) *err = 0;
  if (!h || !line) return NULL;

  if (strcmp(line, "!!") == 0) {
    if (h->len == 0) {
      if (err) *err = 1;
      return NULL;
    }
    return xstrdup(h->items[h->len - 1]);
  }

  if (line[0] == '!' && line[1] != '\0') {
    long n = 0;
    if (parse_num(line + 1, &n) != 0) {
      if (err) *err = 1;
      return NULL;
    }
    const char *hit = history_get(h, (size_t)n);
    if (!hit) {
      if (err) *err = 1;
      return NULL;
    }
    return xstrdup(hit);
  }

  return NULL;
}


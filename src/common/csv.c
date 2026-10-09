#include "csv.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

struct Csv { FILE *f; };

Csv *csv_open(const char *path, const char *header) {
  FILE *f = fopen(path, "w");
  if (!f) return NULL;
  if (header && *header) fprintf(f, "%s\n", header);
  Csv *c = malloc(sizeof(*c));
  if (!c) { fclose(f); return NULL; }
  c->f = f;
  return c;
}

void csv_row(Csv *c, const char *fmt, ...) {
  if (!c || !c->f) return;
  va_list ap;
  va_start(ap, fmt);
  vfprintf(c->f, fmt, ap);
  va_end(ap);
  fputc('\n', c->f);
}

void csv_close(Csv *c) {
  if (!c) return;
  if (c->f) fclose(c->f);
  free(c);
}

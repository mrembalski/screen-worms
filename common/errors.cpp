#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "errors.h"

void fatal(const char *fmt, ...)
{
  va_list fmt_args;

  fprintf(stderr, "ERROR: ");
  va_start(fmt_args, fmt);

  vfprintf(stderr, fmt, fmt_args);
  va_end(fmt_args);

  fprintf(stderr, "\n");
  exit(EXIT_FAILURE);
}

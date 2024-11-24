#include "mini.h"
#include <stdio.h>

int main(int argc, char ** argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: test [ini file]\n");
    return 1;
  }

  FILE* f = fopen(argv[1], "r");
  if (!f) {
    fprintf(stderr, "file not found\n");
    return 1;
  }

  ini ini = {0};
  ini_errs errs = {0};

  ini__nextfile(&ini, &errs, f);
  fclose(f);

  if (errs.msg_len > 0) {
    ini_errs__print(errs, stderr);
    return 1;
  }

  ini__debug(ini, stdout);
  ini__free(ini);

  return 0;
}

#include "mini.h"
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

void ini_ent__free(ini_ent e) {
  free(e.k);
  free(e.v);
}

void ini_sectionh__debug(ini_sectionh s, FILE* dest) {
  for (size_t i = 0; i < s.len; i ++) {
    if (i > 0) fprintf(dest, ".");
    fprintf(dest, "%s", s.parts[i]);
  }
}

void ini_sectionh__free(ini_sectionh s) {
  free(s.parts);
  free(s.free_me);
}

void ini_section__debug(ini_section s, FILE* dest) {
  if (!s.is_root) {
    fprintf(dest, "[");
    ini_sectionh__debug(s.header, dest);
    fprintf(dest, "]\n");
  }
  for (size_t i = 0; i < s.ents_len; i ++) {
    ini_ent e = s.ents[i];
    fprintf(dest, "%s = %s\n", e.k, e.v);
  }
}

void ini_section__free(ini_section s) {
  if (!s.is_root) ini_sectionh__free(s.header);
  for (size_t i = 0; i < s.ents_len; i ++)
    ini_ent__free(s.ents[i]);
  free(s.ents);
}

static void ini_section__addent(ini_section *s, ini_ent e) {
  s->ents = realloc(s->ents, (s->ents_len + 1) * sizeof(ini_ent));
  s->ents[s->ents_len ++] = e;
}

void ini__debug(ini ini, FILE* dest) {
  for (size_t i = 0; i < ini.len; i ++) {
    if (i > 0)
      fprintf(dest, "\n");
    ini_section__debug(ini.items[i], dest);
  }
}

void ini__free(ini ini) {
  for (size_t i = 0; i < ini.len; i ++)
    ini_section__free(ini.items[i]);
}

static void ini__add(ini* ini, ini_section s) {
  ini->items = realloc(ini->items, (ini->len + 1) * sizeof(ini_section));
  ini->items[ini->len ++] = s;
}

void ini_errs__free(ini_errs e) {
  for (size_t i = 0; i < e.msg_len; i ++)
    free(e.msg[i]);
  free(e.msg);
}

void ini_errs__print(ini_errs e, FILE* dest) {
  for (size_t i = 0; i < e.msg_len; i ++)
    fprintf(dest, "%s\n", e.msg[i]);
}

static void ini_errs__add(ini_errs* errs, char * msg) {
  errs->msg = realloc(errs->msg, (errs->msg_len + 1) * sizeof(char *));
  errs->msg[errs->msg_len ++] = msg;
}

static void ini__line_err(ini_line_errs* errs, const char * msg) {
  errs->msg = realloc(errs->msg, (errs->msg_len + 1) * sizeof(const char *));
  errs->msg[errs->msg_len ++] = msg;
}

void ini_line_errs__to_errs(ini_errs* errs, ini_line_errs linerrs, size_t lnum) {
  for (size_t i = 0; i < linerrs.msg_len; i ++) {
    const char * err = linerrs.msg[i];
    char * fmt = malloc(strlen(err) + 40);
    sprintf(fmt, "in line %zu: %s", lnum, err);
    ini_errs__add(errs, fmt);
  }
  free(linerrs.msg);
}

static char parse_char(ini_line_errs* errs, char** reader) {
  char c = (*reader)[0];
  if (c == '\0') return '\0';
  (*reader)++;
  if (c == '\\') {
    c = (*reader)[0];
    switch (c) {
      case 'n':  c = '\n'; break;
      case 'r':  c = '\r'; break;
      case 't':  c = '\t'; break;
      case 'b':  c = '\b'; break;
      case '\\': c = '\\'; break;
      case '#':  c = '#' ; break;
      default:
        ini__line_err(errs, "invalid escape");
        return c;
    }
    (*reader)++;
  }
  return c;
}

static void skip_white(char** reader) {
  while (isspace(**reader))
    (*reader)++;
}

static char * find_comment(char * s) {
  for (; *s; s ++) {
    if (*s =='\\')
      s ++;
    if (*s == '#')
      return s;
  }
  return NULL;
}

static size_t strcount(const char *s, char c) {
  size_t r = 0;
  for (; *s; s ++)
    if (*s == c)
      r ++;
  return r;
}

static void strcatc(char* str, char c) {
  char cc[] = { c, 0 };
  strcat(str, cc);
}

static char * trim_start_end(char* buf) {
  skip_white(&buf);

  char* i = buf;
  char* end = buf;
  for (; *i; i ++) {
    if (!isspace(*i))
      end = i + 1;
  }
  *end = '\0';

  return buf;
}

// line is modified but allocation borrowed
void ini__nextline(ini* ini, ini_line_errs* errs, char * line) {
  skip_white(&line);
  char* comment = find_comment(line);
  if (comment) *comment = '\0';

  if (*line == '[') {
    // TODO: support relative. examplel: [.names]
    line ++;
    char* end = strchr(line, ']');
    if (!end) {
      ini__line_err(errs, "missing ]");
      return;
    }
    *end = '\0';
    ini_sectionh h;
    h.len = strcount(line, '.') + 1;
    h.parts = malloc(h.len * sizeof(*h.parts));
    char* cpy = strdup(line);
    h.free_me = cpy;
    h.parts[0] = cpy;
    size_t i = 1;
    for (; *cpy; cpy ++) {
      if (*cpy == '.') {
        *cpy = '\0';
        cpy ++;
        h.parts[i++] = cpy;
      }
    }
    ini_section s;
    s.is_root = false;
    s.header = h;
    s.ents = NULL;
    s.ents_len = 0;
    ini__add(ini, s);
    return;
  }

  if (!*line)
    return;

  char* sep = strchr(line, '=');
  if (!sep) sep = strchr(line, ':');

  if (!sep) {
    ini__line_err(errs, "missing separator");
    return;
  }

  *sep = '\0';
  char* val = sep + 1;

  line = trim_start_end(line); //k
  val = trim_start_end(val);

  ini_ent e;
  e.k = malloc(strlen(line)+1);
  e.k[0] = '\0';
  e.v = malloc(strlen(line)+1);
  e.v[0] = '\0';

  char c;
  while ((c = parse_char(errs, &line)) != '\0') {
    strcatc(e.k, c);
  }
  while ((c = parse_char(errs, &val)) != '\0') {
    strcatc(e.v, c);
  }

  if (!ini->len) {
    ini_section s;
    s.is_root = true;
    s.ents = NULL;
    s.ents_len = 0;
    ini__add(ini, s);
  }
  ini_section* s = &ini->items[ini->len - 1];

  ini_section__addent(s, e);
}

void ini__nextfile(ini* ini, ini_errs* errs, FILE* file) {
  size_t linum = 1;
  for (char buf[256]; fgets(buf, sizeof(buf), file); linum ++) {
    ini_line_errs lerr = {0};
    ini__nextline(ini, &lerr, buf);
    ini_line_errs__to_errs(errs, lerr, linum);
  }
}

static bool strarr_match(char const* const* a, size_t alen,
                         char const* const* b, size_t blen) {
  if (alen != blen) return false;
  for (size_t i = 0; i < alen; i ++)
    if (strcmp(a[i], b[i]))
      return false;
  return true;
}

/** return is nullable; the returned pointer lives as long as the ini */
ini_section* ini__find_sectionp(ini* ini, char const* const* path, size_t path_len) {
  for (size_t i = 0; i < ini->len; i ++) {
    ini_section* s = &ini->items[i];
    if (s->is_root) continue;
    if (!strarr_match(s->header.parts, s->header.len, path, path_len)) continue;
    return s;
  }
  return NULL;
}

/** return is nullable; the returned pointer lives as long as the ini; path is null terminated */
ini_section* ini__find_sectionpn(ini* ini, char const* const* path) {
  size_t plen = 0;
  for (; path[plen]; plen ++);
  return ini__find_sectionp(ini, path, plen);
}

/** return is nullable; the returned pointer lives as long as the ini; propagates NULL of section */ 
const char * ini_section__find(ini_section* section, const char * search) {
  if (section == NULL) return NULL;
  for (size_t i = 0; i < section->ents_len; i ++)
    if (!strcmp(section->ents[i].k, search))
      return section->ents[i].v;
  return NULL;
}

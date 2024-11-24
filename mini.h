#ifndef _MINI_H
#define _MINI_H

#include <stdbool.h>
#include <stdio.h>

typedef struct {
  char * k;
  char * v;
} ini_ent;
void ini_ent__free(ini_ent e);

typedef struct {
  char * free_me;
  char const ** parts;
  size_t len;
} ini_sectionh;
void ini_sectionh__debug(ini_sectionh s, FILE* dest);
void ini_sectionh__free(ini_sectionh s);

typedef struct {
  bool is_root; // if true, doesn't have header
  ini_sectionh header;
  ini_ent * ents;
  size_t ents_len;
} ini_section;
void ini_section__debug(ini_section s, FILE* dest);
void ini_section__free(ini_section s);

typedef struct {
  ini_section * items;
  size_t len;
} ini;
void ini__debug(ini ini, FILE* dest);
void ini__free(ini ini);

typedef struct {
  char ** msg;
  size_t  msg_len;
} ini_errs;
void ini_errs__free(ini_errs e);
void ini_errs__print(ini_errs e, FILE* dest);

typedef struct {
  const char ** msg;
  size_t msg_len;
} ini_line_errs;
void ini_line_errs__to_errs(ini_errs* errs, ini_line_errs linerrs, size_t lnum);

/** line is modified but allocation borrowed */
void ini__nextline(ini* ini, ini_line_errs* errs, char * line);
void ini__nextfile(ini* ini, ini_errs* errs, FILE* file);

/** return is nullable; the returned pointer lives as long as the ini */
ini_section* ini__find_sectionp(ini* ini, char const* const* path, size_t path_len);

/** return is nullable; the returned pointer lives as long as the ini; path is null terminated */
ini_section* ini__find_sectionpn(ini* ini, char const* const* path);

/** return is nullable; the returned pointer lives as long as the ini; propagates NULL of section */ 
const char * ini_section__find(ini_section* section, const char * search);

#endif 

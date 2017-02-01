#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

FILE *fmemopen(void *buf, size_t size, const char *mode);
char* readall(const char *fname, unsigned int *len);
const char* trim(const char *start, const char *end);
int tohex(char ch);
int todigit(char ch);
int tohex(char ch);
unsigned int get_hex(const char *start, const char *end, const char ** raw, bool *overflow);
unsigned int get_octal(const char *start, const char *end, const char ** raw, bool *overflow);
bool get_boolean(const char *start, const char *end, const char ** raw);
double get_fraction(const char *start, const char *end, const char ** raw);
const char* get_string(const char *start, const char *end, const char ** raw, int *len);
long get_integer(const char *start, const char *end,const  char** raw, bool *overflow, bool *unsigned_flag);
bool is_hex(const char *start, const char *end);
bool is_octal(const char *start, const char *end);

#ifdef __cplusplus
}
#endif
#endif
#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

char* trim(char *start, char *end);
unsigned int parse_hex(char *start, char *end, char ** raw, bool *overflow);
unsigned int parse_octal(char *start, char *end, char ** raw, bool *overflow);
bool parse_boolean(char *start, char *end, char ** raw);
double parse_float(char *start, char *end, char ** raw);
char* parse_str(char *start, char *end, char ** raw, int *len);
long parse_int(char *start, char *end, char** raw, bool *overflow, bool *unsigned_flag);
bool is_hex(char *start, char *end);
bool is_octal(char *start, char *end);

#ifdef __cplusplus
}
#endif
#endif
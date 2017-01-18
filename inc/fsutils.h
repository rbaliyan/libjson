#ifndef __FS_UTILS_H__
#define __FS_UTILS_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

#ifndef FMEM_OPEN_SUPPORT
FILE *fmemopen(void *buf, size_t size, const char *mode);
#endif
char* readall(const char *fname, unsigned int *len);

#ifdef __cplusplus
}
#endif
#endif
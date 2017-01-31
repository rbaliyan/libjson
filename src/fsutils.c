#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/mman.h>
#include "fsutils.h"

#ifndef FMEM_OPEN_SUPPORT
#define BUFFER_SIZE 1024
#define BUFFER_SIZE_EXTRA 512

/*
* fmem structure to hold the buffer and pointer for the in memeory stream
*/
struct fmem {
    size_t pos;
    size_t size;
    char *buffer;
    int dynamic;
};


/*
* Read function for the in memeory stream
*/
static int read_buffer(void *handler, char *buf, int size)
{
    struct fmem *mem = handler;
    size_t available = mem->size - mem->pos;

    /* Check if we have enough space available */
    if (size > available) {
        /* Truncate data, can not go beyond the buffer size */
        size = available;
    }
    /* Copy data in buffer */
    memcpy(buf, mem->buffer + mem->pos, sizeof(char) * size);
    mem->pos += size;
    
    return size;
}

/*
* Write function for the in memeory stream
*/
static int write_buffer(void *handler, const char *buf, int size)
{
    struct fmem *mem = handler;
    char *buffer = NULL;
    size_t available = mem->size - mem->pos;
    /* Check if we have enough space available */
    if (size > available) {
        if((buffer = realloc(mem->buffer, size + mem->pos + BUFFER_SIZE_EXTRA))){
            mem->buffer = buffer;
            mem->size = size + mem->pos + BUFFER_SIZE_EXTRA;
        } else {
            /* Truncate data , can not go beyond the buffer size*/
            size = available;
        }
    }
    /* Copy data in buffer */
    memcpy(mem->buffer + mem->pos, buf, sizeof(char) * size);
    mem->pos += size;

    return size;
}

/*
* Seek function for the in memeory stream
*/
static fpos_t seek_buffer(void *handler, fpos_t offset, int whence)
{
    size_t pos;
    struct fmem *mem = handler;

    switch (whence) {
        case SEEK_SET:
            if (offset >= 0) {
                pos = (size_t)offset;
            } else {
                pos = 0;
            }
            break;
        
        case SEEK_CUR: 
            /* Only move if offset is in limits */
            if (offset >= 0 || (size_t)(-offset) <= mem->pos) {
                pos = mem->pos + (size_t)offset;
            } else {
                pos = 0;
            }
            break;
        
        case SEEK_END: 
            pos = mem->size + (size_t)offset;
            break;
        default: 
            return -1;
    }

    /* Make sure to not reach beyond buffer size */
    if (pos > mem->size) {
        return -1;
    }

    /* Set current pointer */
    mem->pos = pos;
    return (fpos_t)pos;
}

/*
* Close function
*/
static int close_buffer(void *handler)
{
  free(handler);
  return 0;
}

/*
* fmemopen
* Simulate stream io on memory buffer
*/
FILE *fmemopen(void *buf, size_t size, const char *mode)
{
    if(!buf || size == 0){
         fprintf(stderr, "Null args\n");
         return NULL;
    }
    // This data is released on fclose.
    struct fmem* mem = (struct fmem *) malloc(sizeof(struct fmem));

    // Zero-out the structure.
    memset(mem, 0, sizeof(struct fmem));
    mem->size = size;
    mem->buffer = buf;

    if((strcmp(mode, "r") == 0)){
        return fropen(mem, read_buffer);
    } else if((strcmp(mode, "w") == 0)){
        return fwopen(mem, write_buffer);
    } else if((strcmp(mode, "a") == 0)){
        return fwopen(mem, write_buffer);
    } else if((strcmp(mode, "w+") == 0)||
              (strcmp(mode, "r+") == 0)||
              (strcmp(mode, "rw+") == 0)||
              (strcmp(mode, "rw") == 0)){
        return funopen(mem, read_buffer, write_buffer, seek_buffer, close_buffer);
    } else {
        fprintf(stderr, "Unkown mode :%s\n", mode);
    }  

    return NULL;
}

FILE *fdmemopen(char ***buffer, size_t **size)
{
    if(!buffer){
        fprintf(stderr, "Null args\n");
        return NULL;
    }
    char *buf = NULL;
    // This data is released on fclose.
    struct fmem* mem = (struct fmem *) malloc(sizeof(struct fmem));

    // Zero-out the structure.
    memset(mem, 0, sizeof(struct fmem));

    if((buf = malloc(BUFFER_SIZE))){
        mem->dynamic = 1;
        mem->size = BUFFER_SIZE;
        if(size)
            *size = &mem->size;
        *buffer = &mem->buffer; 
    } else {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }

    mem->buffer = buf;

    return fwopen(mem, write_buffer);
}

#endif

/*
* Open a file in read mode and read all data in buffer
*/
char* readall(const char *fname, unsigned int *len)
{
    int ret = -1;
    int fd, rlen;
    char *buffer;
    off_t size = 0;
    int index = 0;

    /* Check data */
    if(fname){
        /* Open File */
        if((fd = open(fname, O_RDONLY)) >= 0){
            /* Goto end of file */
            if((size = lseek(fd, 0, SEEK_END)) > 0){
                /* Retrun to beginning */
                lseek(fd, 0, SEEK_SET);

                /* File needs to be in memeory */
                /* Allocate buffer for file data */
                if((buffer = malloc(size + 1))){
                    
                    /* Read entire file in buffer */
                    for(index = 0; index < size; index += rlen){
                        /* Read data from file */
                        rlen = read(fd, &buffer[index], size - index);
                        if(rlen < 0){
                            /* Dara read error */
                            fprintf(stderr, "%s:%d>Failed to read file : %s", __func__, __LINE__, fname);
                            /* Free buffer */
                            free(buffer);
                            break;
                        }
                    }
                    
                    /* Check if all data was read, do not process incomplete data */
                    if(index == size ){  
                      /* Make sure to NULL terminate */
                      buffer[index] = '\0';  
                        if(len)
                            *len = (unsigned int)size;

                        return buffer;
                    } else {
                        /* Free Buffer */
                        free(buffer);
                    }
                } else {
                    /* Memory allocation failure */
                    fprintf(stderr, "%s:%d>Failed to allocate memeory", __func__, __LINE__);
                }
            } else {
                /* Seek Fail, might have holes */
                fprintf(stderr, "%s:%d>Failed to seek in file", __func__, __LINE__);
            }
        } else {   
            /* File open fail*/          
            fprintf(stderr, "%s:%d>Failed to open file : %s", __func__, __LINE__,fname);
        } 
    } else {
        /* Invalid parameters */
        fprintf(stderr, "%s:%d>Invalid params", __func__, __LINE__);
    }
    return NULL;
}

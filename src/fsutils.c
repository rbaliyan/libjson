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

#define BUFFER_SIZE 1024
#define BUFFER_SIZE_EXTRA 512


#define FMEM_DYNAMIC_MEM 0x01
#define FMEM_RONLY 0x02
#define FMEM_WONLY 0x04
#define FMEM_RW 0x04


#ifndef FUNOPEN_SUPPORT
typedef ssize_t (write_t)(void *cookie, const char *buf, size_t size);
typedef ssize_t (read_t)(void *cookie, char *buf, size_t size);
typedef off_t (seek_t)(void *cookie, off_t offset, int whence);
typedef int (close_t)(void *cookie);
typedef struct _io
{
    read_t  *read;
    write_t *write;
    seek_t  *seek;
    close_t *close;
} io_functions_t;

FILE* fopencookie(void* cookie, const char* mode, io_functions_t io);
#endif

#ifndef FUNOPEN_SUPPORT
FILE *funopen(void *cookie, read_t *readfn, write_t *writefn, 
              seek_t *seekfn, close_t *closefn);
#endif

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
* @brief Read function for the in memeory stream
* @param handler Handler for memory buffer
* @param buf Buffer to read data to
* @param size Size of buffer
* @return length of data read
*/
#ifndef FUNOPEN_SUPPORT
static ssize_t read_buffer(void *handler, char *buf, size_t size)
#else
static int read_buffer(void *handler, char *buf, int size)
#endif
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
* @brief write function for the in memeory stream
* @param handler Handler for memory buffer
* @param buf Buffer to write data to
* @param size Size of buffer
* @return length of data written
*/
#ifndef FUNOPEN_SUPPORT
static ssize_t write_buffer(void *handler, char *buf, size_t size)
#else
static int write_buffer(void *handler, const char *buf, int size)
#endif
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
* @brief Seek function for the in memeory stream
* @param handler Handler for memory buffer
* @param offset Offset to seek to
* @param whence Offset start
* @return offset from start
*/
static off_t seek_buffer(void *handler, off_t offset, int whence)
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
    return (off_t)pos;
}

/*
* @brief Close function
* @param handle Hanlder for memory stream
* @return 0
*/
static int close_buffer(void *handler)
{
  free(handler);
  return 0;
}

#ifndef FUNOPEN_SUPPORT
/*
* @brief
* @param cookie memeory stream handler
* @param readfn Read function
* @param writefn write function
* @param seekfn seek function
* @param readfn closefn function
* @return File pointer
*/
FILE *funopen(void *cookie, read_t *readfn, write_t *writefn, 
              seek_t *seekfn, close_t *closefn)
{
  io_functions_t io = { NULL };
  io.read = readfn;
  io.write = writefn;
  io.seek = seekfn;
  io.close = closefn;
  const char* mode = NULL;
  if(readfn && writefn){
      mode = "rw";
  } else if(readfn){
      mode = "r";
  } else {
      mode = "w";
  }

  return fopencookie (cookie, mode, io);
}
#endif

#ifndef FMEMOPEN_SUPPORT
/*
* @brief Simulate stream io on memory buffer
* @param buf Buffer for stream
* @param size Size of buffer
* @param mode IO Mode["r", "w", "rw"]
* @return File pointer
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

    return funopen(mem, read_buffer, write_buffer, seek_buffer, close_buffer);
}
#endif
/*
* @brief Simulate stream io on dynamically growing memory buffer
* @param buf where pointer to allocated buffer will be stored
* @param size Size of buffer
* @return File pointer
*/
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
        free(mem);
        return NULL;
    }

    mem->buffer = buf;

    return funopen(mem, read_buffer, write_buffer, seek_buffer, close_buffer);
}

/*
* @brief Open a file in read mode and read all data in buffer
* @param fname filename 
* @param len Pointer where length of read data will be saved
* @return Buffer where data is read
*/
char* readall(const char *fname, unsigned int *len)
{
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


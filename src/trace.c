#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdarg.h>
#include <ctype.h>


#define MODULE "Trace"
#include "trace.h"

static int current_trace_level = _TRACE_DUMP_LEVEL_;
static FILE* debug_stream = NULL;

/*
* @brief Hexdump binary data
* @param level debugging level
* @param str Tag message
* @param data Binary data
* @param len length of data
* @return Length of data printed
*/
int hexdump(int level, char * str, unsigned int* data, size_t size)
{
    if(!debug_stream)
        debug_stream = stdout;

    if(level > current_trace_level)
        return 0;
    int i = 0;
    int index  = 0;
    int len = 0;

    char msg[18] = {0};

    if(str)
        len +=fprintf(debug_stream, "%s\n", str);
    /*Print data*/    
    for(i = 0; i < size; i++){
        if((i)&&(i % 8 == 0))
           len +=fprintf(debug_stream, " ");
        if(i && i % 16 == 0){
            index = 0;
            len += fprintf(debug_stream, "%s\n", msg);
        }
        if(isgraph(data[i]) || data[i] == ' ')
            msg[index++]=data[i];
        else
            msg[index++]='.';
        msg[index]=0;
        len += fprintf(debug_stream, "%02x ", data[i]);
    }

    if(index){
        while(index--)len +=fprintf(debug_stream, " ");
        len += fprintf(debug_stream, "%s\n", msg);
    }
    len += fprintf(debug_stream, "\n");

    return len;
}

/*
* @brief Dump log to screen
* @param level debugging level
* @param format string format
* @return length of bytes printed
*/
int dump_log(int level, const char *format, ...)
{
    if(!debug_stream)
        debug_stream = stdout;
    int n = 0;
    if(level<=current_trace_level){
        va_list args;
        va_start(args, format);
        n = vfprintf(debug_stream, format, args);
        va_end(args);
    }
    
    return n;
}

/*
* @brief Set Trace leve;
* @param level debugging level
* @return current trace level
*/
int set_trace_level( int level )
{
    return (current_trace_level = level);
}

/*
* @brief Set file for debug logs
* @param fname filename for output
* @return 0 for success
*/
int set_trace_file(const char *fname)
{
    if((debug_stream != stdout) && (debug_stream != stderr)){
        fclose(debug_stream);
    }

    if(fname){
        if((strncasecmp(fname,"stdout", 6) ==0)){
            debug_stream = stdout;
        } else if((strncasecmp(fname,"stderr", 6) ==0)){
            debug_stream = stderr;
        }

        FILE *fp = fopen(fname,"a");
        debug_stream = fp;
        if(!fp)
            return -1;
    
    } else {
        debug_stream = stdout;
    }

    return 0;
}
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <ctype.h>


#define MODULE "Trace"
#include "trace.h"

static int current_trace_level = _TRACE_DUMP_LEVEL_;


void hexdump(int level, char * str, unsigned int* data, int len)
{
    if(level > current_trace_level)
        return;
    int i = 0;
    int index  = 0;

    char msg[18] = {0};
    if(str)
        printf("%s\n", str);
        
    for(i = 0;i < len; i++){
        if((i)&&(i % 8 == 0))
            putchar(' ');
        if(i && i % 16 == 0){
            index = 0;
            printf("%s\n", msg);
        }
        if(isgraph(data[i]) || data[i] == ' ')
            msg[index++]=data[i];
        else
            msg[index++]='.';
        msg[index]=0;
        printf("%02x ", data[i]);
    }

    if(index){
        while(index--)putchar(' ');
        printf("%s\n", msg);
    }
    printf("\n");
}

int dump_log(int level, const char *format, ...)
{
    int n = 0;
    if(level<=current_trace_level){
        va_list args;
        va_start(args, format);
        n = vprintf(format, args);
        va_end(args);
    }
    
    return n;
}


int set_trace_level( int level )
{
    return (current_trace_level = level);
}
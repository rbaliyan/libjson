#ifndef _TRACE_H_
#define _TRACE_H_

#include <errno.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TRACE_LEVEL_NONE 0
#define TRACE_LEVEL_ERROR 1
#define TRACE_LEVEL_WARN 2
#define TRACE_LEVEL_INFO 3
#define TRACE_LEVEL_DEBUG 4
#define TRACE_LEVEL_ALL  TRACE_LEVEL_DEBUG

#ifndef _TRACE_LEVEL_
#define _TRACE_LEVEL_   TRACE_LEVEL_DEBUG
#endif

#ifndef _TRACE_DUMP_LEVEL_
#define _TRACE_DUMP_LEVEL_ TRACE_LEVEL_DEBUG
#endif

#ifndef MODULE
#define MODULE __FILE__
#endif

#define _PRINT(L,args,...)   dump_log(L, args ,##__VA_ARGS__ )
#define _TRACE(P,L,args,...)    dump_log(P, #L "!!!" MODULE ":%s>[%d]" args "\n" ,__func__,__LINE__,##__VA_ARGS__ )
#define _HEXDUMP(L,str, ptr, len)    hexdump(L, str, ptr, len)

#define TRACE(L,args,...)   TRACE_##L(L, args,##__VA_ARGS__)
#define PRINT(L,args,...)   PRINT_##L(args,##__VA_ARGS__)
#define HEXDUMP(L,str, ptr, len)   HEXDUMP_##L(str, ptr, len)

#if _TRACE_LEVEL_ == TRACE_LEVEL_ERROR

#define TRACE_ERROR(L, args,...) _TRACE(TRACE_LEVEL_ERROR,L,args,##__VA_ARGS__)
#define PRINT_ERROR(args,...) _PRINT(TRACE_LEVEL_ERROR,args,##__VA_ARGS__)
#define HEXDUMP_ERROR(str, ptr, len) _HEXDUMP(TRACE_LEVEL_ERROR,args,##__VA_ARGS__)

#elif _TRACE_LEVEL_ == TRACE_LEVEL_WARN

#define TRACE_ERROR(L,args,...) _TRACE(TRACE_LEVEL_ERROR,L,args,##__VA_ARGS__)
#define TRACE_WARN(L,args,...)  _TRACE(TRACE_LEVEL_WARN,L,args,##__VA_ARGS__)

#define PRINT_ERROR(args,...) _PRINT(TRACE_LEVEL_ERROR,args,##__VA_ARGS__)
#define PRINT_WARN(args,...)  _PRINT(TRACE_LEVEL_WARN,args,##__VA_ARGS__)

#define HEXDUMP_ERROR(str, ptr, len)) _HEXDUMP(TRACE_LEVEL_ERROR,str, ptr, len)
#define HEXDUMP_WARN(str, ptr, len))  _HEXDUMP(TRACE_LEVEL_WARN,str, ptr, len)

#elif _TRACE_LEVEL_ == TRACE_LEVEL_INFO

#define TRACE_ERROR(L,args,...) _TRACE(TRACE_LEVEL_ERROR,L,args,##__VA_ARGS__)
#define TRACE_WARN(L,args,...)  _TRACE(TRACE_LEVEL_WARN,L,args,##__VA_ARGS__)
#define TRACE_INFO(L,args,...)  _TRACE(TRACE_LEVEL_INFO,L,args,##__VA_ARGS__)

#define PRINT_ERROR(args,...) _PRINT(TRACE_LEVEL_ERROR,args,##__VA_ARGS__)
#define PRINT_WARN(args,...)  _PRINT(TRACE_LEVEL_WARN,args,##__VA_ARGS__)
#define PRINT_INFO(args,...)  _PRINT(TRACE_LEVEL_INFO,args,##__VA_ARGS__)

#define HEXDUMP_ERROR(str, ptr, len) _HEXDUMP(TRACE_LEVEL_ERROR,str, ptr, len)
#define HEXDUMP_WARN(str, ptr, len)  _HEXDUMP(TRACE_LEVEL_WARN,str, ptr, len)
#define HEXDUMP_INFO(str, ptr, len)  _HEXDUMP(TRACE_LEVEL_INFO,str, ptr, len)


#elif _TRACE_LEVEL_ == TRACE_LEVEL_DEBUG 
#define TRACE_ERROR(L,args,...) _TRACE(TRACE_LEVEL_ERROR,L,args,##__VA_ARGS__)
#define TRACE_WARN(L,args,...)  _TRACE(TRACE_LEVEL_WARN,L,args,##__VA_ARGS__)
#define TRACE_INFO(L,args,...)  _TRACE(TRACE_LEVEL_INFO,L,args,##__VA_ARGS__)
#define TRACE_DEBUG(L,args,...) _TRACE(TRACE_LEVEL_DEBUG,L,args,##__VA_ARGS__)


#define PRINT_ERROR(args,...) _PRINT(TRACE_LEVEL_ERROR,args,##__VA_ARGS__)
#define PRINT_WARN(args,...)  _PRINT(TRACE_LEVEL_WARN,args,##__VA_ARGS__)
#define PRINT_INFO(args,...)  _PRINT(TRACE_LEVEL_INFO,args,##__VA_ARGS__)
#define PRINT_DEBUG(args,...) _PRINT(TRACE_LEVEL_DEBUG,args,##__VA_ARGS__)

#define HEXDUMP_ERROR(str, ptr, len) _HEXDUMP(TRACE_LEVEL_ERROR,str, ptr, len)
#define HEXDUMP_WARN(str, ptr, len)  _HEXDUMP(TRACE_LEVEL_WARN,str, ptr, len)
#define HEXDUMP_INFO(str, ptr, len)  _HEXDUMP(TRACE_LEVEL_INFO,str, ptr, len)
#define HEXDUMP_DEBUG(str, ptr, len) _HEXDUMP(TRACE_LEVEL_DEBUG,str, ptr, len)


#endif


#ifndef TRACE_ERROR
#define TRACE_ERROR(L,args,...)
#endif

#ifndef TRACE_WARN
#define TRACE_WARN(L,args,...)  
#endif

#ifndef TRACE_INFO
#define TRACE_INFO(L,args,...) 
#endif

#ifndef TRACE_DEBUG
#define TRACE_DEBUG(L,args,...)
#endif


#ifndef PRINT_ERROR
#define PRINT_ERROR(L,args,...)
#endif

#ifndef PRINT_WARN
#define PRINT_WARN(L,args,...)  
#endif

#ifndef PRINT_INFO
#define PRINT_INFO(L,args,...)  
#endif

#ifndef PRINT_DEBUG
#define PRINT_DEBUG(L,args,...)
#endif

#ifndef HEXDUMP_ERROR
#define HEXDUMP_ERROR(L,args,...)
#endif

#ifndef HEXDUMP_WARN
#define HEXDUMP_WARN(L,args,...)  
#endif

#ifndef HEXDUMP_INFO
#define HEXDUMP_INFO(L,args,...)  
#endif

#ifndef HEXDUMP_DEBUG
#define HEXDUMP_DEBUG(L,args,...)
#endif
  

int hexdump(int level, char * str, unsigned int* data, size_t size);
int set_trace_level( int level );
int dump_log(int level, const char *format, ...);
int set_trace_file(const char *fname);
#ifdef __cplusplus
}
#endif
#endif
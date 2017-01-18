#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/mman.h>
#include "utils.h"


/**
* The same function can be used in reverse also, by reversing arguments
* just skip all spaces
*/
const char* trim(const char *start, const char *end)
{
    /* Check for valid ptrs */
    if(start && end){
        /* If length is negative then trim in reverse */
        if(start >  end){
            /* trim from the end */
            /* Skip all space, including null characters */
            for(; (start > end) && (isspace(*start) || !*start) ; start--);

        } else {
            /* trim from beginning */
            /* Skip all space and stop at null character or any other valid characters*/
            for(; (start < end) && (!*start || isspace(*start)); start++);
        }
    }
     /* Return the trimmed ptr, start and end will be handled by caller */
    return start;
}

/* Check is given number is in hexa decimal format*/
bool is_hex(const char *start, const char *end)
{
    /* Check data*/
    if(start && end && (start < end)){
      /* Hex starts with 0x or 0X*/
        if((*start == '0') && ((end - start) > 2) && 
        ((*(start + 1) == 'x') || (*(start + 1) == 'X')))
          return true;
    } else {
        fprintf(stderr,"%s:%d>Null data", __func__, __LINE__);
    }
    return false;
}

/* Check if given number is in octal format*/
bool is_octal(const char *start, const char *end)
{
    /*check for data*/
    if(start && end && (start < end)){
        /* octal starts with 0 and some digit*/
        if((*start == '0')&& (start + 1 < end) && isdigit(*(start + 1)))
          return true;
    } else {
        fprintf(stderr,"%s:%d>Null data", __func__, __LINE__);
    }
    return false;
}

/* Convert characters to hex digit,
* For invalid hex return -1 
*/
int tohex(char ch)
{
    int val = -1;
    /* Valid Hex values should be between 
    *'A' and 'F'
    *'a' and 'f'
    *'0' and '9', including 
    */
    if((ch >= 'a' ) && ( ch <='f')){
        val = ch - 'a' + 10;
    } else if((ch >= 'A' ) && ( ch <='F')){
        val = ch - 'A' + 10;
    } else if((ch >= '0' ) && ( ch <='9')){
        val = ch - '0';
    }

    return val;
}

/* Convert character to digit , return -1 for invalid value */
int todigit(char ch)
{
    int val = -1;
    /* Valid digit should be between 
    * '0' and '9' including
    */
    if((ch >= '0' ) && ( ch <='9')){
        val = ch - '0';
    }

    return val;
}

/*
* Parse string representation of hex value
* For overflow number is trancated
*/
unsigned int get_hex(const char *start, const char *end, const char **raw,bool *overflow)
{
    const char *begin = start;
    unsigned int number = 0;
    unsigned int count = 0;
    int hex_max = sizeof(number) * 2;
    int val = 0;
    bool is_overflow = false;
    unsigned int number_truncated = 0;

    /* Check Data */
    if( start && end && raw && (start < end)){
        /* Traverse string */
        for(*raw = start; *start && start < end; *raw = ++start){
            
            count++;
            /* size of byte is 2 nibble */
            if(count > hex_max){
                /* Overflow */
                fprintf(stderr, "Maximum hex length %d\n",hex_max);
                is_overflow = true;
                number_truncated = number;
                if(overflow)
                    *overflow = true;
            }

            /* Convert to Hex a-f, A-F, 0-9*/
            val = tohex(*start);

            if(val == -1){
                if(count == 1){
                /* No Hex Digit Found */
                    fprintf(stderr, "Invalid Hex digit %c\n", *start);
                    /* Nothing Parsed */
                    *raw = begin;
                    return 0;
                }
                break;
            }

            number =  (number<<4) | val;
        }
    } else {
        fprintf(stderr, "%s:%d>Null data\n",__func__, __LINE__);
    }

    return (is_overflow == false) ? number : number_truncated;
}

/* Read octal in string and return integer
* for any error set raw ptr to start to indicate
* nothing was parsed 
*/
unsigned int get_octal(const char *start, const char *end,const  char **raw, bool *overflow)
{
    const char *begin = start;
    unsigned int number = 0;
    int octal_max = sizeof(number) * 8 / 3;
    unsigned int count = 0;
    int val = 0;
    bool is_overflow = false;
    unsigned int number_truncated = 0;

    if( start && end && raw && (start < end)){
        /* Traverse all string */
        for(*raw = start; *start && start < end; *raw = ++start){
            count++;
            /* size of byte is 2 nibble */
            if(count > octal_max){
                /* Overflow */
                fprintf(stderr, "Maximum length : %d", octal_max);
                is_overflow = true;
                number_truncated = number;
                if(overflow)
                    *overflow = true;
                  
            }
            /* Convert to octal 0-7*/
            val = todigit(*start);
            if((val == -1) || (val > 7)){
                if(count == 1){
                /* No Hex Digit Found */
                    fprintf(stderr, "Invalid octal digit %c\n", *start);
                    /* Nothing Parsed */
                    *raw = begin;
                    return 0;
                }
                break;
            }
            
            number =  (number << 3) | val;
        }
    } else {
        fprintf(stderr, "%s:%d>Null data\n",__func__, __LINE__);
    }

    return (is_overflow == false) ? number : number_truncated;
}

/* 
* Read fraction in string and return double
* for any error set raw ptr to start to indicate
* nothing was parsed 
*/
double get_fraction(const char *start, const char *end, const  char **raw)
{
    const char *begin = start;
    double number = 0;
    double multiple = 1.0f;
    int val = 0;
    int count = 0;

    /* Check data*/
    if( start && end && raw && (start < end)){
        /* Traverse all string */
        for(*raw = start; *start && start < end; *raw = ++start){
            count++;

            /* Convert to digit */
            val = todigit(*start);
            if(val == -1 ){
                if(count == 1){
                    fprintf(stderr,"Failed to parse, nothing after decimal\n");
                    /* Nothing Parsed */
                    *raw = begin;
                    return 0;
                }
                break;
            }

            multiple *= 0.1f;
            number = number + val * multiple;     
        }

        /* Float */
        if((*raw < end)&& (**raw == 'f' || **raw == 'F')){
            *raw = (*raw) + 1;
        }
    } else {
        fprintf(stderr, "%s:%d>Null data\n",__func__, __LINE__);
    }

    return number;
}

/*
* Parse boolean values
*/
bool get_boolean(const char *start, const char *end,const  char** raw)
{
    const char *begin = start;
    int len = end - start;
    int tlen = 4;
    int flen = 5;
    if(start && end && raw && (len > 0)){
        /* Nothing Parsed */
        *raw = start;

        if((len >= tlen) || (len >= flen)){
            if(strncmp(start, "true", 4) == 0){
                *raw = trim(start + 4, end);
                return true;
            } else if(strncmp(start, "false", 5) == 0){
                *raw = trim(start + 5, end);
                return false;
            }
        }
    }

    return false;
}

/*
* Parse string in double quotes, 
* string may also contain double quotes after escaping with backslash
* double quotes are changed to Null characters
*/
const char *get_string(const char *start, const char *end,const  char** raw, int *len)
{
    const char *begin = start;
    bool escape_on = false;
    const char *str_start = NULL;
    char *buffer = NULL;

    if(start && end && raw && (start < end)){

        /* First characters should be quotes */
        if(*start != '"')
            return NULL;

        /* Set it to NULL */
        start++;
        str_start = start;

        /* For now we allow line break */
        for(*raw = begin;  *start && (start < end); *raw = ++start){
            /* Escaped */
            if(*start == '/'){
                /* Esacpe Sign, can be used to embed double quotes */
                escape_on = !escape_on;
            } else if((*start == '"' ) &&( !escape_on )){
                *raw = start + 1;
                buffer = malloc(start - str_start + 1);
                if(buffer){
                    memcpy(buffer, str_start, start - str_start );
                    buffer[start - str_start] = '\0';
                    if(len)
                      *len = start - str_start;
                    return (const char*)buffer;
                }
                break;
            } else {
                /* Escape is only applied to next one character */
                escape_on = false;
            }
        }
    } else {
        fprintf(stderr,"Invalid args");
    }

    /* Nothing Parsed */
    *raw = start;
    return NULL;
}

/* Parse long number representation, its is also used to parse the iteger part of deouble number*/
long get_integer(const char *start, const char *end,const  char** raw, bool *overflow, bool *unsigned_flag)
{
    long number = 0;
    long old = 0;
    long number_truncated = 0;
    int count = 0;
    bool is_overflow = false;
    int val = 0;
    const char *begin = start;
    /* Check data */
    if(start && end && raw && (start < end)){
        /* Traverse all data */
        for(; *start && start < end; *raw = ++start){
            if((*start == 'u') || (*start == 'U')){
                if(unsigned_flag)
                    *unsigned_flag = true;
                /* check if found any digit*/
                if(count == 0){
                    fprintf(stderr,"%s:%d>Number missing", __func__, __LINE__);
                    *raw = begin;
                    return 0;
                }
                /* Rest of data to process*/
                *raw = trim(start+1, end);
                break;
            }else if((*start == 'l') || (*start == 'L')){
                /* check if found any digit*/
                if(count == 0){
                    fprintf(stderr,"%s:%d>Number missing", __func__, __LINE__);
                    *raw = begin;
                    return 0;
                }
                /* Rest of data to process*/
                *raw = trim(start+1, end);
                break;
            } else if((val = todigit(*start)) >= 0) {
                count++;
                old = number; 
                number = number * 10 + val;
                if(( number < 0 ) && (is_overflow == false)){
                    is_overflow = true;
                    number_truncated = old;
                    fprintf(stderr,"%s:%d>Overflow in integer max value:%lu", __func__, __LINE__, LONG_MAX);
                    if(*overflow)
                        *overflow = true;
                } 
            } else if (count == 0){
                fprintf(stderr,"%s:%d>Number missing", __func__, __LINE__);
                *raw = begin;
                return 0;
            } else {
                /* Rest of data to process*/
                *raw = trim(start, end);
                break;
            }
        }
    } else {
        fprintf(stderr,"%s:%d>Null data", __func__, __LINE__);
    }
    return (is_overflow == false) ? number : number_truncated;
}

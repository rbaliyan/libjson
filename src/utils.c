#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#ifdef DARWIN_SYS
#include <sys/mman.h>
#endif
#include "utils.h"

static int tohex(char ch);
static int todigit(char ch);
static int tohex(char ch);


/*
* @brief Convert character to hex
* @param ch character
* @return hex value or -1 for invalid data
*/
static int tohex(char ch)
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

/*
* @brief Convert character to digit
* @param ch character
* @return digit value or -1 for invalid data
*/
static int todigit(char ch)
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
* @brief skip spaces in buffer 
* The same function can be used in reverse also, by reversing arguments
* @param start start of buffer
* @param end end of buffer
* @return rest of buffer after removing space
*/
char* trim(char *start, char *end)
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

/*
* @brief Check if given number is in hexa decimal format
* @param start start of buffer
* @param end end of buffer
* @return true if string representation is in hex
*/
bool is_hex(char *start, char *end)
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

/*
* @brief Check if given number is in octal decimal format
* @param start start of buffer
* @param end end of buffer
* @return true if string representation is in octal
*/
bool is_octal(char *start, char *end)
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

/*
* @brief Convert string to hex number
* @param start start of buffer
* @param end end of buffer
* @param raw rest of data after parsing
* @param overflow overflow occured and data got trucnated
* @return integer
*/
unsigned int parse_hex(char *start, char *end, char **raw,bool *overflow)
{
    char *begin = start;
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

/*
* @brief Convert string to octal number
* @param start start of buffer
* @param end end of buffer
* @param raw rest of data after parsing
* @param overflow overflow occured and data got trucnated
* @return integer
*/
unsigned int parse_octal(char *start, char *end, char **raw, bool *overflow)
{
    char *begin = start;
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
* @brief Convert string to fractional part of number
* @param start start of buffer
* @param end end of buffer
* @param raw rest of data after parsing
* @return fractional value
*/
double parse_float(char *start, char *end,  char **raw)
{
    char *begin = start;
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
* @brief Convert string to boolean
* @param start start of buffer
* @param end end of buffer
* @param raw rest of data after parsing
* @return boolean
*/
bool parse_boolean(char *start, char *end, char** raw)
{
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
* @brief parse quoted string
* @param start start of buffer
* @param end end of buffer
* @param raw rest of data after parsing
* @param len length of string
* @return pointer to string 
*/
char *parse_str(char *start, char *end, char** raw, int *len)
{
    char *begin = start;
    bool escape_on = false;
    char *str_start = NULL;

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
                *start = '\0';
                return str_start;
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

/*
* @brief Convert string to long number
* @param start start of buffer
* @param end end of buffer
* @param raw rest of data after parsing
* @param overflow overflow occured and data got trucnated
* @param unsigned_flag if number if unsigned
* @return long number
*/
long parse_int(char *start, char *end, char** raw, bool *overflow, bool *unsigned_flag)
{
    long number = 0;
    long old = 0;
    long number_truncated = 0;
    int count = 0;
    bool is_overflow = false;
    int val = 0;
    char *begin = start;
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

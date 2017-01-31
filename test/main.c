#include <stdio.h>
#include "json.h"


int main(int argc, char *argv[])
{
    int len = 0;
    char buffer[1024] = {0};
    char *buff_new = NULL;
    struct json *json = NULL;
    json = json_load("test.json", NULL);
    struct json *json_1 = NULL;
    long n = 1234;
    json_1 = json_new();
    json_set(json_1, "int", JSON_TYPE_INT, &n);
    json_set(json_1, "str", JSON_TYPE_STR, "str value");
    json_set(json, "json", JSON_TYPE_OBJ, json_1);
    if(json){
        len = json_prints(json, buffer, sizeof(buffer), 2);
        printf("\n%s\n", buffer);
        json_set(json, "json", 0, NULL);
        json_print(json, 2);
        buff_new = json_str(json, &len, 0);
        printf("\n%s\n", buff_new);
        free(buff_new);
        json_del(json);
    }
}


#include <stdio.h>
#include "json.h"


int main(int argc, char *argv[])
{
    int len = 0;
    char buffer[1024] = {0};
    struct json *json = NULL;
    json_load("test.json", &json);
    if(json){
        len = json_prints(json, buffer, sizeof(buffer), 4);
        printf("%s", buffer);

    }
}


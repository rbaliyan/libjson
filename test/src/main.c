#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdlib.h>
#include "json.h"
#include "list.h"
#include "dict.h"
#include "iter.h"
#include "test.h"

#define MODULE "Main"
#include "trace.h"


int main(int argc, char *argv[])
{
    test_list_run();
    test_dict_run();
    test_json_run();
    test_iter_run();
}



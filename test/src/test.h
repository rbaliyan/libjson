#ifndef _TEST_H_
#define _TEST_H_

#define TEST_SUITE_INIT(x)      int _total_test_count_ = 0, _failed_test_count_ = 0; char* _test_suite_name_=x;
#define TEST_SUITE_BEGIN()     printf("%s",_test_suite_name_);
                                    
#define TEST_RUN(x,y)           {\
                                    _total_test_count_++;\
                                    if(x()){\
                                        printf("\nTest %-20s : Pass\n", y);\
                                    } else {\
                                        _failed_test_count_++;\
                                         printf("\nTest %-20s : Failed\n", y);\
                                    }\
                                }
#define TEST_SUITE_RESULTS()   {\
                                    printf("======================\n");\
                                    printf("%s\n", _test_suite_name_);\
                                    printf("Total Tests     : %d\n", _total_test_count_ - _failed_test_count_);\
                                    printf("Pass Tests      : %d\n", _total_test_count_);\
                                    printf("Failed Tests    : %d\n", _failed_test_count_);\
                                    printf("======================\n");\
                                }

extern int test_list_run(void);
extern int test_dict_run(void);
extern int test_json_run(void);
extern int test_iter_run(void);
#endif
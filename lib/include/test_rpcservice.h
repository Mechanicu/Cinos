#define MAX_TEST_FILE_NAME_LEN 64


typedef struct test_rpcservice_params {
    char file_name[MAX_TEST_FILE_NAME_LEN];
    unsigned long str_len;
    char file_content[0];
} test_rpcservice_params_t;
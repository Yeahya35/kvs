typedef enum 
{
    PUT,
    DEL,
    GET,
    DUMP,
    QUIT,
    QUITSERVER
} request_type;


typedef struct
{
    request_type type;
    long key;
    char *value;
} request_t;
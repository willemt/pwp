
typedef struct
{
    int pos;
    char *data;
    unsigned char last_send_data[128];
    void *piece;
    char infohash[21];
} test_sender_t;

typedef struct
{
    int pos;
    unsigned char *data;
    int has_disconnected;

    void *piece;

    bt_block_t last_block;
    unsigned char last_block_data[10];
    char infohash[21];
} test_reader_t;


int __FUNC_peercon_recv( void* r, void * peer __attribute__((__unused__)), char *buf, int *len);

void *__FUNC_reader_get_piece( void * r, const int idx __attribute__((__unused__)));

int __FUNC_disconnect( void* r, void * peer __attribute__((__unused__)), char *reason __attribute__((__unused__)));

int __FUNC_MOCK_push_block( void * reader __attribute__((__unused__)), void * peer  __attribute__((__unused__)), bt_block_t * block __attribute__((__unused__)), void *data __attribute__((__unused__)));

int __FUNC_push_block( void* r, void * peer __attribute__((__unused__)), bt_block_t * block, void *data);

void *__reader_set( test_reader_t * reader, unsigned char *msg);

unsigned char *__sender_set( test_sender_t * sender);

int __FUNC_MOCK_send( void* s __attribute__((__unused__)), const void *peer __attribute__((__unused__)), const void *send_data __attribute__((__unused__)), const int len __attribute__((__unused__)));

int __FUNC_send( void* s, const void *peer __attribute__((__unused__)), const void *send_data, const int len);

int __FUNC_failing_send( void * sender __attribute__((__unused__)), const void *peer __attribute__((__unused__)), const void *send_data __attribute__((__unused__)), const int len __attribute__((__unused__)));

void *__FUNC_sender_get_piece( void* s, const int idx __attribute__((__unused__)));

int __FUNC_pieceiscomplete( void *bto __attribute__((__unused__)), void *piece __attribute__((__unused__)));

int __FUNC_pieceiscomplete_fail( void *bto __attribute__((__unused__)), void *piece __attribute__((__unused__)));


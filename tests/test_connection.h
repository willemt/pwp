#ifndef TEST_CONNECTION_H
#define TEST_CONNECTION_H

typedef struct
{
    /* send */
    /* number of sent messages */
    int nsent_messages;
    /* position we are on our send buffer */
    int send_pos;
    /* send buffer */
    char *send_data;

    /* read */
    int read_pos;
    char *read_data;

    bt_block_t read_last_block;
    char read_last_block_data[10];

    void *piece;
    char infohash[21];

    int has_disconnected;

    void* sc;

} test_sender_t;

int __FUNC_connect(
    /* caller */
    void* r __attribute__((__unused__)), 
    /* me */
    void* me __attribute__((__unused__)),
    void * peer __attribute__((__unused__)));

int __FUNC_peercon_recv( void* r,
        void * peer __attribute__((__unused__)),
        char *buf,
        int *len);

void *__FUNC_reader_get_piece( void * r,
        const int idx __attribute__((__unused__)));

int __FUNC_disconnect( void* r,
        void * peer __attribute__((__unused__)),
        char *reason __attribute__((__unused__)));

int __FUNC_MOCK_push_block( void * reader __attribute__((__unused__)),
        void * peer  __attribute__((__unused__)),
        bt_block_t * block __attribute__((__unused__)),
        const void *data __attribute__((__unused__)));

int __FUNC_push_block( void* r,
        void * peer __attribute__((__unused__)),
        bt_block_t * block,
        const void *data);

char *__sender_set(
    test_sender_t * sender,
    char *read_msg,
    char *send_msg);


int __FUNC_MOCK_send( void* s __attribute__((__unused__)),
        const void *peer __attribute__((__unused__)),
        const void *send_data __attribute__((__unused__)),
        const int len __attribute__((__unused__)));

void* __FUNC_get_piece_never_have(
    void* s __attribute__((__unused__)),
    const unsigned int idx __attribute__((__unused__)));

int __FUNC_send( void* s,
        const void *peer __attribute__((__unused__)),
        const void *send_data, const int len);

int __FUNC_failing_send(
        void * sender __attribute__((__unused__)),
        const void *peer __attribute__((__unused__)),
        const void *send_data __attribute__((__unused__)),
        const int len __attribute__((__unused__)));

void *__FUNC_sender_get_piece(
        void* s,
        const unsigned int idx __attribute__((__unused__)));

int __FUNC_pieceiscomplete(
        void *bto __attribute__((__unused__)),
        void *piece __attribute__((__unused__)));

int __FUNC_pieceiscomplete_fail(
        void *bto __attribute__((__unused__)),
        void *piece __attribute__((__unused__)));

void __FUNC_piece_write_block_to_stream(
    void * me,
    bt_block_t * blk,
    char ** msg);

void __FUNC_peer_piece_have(
    void *udata,
    void *peer __attribute__((__unused__)),
    int piece);


#endif /* TEST_CONNECTION_H */

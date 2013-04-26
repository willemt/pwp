typedef void *(*func_getpiece_f)( void *udata, int piece);

typedef void (*func_write_block_to_stream_f)(
    void* pce, bt_block_t * blk, unsigned char ** msg);


typedef struct {
    /* write piece block data to stream when we need to send blocks */
    func_write_block_to_stream_f func_write_block_to_stream;

    /* determine if the piece is complete */
    func_get_int_f func_piece_is_complete;

    /* get piece */
    func_getpiece_f func_getpiece;
    void* caller;
} bt_piece_i;

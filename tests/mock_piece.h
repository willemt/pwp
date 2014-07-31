#ifndef MOCK_PIECE_H
#define MOCK_PIECE_H

void *mock_piece_new(const char *sha1sum, const int piece_bytes_size);

void mock_piece_set_disk_blockrw( void * pce, bt_blockrw_i * irw, void *udata);

int mock_piece_write_block( void *me, void *caller __attribute__((__unused__)), const bt_block_t * blk, const void *blkdata);

void mock_piece_write_block_to_stream(
    void * me,
    bt_block_t * blk,
    char ** msg
);

#endif /* MOCK_PIECE_H */

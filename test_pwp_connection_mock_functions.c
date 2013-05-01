#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include <stdbool.h>
#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "CuTest.h"

#include "pwp_connection.h"
#include "bitstream.h"
#include "bt_block_readwriter_i.h"
#include "bt_diskmem.h"
#include "mock_piece.h"
#include "test_pwp_connection.h"

int __FUNC_peercon_recv(
    void* r,
    void * peer __attribute__((__unused__)),
    char *buf,
    int *len
)
{
    test_reader_t * reader = r;

    memcpy(buf, &reader->data[reader->pos], sizeof(char) * (*len));
//    printf("%d\n", reader->pos);
    reader->pos += *len;

#if 0
    printf("read:");
    int ii;

    for (ii = 0; ii < *len; ii++)
    {
        printf("%d ", buf[ii]);
    }
    printf("\n");
#endif

    return 1;
}

void *__FUNC_reader_get_piece(
    void * r,
    const int idx __attribute__((__unused__))
)
{
    test_reader_t * reader = r;
    void *piece;

    piece = reader->piece;
    return piece;
}

/*----------------------------------------------------------------------------*/

int __FUNC_disconnect(
    void* r,
    void * peer __attribute__((__unused__)),
    char *reason __attribute__((__unused__))
)
{
    test_reader_t * reader = r;
    reader->has_disconnected = 1;
    return 0;
}

int __FUNC_MOCK_push_block(
    void * reader __attribute__((__unused__)), 
    void * peer  __attribute__((__unused__)),
    bt_block_t * block __attribute__((__unused__)),
    void *data __attribute__((__unused__))
)
{
    return 1;
}

/*  allocate the block */
int __FUNC_push_block(
        void* r,
    void * peer __attribute__((__unused__)),
    bt_block_t * block,
    void *data
)
{
    test_reader_t * reader = r;

    memcpy(&reader->last_block, block, sizeof(bt_block_t));
    assert(block->block_len < 10);
    memcpy(reader->last_block_data, data, sizeof(char) * block->block_len);
    return 1;
}

void *__reader_set(
    test_reader_t * reader,
    unsigned char *msg
)
{
    /*  add the piece db to reader */
    bt_block_t blk;
    char piecedata[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    void *dc;

    reader->pos = 0;
    reader->data = msg;
    reader->has_disconnected = 0;

    /* setup backend for data to be written to */
    dc = bt_diskmem_new();
    bt_diskmem_set_size(dc, 4);
    reader->piece = mock_piece_new("00000000000000000000", 4); /* fake hash */
    mock_piece_set_disk_blockrw(reader->piece, bt_diskmem_get_blockrw(dc), dc);
    blk.piece_idx = 0;
    blk.block_byte_offset = 0;
    blk.block_len = 4;
    mock_piece_write_block(reader->piece, NULL, &blk, piecedata);

    return msg;
}

/*----------------------------------------------------------------------------*/
/*  Send data                                                                 */
/*----------------------------------------------------------------------------*/

unsigned char *__sender_set(
    test_sender_t * sender
)
{
    bt_block_t blk;
    char piecedata[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    void *dc;

    /* init the sender */
    memset(sender, 0, sizeof(test_sender_t));

    /* create holding place for data written */
    dc = bt_diskmem_new();
    bt_diskmem_set_size(dc, 4);

    /* create mock piece */
    sender->piece = mock_piece_new("00000000000000000000", 4);
    mock_piece_set_disk_blockrw(sender->piece, bt_diskmem_get_blockrw(dc), dc);
    /* write block of length 4 */
    /* piece idx not necessary */
    blk.piece_idx = 0;
    blk.block_byte_offset = 0;
    blk.block_len = 4;
    mock_piece_write_block(sender->piece, NULL, &blk, piecedata);


    return sender->last_send_data;
}

/*  Just a mock.
 *  Used when sent data is not tested */
int __FUNC_MOCK_send(
    void* s __attribute__((__unused__)),
    const void *peer __attribute__((__unused__)),
    const void *send_data __attribute__((__unused__)),
    const int len __attribute__((__unused__))
)
{
    return 1;
}

int __FUNC_send(
    void* s,
    const void *peer __attribute__((__unused__)),
    const void *send_data,
    const int len
)
{
    test_sender_t * sender = s;

#if 0
    int ii;

    for (ii = 0; ii < len; ii++)
    {
        if (ii % 4 == 0 && ii != 0) printf(" ");
        printf("%02x,", ((const unsigned char*)send_data)[ii]);
    }
    printf("\n");
#endif

    memcpy(sender->last_send_data + sender->pos, send_data, len);

    sender->pos += len;

    return 1;
}

int __FUNC_failing_send(
    void * sender __attribute__((__unused__)),
    const void *peer __attribute__((__unused__)),
    const void *send_data __attribute__((__unused__)),
    const int len __attribute__((__unused__))
)
{
    return 0;
}

void *__FUNC_sender_get_piece(
    void* s,
    const int idx __attribute__((__unused__))
)
{
    test_sender_t * sender = s;
    void *piece;

    piece = sender->piece;
    return piece;
}

/**
 * Mock an always complete piece */
int __FUNC_pieceiscomplete(
    void *bto __attribute__((__unused__)),
    void *piece __attribute__((__unused__))
)
{
//    void *piece;
//    piece = sender->piece;
    return 1;
}

/**
 * this function pretends as every piece is incomplete
 */
int __FUNC_pieceiscomplete_fail(
    void *bto __attribute__((__unused__)),
    void *piece __attribute__((__unused__))
)
{
    return 0;
}


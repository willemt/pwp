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

#include "bitfield.h"
#include "pwp_connection.h"
#include "bitstream.h"
#include "mock_block_readwriter.h"

/*  bittorrent piece */
typedef struct
{
    /* index on 'bit stream' */
    int idx;

    int piece_length;

    void* data;

    /* functions and data for reading/writing block data */
    bt_blockrw_i *disk;
    void *disk_udata;

    bt_blockrw_i irw;
} __piece_private_t;

#define priv(x) ((__piece_private_t*)(x))

void *mock_piece_new(
    const char *sha1sum __attribute__((__unused__)),
    const int piece_bytes_size
)
{
    __piece_private_t *pce;

    pce = malloc(sizeof(__piece_private_t));
//    priv(pce)->sha1 = malloc(20);
//    memcpy(priv(pce)->sha1, sha1sum, 20);
    priv(pce)->piece_length = piece_bytes_size;
//    priv(pce)->irw.read_block = bt_piece_read_block;
//    priv(pce)->irw.write_block = bt_piece_write_block;
    priv(pce)->data = malloc(priv(pce)->piece_length);
    memset(priv(pce)->data, 0, piece_bytes_size);

    return (void*)pce;
}

void mock_piece_set_disk_blockrw(
    void * pce,
    bt_blockrw_i * irw,
    void *udata
)
{
    priv(pce)->disk = irw;
    priv(pce)->disk_udata = udata;
}

int mock_piece_write_block(
    void *me,
    void *caller __attribute__((__unused__)),
    const bt_block_t * blk,
    const void *blkdata
)
{
    assert(priv(me)->disk);

    if (!priv(me)->disk)
    {
        return 0;
    }

    priv(me)->disk->write_block(priv(me)->disk_udata, me, blk, blkdata);

    return 1;
}

void mock_piece_write_block_to_stream(
    void * me,
    bt_block_t * blk,
    unsigned char ** msg
)
{
    unsigned char *data;
    int ii;

    data = priv(me)->disk->read_block(priv(me)->disk_udata, NULL, blk);
//    data = priv(me)->data + blk->byte_offset;

//    printf("writingblock \n");
    for (ii = 0; ii < blk->len; ii++)
    {
        unsigned char val;

        val = *(data + ii);
        bitstream_write_ubyte(msg, val);

        if (ii % 4 == 0 && ii != 0) printf(" ");
//        printf("%02x,", *(data + ii));
    }
//    printf("\n");
}


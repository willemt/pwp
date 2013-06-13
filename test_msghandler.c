
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "CuTest.h"

#include "bitfield.h"
#include "pwp_connection.h"
#include "pwp_msghandler.h"
#include "bitstream.h"

typedef struct {

    int mtype;

    union {
        msg_have_t have;
        msg_bitfield_t bitfield;
        msg_request_t request;
        msg_cancel_t cancel;
        msg_piece_t piece;
    };

} fake_pc_t;


void pwp_conn_keepalive(void* pco)
{
    fake_pc_t* pc = pco;
    pc->mtype = -1;
//    printf("KEEPALIVE\n");
}

void pwp_conn_choke(void* pco)
{
    fake_pc_t* pc = pco;
    pc->mtype = PWP_MSGTYPE_CHOKE;
//    printf("CHOKE\n");
}

void pwp_conn_unchoke(void* pco)
{
    fake_pc_t* pc = pco;
    pc->mtype = PWP_MSGTYPE_UNCHOKE;
//    printf("UNCHOKE\n");
}

void pwp_conn_interested(void* pco)
{
    fake_pc_t* pc = pco;
    pc->mtype = PWP_MSGTYPE_INTERESTED;
//    printf("INTERESTED\n");
}

void pwp_conn_uninterested(void* pco)
{
    fake_pc_t* pc = pco;
    pc->mtype = PWP_MSGTYPE_UNINTERESTED;
//    printf("UNINTERESTED\n");
}

void pwp_conn_have(void* pco, msg_have_t* have)
{
    fake_pc_t* pc = pco;
    pc->mtype = PWP_MSGTYPE_HAVE;
//    printf("HAVE %d\n", have->pieceidx);
    memcpy(&pc->have,have,sizeof(msg_have_t));
}

void pwp_conn_bitfield(void* pco, msg_bitfield_t* bitfield)
{
    fake_pc_t* pc = pco;
    pc->mtype = PWP_MSGTYPE_BITFIELD;
//    printf("BITFIELD %s\n", bitfield_str(&bitfield->bf));
    memcpy(&pc->bitfield,bitfield,sizeof(msg_bitfield_t));
}

void pwp_conn_request(void* pco,
    msg_request_t *request)
{
    fake_pc_t* pc = pco;
    pc->mtype = PWP_MSGTYPE_REQUEST;
//    printf("REQUEST %d %d %d\n",
//            request->pieceidx,
//            request->block_byte_offset,
//            request->block_len);
    memcpy(&pc->request,request,sizeof(msg_request_t));
}

void pwp_conn_cancel(void* pco,
    msg_cancel_t *cancel)
{
    fake_pc_t* pc = pco;
    pc->mtype = PWP_MSGTYPE_CANCEL;
//    printf("CANCEL %d %d %d\n",
//            cancel->pieceidx,
//            cancel->block_byte_offset,
//            cancel->block_len);
    memcpy(&pc->cancel,cancel,sizeof(msg_cancel_t));
}

void pwp_conn_piece(void* pco,
    msg_piece_t *piece)
{
    fake_pc_t* pc = pco;
    pc->mtype = PWP_MSGTYPE_PIECE;
    memcpy(&pc->piece,piece,sizeof(msg_piece_t));
}

void TestPWP_keepalive(
    CuTest * tc
)
{
    fake_pc_t pc;
    unsigned char data[100];
    unsigned char* ptr;
    void* mh;

    /* keepalive */
    ptr = data;
    mh = pwp_msghandler_new(&pc);
    bitstream_write_uint32(&ptr,0);
    pwp_msghandler_dispatch_from_buffer(mh, data, 4);
    CuAssertTrue(tc, -1 == pc.mtype);
}

void TestPWP_choke(
    CuTest * tc
)
{
    fake_pc_t pc;
    unsigned char data[100];
    unsigned char* ptr;
    void* mh;

    /* choke */
    ptr = data;
    mh = pwp_msghandler_new(&pc);
    bitstream_write_uint32(&ptr,1);
    bitstream_write_ubyte(&ptr,PWP_MSGTYPE_CHOKE);
    pwp_msghandler_dispatch_from_buffer(mh, data, 4 + 1);
    CuAssertTrue(tc, PWP_MSGTYPE_CHOKE == pc.mtype);
}

void TestPWP_unchoke(
    CuTest * tc
)
{
    fake_pc_t pc;
    unsigned char data[100];
    unsigned char* ptr;
    void* mh;

    /* unchoke */
    ptr = data;
    mh = pwp_msghandler_new(&pc);
    bitstream_write_uint32(&ptr,1);
    bitstream_write_ubyte(&ptr,PWP_MSGTYPE_UNCHOKE);
    pwp_msghandler_dispatch_from_buffer(mh, data, 4 + 1);
    CuAssertTrue(tc, PWP_MSGTYPE_UNCHOKE == pc.mtype);
}

void TestPWP_interested(
    CuTest * tc
)
{
    fake_pc_t pc;
    unsigned char data[100];
    unsigned char* ptr;
    void* mh;

    /* interested */
    ptr = data;
    mh = pwp_msghandler_new(&pc);
    bitstream_write_uint32(&ptr,1);
    bitstream_write_ubyte(&ptr,PWP_MSGTYPE_INTERESTED);
    pwp_msghandler_dispatch_from_buffer(mh, data, 4 + 1);
    CuAssertTrue(tc, PWP_MSGTYPE_INTERESTED == pc.mtype);
}

void TestPWP_uninterested(
    CuTest * tc
)
{
    fake_pc_t pc;
    unsigned char data[100];
    unsigned char* ptr;
    void* mh;

    /* uninterested */
    ptr = data;
    mh = pwp_msghandler_new(&pc);
    bitstream_write_uint32(&ptr,1);
    bitstream_write_ubyte(&ptr,PWP_MSGTYPE_UNINTERESTED);
    pwp_msghandler_dispatch_from_buffer(mh, data, 4 + 1);
    CuAssertTrue(tc, PWP_MSGTYPE_UNINTERESTED == pc.mtype);
}

void TestPWP_have(
    CuTest * tc
)
{
    fake_pc_t pc;
    unsigned char data[100];
    unsigned char* ptr;
    void* mh;

    /* have */
    ptr = data;
    mh = pwp_msghandler_new(&pc);
    bitstream_write_uint32(&ptr,5);
    bitstream_write_ubyte(&ptr,PWP_MSGTYPE_HAVE);
    bitstream_write_uint32(&ptr,999);
    pwp_msghandler_dispatch_from_buffer(mh, data, 4 + 1 + 4);
    CuAssertTrue(tc, PWP_MSGTYPE_HAVE == pc.mtype);
    CuAssertTrue(tc, 999 == pc.have.pieceidx);
}

void TestPWP_request(
    CuTest * tc
)
{
    fake_pc_t pc;
    unsigned char data[100];
    unsigned char* ptr;
    void* mh;

    /* request */
    ptr = data;
    mh = pwp_msghandler_new(&pc);
    bitstream_write_uint32(&ptr,13);
    bitstream_write_ubyte(&ptr,PWP_MSGTYPE_REQUEST);
    bitstream_write_uint32(&ptr,123);
    bitstream_write_uint32(&ptr,456);
    bitstream_write_uint32(&ptr,789);
    pwp_msghandler_dispatch_from_buffer(mh, data, 4 + 1 + 4 + 4 + 4);
    CuAssertTrue(tc, PWP_MSGTYPE_REQUEST == pc.mtype);
    CuAssertTrue(tc, 123 == pc.request.pieceidx);
    CuAssertTrue(tc, 456 == pc.request.block_byte_offset);
    CuAssertTrue(tc, 789 == pc.request.block_len);
}

void TestPWP_cancel(
    CuTest * tc
)
{
    fake_pc_t pc;
    unsigned char data[100];
    unsigned char* ptr;
    void* mh;

    /* cancel */
    ptr = data;
    mh = pwp_msghandler_new(&pc);
    bitstream_write_uint32(&ptr,13);
    bitstream_write_ubyte(&ptr,PWP_MSGTYPE_CANCEL);
    bitstream_write_uint32(&ptr,123);
    bitstream_write_uint32(&ptr,456);
    bitstream_write_uint32(&ptr,789);
    pwp_msghandler_dispatch_from_buffer(mh, data, 4 + 1 + 4 + 4 + 4);
    CuAssertTrue(tc, PWP_MSGTYPE_CANCEL == pc.mtype);
    CuAssertTrue(tc, 123 == pc.cancel.pieceidx);
    CuAssertTrue(tc, 456 == pc.cancel.block_byte_offset);
    CuAssertTrue(tc, 789 == pc.cancel.block_len);
}

void TestPWP_bitfield(
    CuTest * tc
)
{
    fake_pc_t pc;
    unsigned char data[100];
    unsigned char* ptr;
    void* mh;

    /* bitfield */
    ptr = data;
    mh = pwp_msghandler_new(&pc);
    bitstream_write_uint32(&ptr,2);
    bitstream_write_ubyte(&ptr,PWP_MSGTYPE_BITFIELD);
    bitstream_write_ubyte(&ptr,0x4e);
    pwp_msghandler_dispatch_from_buffer(mh, data, 4 + 1 + 1);
    CuAssertTrue(tc, PWP_MSGTYPE_BITFIELD == pc.mtype);
    CuAssertTrue(tc, 0 == strncmp("01011110",bitfield_str(&pc.bitfield.bf),8));
}

void TestPWP_piece(
    CuTest * tc
)
{
    fake_pc_t pc;
    unsigned char data[100];
    unsigned char* ptr;
    void* mh;

    /* piece */
    ptr = data;
    mh = pwp_msghandler_new(&pc);
    bitstream_write_uint32(&ptr,9 + 10);
    bitstream_write_ubyte(&ptr,PWP_MSGTYPE_PIECE);
    bitstream_write_uint32(&ptr,1);
    bitstream_write_uint32(&ptr,2);
    bitstream_write_ubyte(&ptr,'t');
    bitstream_write_ubyte(&ptr,'e');
    bitstream_write_ubyte(&ptr,'s');
    bitstream_write_ubyte(&ptr,'t');
    bitstream_write_ubyte(&ptr,' ');
    bitstream_write_ubyte(&ptr,'m');
    bitstream_write_ubyte(&ptr,'s');
    bitstream_write_ubyte(&ptr,'g');
    bitstream_write_ubyte(&ptr,'1');
    bitstream_write_ubyte(&ptr,'\0');
    pwp_msghandler_dispatch_from_buffer(mh, data, 4 + 1 + 4 + 4 + 10);
    CuAssertTrue(tc, PWP_MSGTYPE_PIECE == pc.mtype);
    CuAssertTrue(tc, 1 == pc.piece.pieceidx);
    CuAssertTrue(tc, 2 == pc.piece.block_byte_offset);
    CuAssertTrue(tc, 5 == pc.piece.data_size);
    CuAssertTrue(tc, 0 == strncmp("test ",pc.piece.data,pc.piece.data_size));
}

void TestPWP_piece_halfread(
    CuTest * tc
)
{
    fake_pc_t pc;
    unsigned char data[100];
    unsigned char* ptr;
    void* mh;

    /* piece */
    ptr = data;
    mh = pwp_msghandler_new(&pc);
    bitstream_write_uint32(&ptr,9 + 10);
    bitstream_write_ubyte(&ptr,PWP_MSGTYPE_PIECE);
    bitstream_write_uint32(&ptr,1);
    bitstream_write_uint32(&ptr,2);
    bitstream_write_ubyte(&ptr,'t');
    bitstream_write_ubyte(&ptr,'e');
    bitstream_write_ubyte(&ptr,'s');
    bitstream_write_ubyte(&ptr,'t');
    bitstream_write_ubyte(&ptr,' ');
    bitstream_write_ubyte(&ptr,'m');
    bitstream_write_ubyte(&ptr,'s');
    bitstream_write_ubyte(&ptr,'g');
    bitstream_write_ubyte(&ptr,'2');
    bitstream_write_ubyte(&ptr,'\0');
    /* read the first 5 bytes of data payload */
    pwp_msghandler_dispatch_from_buffer(mh, data, 4 + 1 + 4 + 4 + 5);
    CuAssertTrue(tc, PWP_MSGTYPE_PIECE == pc.mtype);
    CuAssertTrue(tc, 1 == pc.piece.pieceidx);
    CuAssertTrue(tc, 2 == pc.piece.block_byte_offset);
    CuAssertTrue(tc, 5 == pc.piece.data_size);
    CuAssertTrue(tc, 0 == strncmp("test msg2",pc.piece.data,pc.piece.data_size));
    /* read the last 5 bytes of data payload */
    pwp_msghandler_dispatch_from_buffer(mh, data + 4 + 1 + 4 + 4 + 5 , 5);
    CuAssertTrue(tc, PWP_MSGTYPE_PIECE == pc.mtype);
    CuAssertTrue(tc, 1 == pc.piece.pieceidx);
    CuAssertTrue(tc, 7 == pc.piece.block_byte_offset);
    CuAssertTrue(tc, 5 == pc.piece.data_size);
    CuAssertTrue(tc, 0 == strncmp("msg2",pc.piece.data,pc.piece.data_size));
}


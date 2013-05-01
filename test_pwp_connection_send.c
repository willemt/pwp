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

//static char* __mock_infohash = "00000000000000000000";
//static char* __mock_peerid = "00000000000000000000";

#define STATE_READY_TO_SENDRECV PC_CONNECTED | PC_HANDSHAKE_SENT | PC_HANDSHAKE_RECEIVED

static int __disconnect_msg(
        void* a __attribute__((__unused__)),
        void* b __attribute__((__unused__)),
        char* msg)
{
    printf("DISCONNECT: %s\n", (char*)msg);
    return 1;
}


/**
 * A request message spawns an appropriate response
 * Note: request message needs to check if we completed the requested piece
 */
void TestPWP_send_request_spawns_wellformed_piece_response(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .send = __FUNC_send,
        .recv = __FUNC_peercon_recv,
        .disconnect = __FUNC_disconnect,
        .pushblock = __FUNC_push_block,
        .write_block_to_stream = mock_piece_write_block_to_stream,
        .piece_is_complete = __FUNC_pieceiscomplete,
        .getpiece = __FUNC_sender_get_piece
    };

    unsigned char msg[50], *ptr = msg;
    void *pc;
    test_sender_t sender;
    bt_block_t request;

    /* create request message */
    request.piece_idx = 0;
    request.block_byte_offset = 0;
    request.block_len = 2;

    /*  piece */
    ptr = __sender_set(&sender);        //, msg);

#if 0
    bitstream_write_uint32(&ptr, 12);
    bitstream_write_ubyte(&ptr, 6);        /* request */
    bitstream_write_uint32(&ptr, 1);       /*  piece one */
    bitstream_write_uint32(&ptr, 0);       /*  block offset 0 */
    bitstream_write_uint32(&ptr, 2);       /*  block length 2 */
#endif

    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc,
                          PC_CONNECTED | PC_HANDSHAKE_SENT |
                          PC_HANDSHAKE_RECEIVED | PC_BITFIELD_RECEIVED);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &sender);
    /*  request message needs to check if we completed the requested piece */
    bt_peerconn_process_request(pc, &request);

    CuAssertTrue(tc, 8 + 1 + 2 == bitstream_read_uint32(&ptr));
    CuAssertTrue(tc, PWP_MSGTYPE_PIECE == bitstream_read_ubyte(&ptr));
    CuAssertTrue(tc, 0 == bitstream_read_uint32(&ptr)); /* piece one */
    CuAssertTrue(tc, 0 == bitstream_read_uint32(&ptr)); /* block offset */
    CuAssertTrue(tc, 0xde == bitstream_read_ubyte(&ptr));
    CuAssertTrue(tc, 0xad == bitstream_read_ubyte(&ptr));
}

void TestPWP_send_state_change_is_wellformed(
    CuTest * tc
)
{
    void *pc;

    test_sender_t sender;

    unsigned char *ptr;
    pwp_connection_functions_t funcs = {
        .send = __FUNC_send
    };

    ptr = __sender_set(&sender);
    pc = bt_peerconn_new();
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &sender);
    bt_peerconn_send_statechange(pc, PWP_MSGTYPE_INTERESTED);

    CuAssertTrue(tc, 1 == bitstream_read_uint32(&ptr));
    CuAssertTrue(tc, PWP_MSGTYPE_INTERESTED == bitstream_read_ubyte(&ptr));
}


/*
 * HAVE message has payload of 4
 * HAVE message has correct type flag
 */
void TestPWP_send_have_is_wellformed(
    CuTest * tc
)
{
    void *pc;

    test_sender_t sender;

    unsigned char *ptr;

    pwp_connection_functions_t funcs = {
        .send = __FUNC_send,
        .getpiece = __FUNC_sender_get_piece
    };

    ptr = __sender_set(&sender);
    pc = bt_peerconn_new();
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &sender);
    bt_peerconn_send_have(pc, 17);

    CuAssertTrue(tc, 5 == bitstream_read_uint32(&ptr));
    CuAssertTrue(tc, PWP_MSGTYPE_HAVE == bitstream_read_ubyte(&ptr));
    CuAssertTrue(tc, 17 == bitstream_read_uint32(&ptr));
}

/**
 * Also checks if spare bits are set or not */
void TestPWP_send_bitField_is_wellformed(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .send = __FUNC_send,
        .piece_is_complete = __FUNC_pieceiscomplete,
        .getpiece = __FUNC_sender_get_piece
    };

    void *pc;

    test_sender_t sender;

    unsigned char *ptr;

    ptr = __sender_set(&sender);
    pc = bt_peerconn_new();
    /*
     * .piece_len = 20,
     * .npieces = 20
     */
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &sender);
    /*  piece complete func will always return 1 (ie. piece is complete) */
    bt_peerconn_send_bitfield(pc);

    ptr = sender.last_send_data;
    /*  length */
    CuAssertTrue(tc, 4 == bitstream_read_uint32(&ptr));
    /*  message type */
    CuAssertTrue(tc, PWP_MSGTYPE_BITFIELD == bitstream_read_ubyte(&ptr));
    /* 11111111 11111111 11110000  */
    /*  please note the mock get_piece is always returning a piece */
    CuAssertTrue(tc, 0XFF == bitstream_read_ubyte(&ptr));
    CuAssertTrue(tc, 0XFF == bitstream_read_ubyte(&ptr));
    CuAssertTrue(tc, 0XF0 == bitstream_read_ubyte(&ptr));
}

/*
 * Request message has payload of 6
 */
void TestPWP_send_request_is_wellformed(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .send = __FUNC_send,
    };
    void *pc;

    test_sender_t sender;

    unsigned char *ptr;

    bt_block_t blk;

    blk.piece_idx = 1;
    blk.block_byte_offset = 0;
    blk.block_len = 20;

    ptr = __sender_set(&sender);
    pc = bt_peerconn_new();
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &sender);
    bt_peerconn_send_request(pc, &blk);

    CuAssertTrue(tc, 13 == bitstream_read_uint32(&ptr));
    CuAssertTrue(tc, 6 == bitstream_read_ubyte(&ptr));
    CuAssertTrue(tc, 1 == bitstream_read_uint32(&ptr));
    CuAssertTrue(tc, 0 == bitstream_read_uint32(&ptr));
    CuAssertTrue(tc, 20 == bitstream_read_uint32(&ptr));
}

#if 0
void TxestPWP_send_RequestForPieceOnlyIfPeerHasPiece_is_wellformed(
    CuTest * tc
)
{
    void *pc;

    test_sender_t sender;

    unsigned char *ptr;

    bt_block_t blk;

    blk.piece_idx = 1;
    blk.block_byte_offset = 0;
    blk.block_len = 20;

    ptr = __sender_set(&sender);
    pc = bt_peerconn_new();
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &sender);
    bt_peerconn_set_func_send(pc, (void *) __FUNC_send);
    bt_peerconn_send_request(pc, &blk);

    bt_peerconn_mark_peer_has_piece(pc, 1);

    CuAssertTrue(tc, 13 == bitstream_read_uint32(&ptr));
    CuAssertTrue(tc, 6 == bitstream_read_ubyte(&ptr));
    CuAssertTrue(tc, 1 == bitstream_read_uint32(&ptr));
    CuAssertTrue(tc, 0 == bitstream_read_uint32(&ptr));
    CuAssertTrue(tc, 20 == bitstream_read_uint32(&ptr));
}
#endif

void TestPWP_send_piece_is_wellformed(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .send = __FUNC_send,
        .write_block_to_stream = mock_piece_write_block_to_stream,
        .getpiece = __FUNC_sender_get_piece
    };
    void *pc;

    test_sender_t sender;

    unsigned char *ptr;

    bt_block_t blk;

    /*  get us 4 bytes of data */
    blk.piece_idx = 0;
    blk.block_byte_offset = 0;
    blk.block_len = 4;

    ptr = __sender_set(&sender);
    pc = bt_peerconn_new();
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &sender);
    bt_peerconn_send_piece(pc, &blk);
    /*  length */
    CuAssertTrue(tc, 13 == bitstream_read_uint32(&ptr));
    /*  msgtype */
    CuAssertTrue(tc, 7 == bitstream_read_ubyte(&ptr));
    /* piece idx  */
    CuAssertTrue(tc, 0 == bitstream_read_uint32(&ptr));
    /*  block offset */
    CuAssertTrue(tc, 0 == bitstream_read_uint32(&ptr));
    /*  block payload */
    CuAssertTrue(tc, 0XDE == bitstream_read_ubyte(&ptr));
    CuAssertTrue(tc, 0XAD == bitstream_read_ubyte(&ptr));
    CuAssertTrue(tc, 0XBE == bitstream_read_ubyte(&ptr));
    CuAssertTrue(tc, 0XEF == bitstream_read_ubyte(&ptr));
}

/*
 * Cancel has payload of 12
 */
void TestPWP_send_cancel_is_wellformed(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .send = __FUNC_send,
    };
    void *pc;

    test_sender_t sender;

    unsigned char *ptr;

    bt_block_t blk;

    blk.piece_idx = 1;
    blk.block_byte_offset = 0;
    blk.block_len = 20;

    ptr = __sender_set(&sender);
    pc = bt_peerconn_new();
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &sender);
    bt_peerconn_send_cancel(pc, &blk);

    CuAssertTrue(tc, 13 == bitstream_read_uint32(&ptr));
    CuAssertTrue(tc, 8 == bitstream_read_ubyte(&ptr));
    CuAssertTrue(tc, 1 == bitstream_read_uint32(&ptr));
    CuAssertTrue(tc, 0 == bitstream_read_uint32(&ptr));
    CuAssertTrue(tc, 20 == bitstream_read_uint32(&ptr));
}

/*----------------------------------------------------------------------------*/
/*  Receive data                                                              */
/*----------------------------------------------------------------------------*/

/*
 * HAVE message marks peer as having this piece
 */
void TestPWP_read_havemsg_marks_peer_as_having_piece(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .recv = __FUNC_peercon_recv,
        .disconnect = __disconnect_msg,
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[50], *ptr = msg;

    memset(&reader, 0, sizeof(test_reader_t));
    ptr = __reader_set(&reader, msg);
    bitstream_write_uint32(&ptr, 5);       /*  length */
    bitstream_write_ubyte(&ptr, 4);        /*  HAVE */
    bitstream_write_uint32(&ptr, 1);       /*  piece 1 */

    /*  peer connection */
    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc, STATE_READY_TO_SENDRECV);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, bt_peerconn_peer_has_piece(pc, 1));
}

/*
 * A peer receiving a HAVE message with bad index will:
 *  - Drop the connection
 */
void TestPWP_read_havemsg_disconnects_with_piece_idx_out_of_bounds(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .recv = __FUNC_peercon_recv,
        .disconnect = __FUNC_disconnect,
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[50], *ptr = msg;

    memset(&reader, 0, sizeof(test_reader_t));
    ptr = __reader_set(&reader, msg);
    bitstream_write_uint32(&ptr, 5);
    bitstream_write_ubyte(&ptr, 4);
    bitstream_write_uint32(&ptr, 10000);

    /*  peer connection */
    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc, STATE_READY_TO_SENDRECV);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_process_msg(pc);

    CuAssertTrue(tc, 1 == reader.has_disconnected);
}

/*
 * A peer receiving a HAVE message MUST send an interested message to the sender if indeed it lacks
 * the piece announced
 *
 * NOTE: NON-CRITICAL
 */
void TestPWP_send_interested_if_lacking_piece_from_have_msg(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .send = __FUNC_send,
        .recv = __FUNC_peercon_recv,
        .disconnect = __disconnect_msg,
        .getpiece = __FUNC_sender_get_piece
    };
    void *pc;
    test_sender_t sender;
    unsigned char msg[50], *ptr = msg;

    memset(&sender, 0, sizeof(test_sender_t));
    ptr = __sender_set(&sender, msg);
    bitstream_write_uint32(&ptr, 5);       /*  length */
    bitstream_write_ubyte(&ptr, 4);        /*  HAVE */
    bitstream_write_uint32(&ptr, 1);       /*  piece 1 */

    /*  peer connection */
    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc, STATE_READY_TO_SENDRECV);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &sender);
    bt_peerconn_process_msg(pc);

    /* check send interested message */
    CuAssertTrue(tc, 1 == bitstream_read_uint32(&ptr));
    CuAssertTrue(tc, 2 == bitstream_read_ubyte(&ptr));
}

/*
 * Choke message chokes us
 */
void TestPWP_read_chokemsg_marks_us_as_choked(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .recv = __FUNC_peercon_recv,
        .disconnect = __disconnect_msg,
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[50], *ptr = msg;

    memset(&reader, 0, sizeof(test_reader_t));

    /*  choke */
    ptr = __reader_set(&reader, msg);
    bitstream_write_uint32(&ptr, 1);
    bitstream_write_ubyte(&ptr, 0);

    /*  peer connection */
    pc = bt_peerconn_new();
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_set_state(pc, STATE_READY_TO_SENDRECV);
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 1 == bt_peerconn_im_choked(pc));
}

void TestPWP_read_chokemsg_empties_our_pending_requests(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .send = __FUNC_MOCK_send,
        .recv = __FUNC_peercon_recv,
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[50], *ptr = msg;
    bt_block_t blk;

    memset(&reader, 0, sizeof(test_reader_t));

    /*  block to request to increase pending queue */
    memset(&blk, 0, sizeof(bt_block_t));
    /*  choke */
    ptr = __reader_set(&reader, msg);
    bitstream_write_uint32(&ptr, 1);       /* length = 1 */
    bitstream_write_ubyte(&ptr, 0);        /* choke = 0 */

    /*  peer connection */
    pc = bt_peerconn_new();
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_set_state(pc, STATE_READY_TO_SENDRECV);
    bt_peerconn_request_block(pc, &blk);
    CuAssertTrue(tc, 1 == bt_peerconn_get_npending_requests(pc));
    bt_peerconn_process_msg(pc);
    /*  queue is now empty */
    CuAssertTrue(tc, 0 == bt_peerconn_get_npending_requests(pc));
}

/*
 * Unchoke message unchokes peer
 */
void TestPWP_read_unchokemsg_marks_us_as_unchoked(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .recv = __FUNC_peercon_recv,
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[50], *ptr = msg;

    memset(&reader, 0, sizeof(test_reader_t));

    /*  choke msg */
    ptr = __reader_set(&reader, msg);
    bitstream_write_uint32(&ptr, 1);
    bitstream_write_ubyte(&ptr, 0);

    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc, STATE_READY_TO_SENDRECV);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 1 == bt_peerconn_im_choked(pc));

    /*  unchoke msg */
    ptr = __reader_set(&reader, msg);
    bitstream_write_uint32(&ptr, 1);
    bitstream_write_ubyte(&ptr, 1);
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 0 == bt_peerconn_im_choked(pc));
}

/**
 * The peer shouldn't be requesting if we are choking them */
void TestPWP_read_request_msg_disconnects_if_peer_is_choked(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .recv = __FUNC_peercon_recv,
        .disconnect = __FUNC_disconnect,
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[50], *ptr = msg;

    memset(&reader, 0, sizeof(test_reader_t));

    /*  request piece */
    bitstream_write_uint32(&ptr, 12);      /*  payload length */
    bitstream_write_ubyte(&ptr, 6);        /*  message type */
    /*  invalid piece idx of 62 */
    bitstream_write_uint32(&ptr, 1);      /*  piece idx */
    bitstream_write_uint32(&ptr, 0);       /*  block offset */
    bitstream_write_uint32(&ptr, 1);       /*  block length */
    reader.pos = 0;
    reader.data = msg;

    ptr = __reader_set(&reader, msg);
    bitstream_write_uint32(&ptr, 1);
    bitstream_write_ubyte(&ptr, 0);

    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc, STATE_READY_TO_SENDRECV);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_choke(pc);
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 1 == reader.has_disconnected);
}

/*
 * Interested message shows peer is interested
 */
void TestPWP_read_peerisinterested_marks_peer_as_interested(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .send = __FUNC_MOCK_send,
        .recv = __FUNC_peercon_recv,
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[50], *ptr = msg;

    memset(&reader, 0, sizeof(test_reader_t));

    /*  show interest */
    ptr = __reader_set(&reader, msg);
    bitstream_write_uint32(&ptr, 1);       /* length = 1 */
    bitstream_write_ubyte(&ptr, 2);        /* interested = 2 */
    /*  peer connection */
    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc, STATE_READY_TO_SENDRECV);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 1 == bt_peerconn_peer_is_interested(pc));
}

/*
 * Uninterested message shows peer is uninterested
 */
void TestPWP_read_peerisuninterested_marks_peer_as_uninterested(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .send = __FUNC_MOCK_send,
        .recv = __FUNC_peercon_recv,
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[50], *ptr = msg;

    memset(&reader, 0, sizeof(test_reader_t));

    /*  show interest */
    ptr = __reader_set(&reader, msg);
    bitstream_write_uint32(&ptr, 1);
    bitstream_write_ubyte(&ptr, 2);
    /*  peer connection */
    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc, STATE_READY_TO_SENDRECV);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 1 == bt_peerconn_peer_is_interested(pc));

    /*  show uninterest */
    ptr = __reader_set(&reader, msg);
    bitstream_write_uint32(&ptr, 1);
    bitstream_write_ubyte(&ptr, 3);
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 0 == bt_peerconn_peer_is_interested(pc));

}

/*
 * Bitfield message marks peer as having these pieces
 */
void TestPWP_read_bitfield_marks_peers_pieces_as_haved_by_peer(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .recv = __FUNC_peercon_recv,
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[50], *ptr = msg;

    memset(&reader, 0, sizeof(test_reader_t));

    /*  bitfield */
    bitstream_write_uint32(&ptr, 4);
    bitstream_write_ubyte(&ptr, 5);        /*  bitpiece */
    bitstream_write_ubyte(&ptr, 0xff);     /*  11111111 */
    bitstream_write_ubyte(&ptr, 0xff);     /*  11111111 */
    bitstream_write_ubyte(&ptr, 0xf0);     /*  11110000 */
    reader.pos = 0;
    reader.data = msg;
    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc, STATE_READY_TO_SENDRECV);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    CuAssertTrue(tc, 0 == bt_peerconn_peer_has_piece(pc, 0));
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 1 == bt_peerconn_peer_has_piece(pc, 0));
    CuAssertTrue(tc, 1 == bt_peerconn_peer_has_piece(pc, 1));
    CuAssertTrue(tc, 1 == bt_peerconn_peer_has_piece(pc, 2));
    CuAssertTrue(tc, 1 == bt_peerconn_peer_has_piece(pc, 3));
//    CuAssertTrue(tc, 0);
}

/*
 * Disconnect if bitfield sent more than once 
 */
void TestPWP_read_disconnect_if_bitfield_received_more_than_once(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .recv = __FUNC_peercon_recv,
        .disconnect = __FUNC_disconnect,
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[50], *ptr = msg;

    memset(&reader, 0, sizeof(test_reader_t));

    /*  bitfield */
    bitstream_write_uint32(&ptr, 1);
    bitstream_write_ubyte(&ptr, 5);
    bitstream_write_ubyte(&ptr, 0xff);
    bitstream_write_ubyte(&ptr, 0xff);
    bitstream_write_ubyte(&ptr, 0xf0);

    reader.pos = 0;
    reader.data = msg;

    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc,
                          PC_CONNECTED | PC_HANDSHAKE_SENT |
                          PC_HANDSHAKE_RECEIVED | PC_BITFIELD_RECEIVED);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 1 == reader.has_disconnected);
}

#if 0
void TxestPWP_readBitfieldGreaterThanNPieces(
    CuTest * tc
)
{
    void *pc;
    test_reader_t reader;
    unsigned char msg[50], *ptr = msg;

    memset(&reader, 0, sizeof(test_reader_t));

    /*  bitfield */
    ptr = __reader_set(&reader, msg);
    bitstream_write_uint32(&ptr, 4);
    bitstream_write_ubyte(&ptr, 5);
    bitstream_write_ubyte(&ptr, 0xF0);     // 11110000
    bitstream_write_ubyte(&ptr, 0x00);     // 00000000
    bitstream_write_ubyte(&ptr, 0x00);     // 00000000
    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc, STATE_READY_TO_SENDRECV);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_set_func_disconnect(pc, (void *) __FUNC_disconnect);
    bt_peerconn_set_func_recv(pc, (void *) __FUNC_peercon_recv);
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 0 == reader.has_disconnected);
    CuAssertTrue(tc, 1 == bt_peerconn_peer_has_piece(pc, 0));
    CuAssertTrue(tc, 1 == bt_peerconn_peer_has_piece(pc, 3));
    CuAssertTrue(tc, 0 == bt_peerconn_peer_has_piece(pc, 4));

    /*  bad bitfield */
    ptr = __reader_set(&reader, msg);
    bitstream_write_uint32(&ptr, 4);
    bitstream_write_ubyte(&ptr, 5);
    bitstream_write_ubyte(&ptr, 0xF0);     // 11110000
    bitstream_write_ubyte(&ptr, 0x00);     // 00000000
    bitstream_write_ubyte(&ptr, 0x08);     // 00001000
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 1 == reader.has_disconnected);
}

void TxestPWP_readBitfieldLessThanNPieces(
    CuTest * tc
)
{
    void *pc;
    test_reader_t reader;
    unsigned char msg[50], *ptr = msg;

    memset(&reader, 0, sizeof(test_reader_t));

    /*  bitfield */
    ptr = __reader_set(&reader, msg);
    bitstream_write_uint32(&ptr, 3);
    bitstream_write_ubyte(&ptr, 5);
    bitstream_write_ubyte(&ptr, 0xF0);     // 11110000
    bitstream_write_ubyte(&ptr, 0x00);     // 00000000
    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc, STATE_READY_TO_SENDRECV);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_set_func_disconnect(pc, (void *) __FUNC_disconnect);
    bt_peerconn_set_func_recv(pc, (void *) __FUNC_peercon_recv);
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 1 == reader.has_disconnected);
}
#endif

/*----------------------------------------------------------------------------*/



/*
 * The peer will disconnect if we request a piece it hasn't completed.
 * We should know better
 * NOTE: request message needs to check if we completed the requested piece
 */
void TestPWP_read_request_of_piece_not_completed_disconnects_peer(
    CuTest * tc
)
{
    void *pc;
    test_reader_t reader;
    unsigned char msg[50], *ptr = msg;

    memset(&reader, 0, sizeof(test_reader_t));
    //ptr = __reader_set(&reader, msg);

    /*  piece */
    bitstream_write_uint32(&ptr, 12);      /*  payload length */
    bitstream_write_ubyte(&ptr, 6);        /*  message type: request */
    /*  invalid piece idx of zero */
    bitstream_write_uint32(&ptr, 1);       /*  piece idx */
    bitstream_write_uint32(&ptr, 0);       /*  block offset */
    /*  invalid length of zero */
    bitstream_write_uint32(&ptr, 2);       /*  block length */

    reader.pos = 0;
    reader.data = msg;

    pwp_connection_functions_t funcs = {
        .recv = __FUNC_peercon_recv,
        .disconnect = __FUNC_disconnect,
        .piece_is_complete = __FUNC_pieceiscomplete_fail,
        .getpiece = __FUNC_reader_get_piece
    };

    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc,
                          PC_CONNECTED | PC_HANDSHAKE_SENT |
                          PC_HANDSHAKE_RECEIVED | PC_BITFIELD_RECEIVED);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 1 == reader.has_disconnected);
}

/**
 * A request must have a piece index that fits the expected bounds
 */
void TestPWP_read_request_with_invalid_piece_idx_disconnects_peer(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .recv = __FUNC_peercon_recv,
        .disconnect = __FUNC_disconnect,
        .piece_is_complete = __FUNC_pieceiscomplete,
        .getpiece = __FUNC_reader_get_piece
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[50], *ptr = msg;

    memset(&reader, 0, sizeof(test_reader_t));

    /*  piece */
    bitstream_write_uint32(&ptr, 12);      /*  payload length */
    bitstream_write_ubyte(&ptr, 6);        /*  message type */
    /*  invalid piece idx of 62 */
    bitstream_write_uint32(&ptr, 62);      /*  piece idx */
    bitstream_write_uint32(&ptr, 0);       /*  block offset */
    bitstream_write_uint32(&ptr, 1);       /*  block length */
    reader.pos = 0;
    reader.data = msg;

    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc,
                          PC_CONNECTED | PC_HANDSHAKE_SENT |
                          PC_HANDSHAKE_RECEIVED | PC_BITFIELD_RECEIVED);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    /*  we need this for us to know if the request is valid */
    /*  this is required for this test: */
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 1 == reader.has_disconnected);
}

/*
 * NON-CRITICAL
 */
void TestPWP_read_request_with_invalid_block_length_disconnects_peer(
    CuTest * tc
)
{
#if 0
    pwp_connection_functions_t funcs = {
        .send = __FUNC_failing_send,
        .recv = __FUNC_peercon_recv,
        .disconnect = __FUNC_disconnect,
        .pushblock = __FUNC_push_block,
        .write_block_to_stream = mock_piece_write_block_to_stream,
        .piece_is_complete = __FUNC_pieceiscomplete,
        .getpiece = __FUNC_sender_get_piece
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[50], *ptr = msg;

    memset(&reader, 0, sizeof(test_reader_t));

    /*  1. create a valid, connected, and handshaken peer */
    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc,
                          PC_CONNECTED | PC_HANDSHAKE_SENT |
                          PC_HANDSHAKE_RECEIVED | PC_BITFIELD_RECEIVED);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_set_func_disconnect(pc, (void *) __FUNC_disconnect);
    bt_peerconn_set_ipce(pc,
        NULL,
        NULL,
        __FUNC_sender_get_piece,
        (void*)&sender);
    bt_peerconn_set_func_recv(pc, (void *) __FUNC_peercon_recv);
    /*  we need this for us to know if the request is valid */
    bt_peerconn_set_func_piece_is_complete(pc, (void *) __FUNC_pieceiscomplete);

    /* 2. peer to send a HAVE */
    ptr = __reader_set(&reader, msg);
    bitstream_write_uint32(&ptr, 5);       /*  length */
    bitstream_write_ubyte(&ptr, 4);        /*  HAVE */
    bitstream_write_uint32(&ptr, 1);       /*  piece 1 */
    bt_peerconn_process_msg(pc);

    /*  we should have this piece for the test to be valid */
    CuAssertTrue(tc, bt_peerconn_peer_has_piece(pc, 1));

    /* 3. create piece with invalid block length */
    /* piece */
    ptr = __reader_set(&reader, msg);
    bitstream_write_uint32(&ptr, 12);      /*  payload length */
    bitstream_write_ubyte(&ptr, 6);        /*  message type : request */
    bitstream_write_uint32(&ptr, 1);       /*  piece idx */
    bitstream_write_uint32(&ptr, 0);       /*  block offset */
    /* invalid length of zero */
    bitstream_write_uint32(&ptr, 100000);  /*  block length */
    bt_peerconn_process_msg(pc);

    /* 4. check we haven't been disconnected */
    CuAssertTrue(tc, 1 == reader.has_disconnected);
#endif
    CuAssertTrue(tc, 0);
}

void TestPWP_read_request_of_piece_which_client_has_results_in_disconnect(
    CuTest * tc
)
{
    CuAssertTrue(tc, 0);
}


/*----------------------------------------------------------------------------*/

/*
 * Disconnect if piece message is received without us requesting the piece
 */
void TestPWP_read_piece_results_in_disconnect_if_we_havent_requested_this_piece(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .recv = __FUNC_peercon_recv,
        .disconnect = __FUNC_disconnect,
        .pushblock = __FUNC_push_block,
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[50], *ptr = msg;

    memset(&reader, 0, sizeof(test_reader_t));

    /*  piece */
//    ptr = __reader_set(&reader, msg);
    bitstream_write_uint32(&ptr, 11);      /*  payload */
    bitstream_write_ubyte(&ptr, 7);        /*  piece type */
    bitstream_write_uint32(&ptr, 1);       /*  piece one */
    bitstream_write_uint32(&ptr, 0);       /*  byte offset is 0 */
    bitstream_write_ubyte(&ptr, 0xDE);
    bitstream_write_ubyte(&ptr, 0xAD);
    reader.pos = 0;
    reader.data = msg;

    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc, STATE_READY_TO_SENDRECV);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 1 == reader.has_disconnected);
}

void TestPWP_read_piece_results_in_correct_receivable(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .recv = __FUNC_peercon_recv,
        .disconnect = __FUNC_disconnect,
        .pushblock = __FUNC_push_block,
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[50], *ptr = msg;
    bt_block_t blk;

    memset(&reader, 0, sizeof(test_reader_t));

    /*  piece */
    ptr = __reader_set(&reader, msg);
    bitstream_write_uint32(&ptr, 11);      /*  payload */
    bitstream_write_ubyte(&ptr, 7);        /*  piece type */
    bitstream_write_uint32(&ptr, 1);       /*  piece one */
    bitstream_write_uint32(&ptr, 0);       /*  byte offset is 0 */
    bitstream_write_ubyte(&ptr, 0xDE);
    bitstream_write_ubyte(&ptr, 0xAD);

    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc,
                          PC_CONNECTED | PC_HANDSHAKE_SENT |
                          PC_HANDSHAKE_RECEIVED | PC_BITFIELD_RECEIVED);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    /*  make sure we're at least requesting this piece */
    blk.piece_idx = 1;
    blk.block_byte_offset = 0;
    blk.block_len = 2;
    bt_peerconn_request_block(pc, &blk);
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 0 == reader.has_disconnected);
    CuAssertTrue(tc, 1 == reader.last_block.piece_idx);
    CuAssertTrue(tc, 0 == reader.last_block.block_byte_offset);
    CuAssertTrue(tc, 2 == reader.last_block.block_len);
    CuAssertTrue(tc, 0xDE == reader.last_block_data[0]);
    CuAssertTrue(tc, 0xAD == reader.last_block_data[1]);
}

void TestPWP_requesting_block_increments_pending_requests(
    CuTest * tc
)
{    pwp_connection_functions_t funcs = {
        .send = __FUNC_MOCK_send,
    };
    bt_block_t blk;
    void *pc;

    /*  block */
    memset(&blk, 0, sizeof(bt_block_t));
    /* peer connection */
    pc = bt_peerconn_new();
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, NULL);
    CuAssertTrue(tc, 0 == bt_peerconn_get_npending_requests(pc));
    bt_peerconn_request_block(pc, &blk);
    CuAssertTrue(tc, 1 == bt_peerconn_get_npending_requests(pc));
}

void TestPWP_read_piece_decreases_pending_requests(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .send = __FUNC_MOCK_send,
        .recv = __FUNC_peercon_recv,
        .pushblock = __FUNC_MOCK_push_block,
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[50], *ptr = msg;
    bt_block_t blk;

    memset(&reader, 0, sizeof(test_reader_t));
    memset(&blk, 0, sizeof(bt_block_t));
    blk.piece_idx = 0;
    blk.block_len = 1;
    /*  piece */
    ptr = __reader_set(&reader, msg);
    bitstream_write_uint32(&ptr, 10);      /* data length = 9 (hdr) + 1 (data) */
    bitstream_write_ubyte(&ptr, 7);        /* piece msg = 7 */
    bitstream_write_uint32(&ptr, 0);       /* piece idx */
    bitstream_write_uint32(&ptr, 0);       /* block offset */
    bitstream_write_uint32(&ptr, 0);       /* data */
    /*  peer connection */
    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc, STATE_READY_TO_SENDRECV);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_request_block(pc, &blk);
    CuAssertTrue(tc, 1 == bt_peerconn_get_npending_requests(pc));
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 0 == bt_peerconn_get_npending_requests(pc));
}

/*  
 * Cancel last message
 * Cancel removes from peer's request list
 * NON-CRITICAL
 */
void TestPWP_read_cancelmsg_cancels_last_request(
    CuTest * tc
)
{
#if 0
    pwp_connection_functions_t funcs = {
        .send = __FUNC_failing_send,
        .recv = __FUNC_peercon_recv,
        .disconnect = __FUNC_disconnect,
        .pushblock = __FUNC_push_block,
        .write_block_to_stream = mock_piece_write_block_to_stream,
        .piece_is_complete = __FUNC_pieceiscomplete,
        .getpiece = __FUNC_sender_get_piece
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[50], *ptr = msg;

    memset(&reader, 0, sizeof(test_reader_t));

    /*  piece */
    ptr = __reader_set(&reader, msg);
    bitstream_write_uint32(&ptr, 12);
    bitstream_write_ubyte(&ptr, 6);
    bitstream_write_uint32(&ptr, 1);
    bitstream_write_uint32(&ptr, 0);
    bitstream_write_uint32(&ptr, 2);
    pc = bt_peerconn_new();
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_set_func_disconnect(pc, (void *) __FUNC_disconnect);
    bt_peerconn_set_func_pushblock(pc, (void *) __FUNC_push_block);
    bt_peerconn_set_func_recv(pc, (void *) __FUNC_peercon_recv);
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 0 == reader.has_disconnected);
    CuAssertTrue(tc, 1 == reader.last_block.piece_idx);
    CuAssertTrue(tc, 0 == reader.last_block.block_byte_offset);
    CuAssertTrue(tc, 2 == reader.last_block.block_len);
    CuAssertTrue(tc, 0xDE == reader.last_block_data[0]);
    CuAssertTrue(tc, 0xAD == reader.last_block_data[1]);
#endif
    CuAssertTrue(tc, 0);
}

/*
 * All requests are dropped when a client chokes a peer
 */
void TestPWP_request_queue_dropped_when_peer_is_choked(
    CuTest * tc
)
{
    CuAssertTrue(tc, 0);
}

/*
 * Disconnect if a peer sends a data message while being choked
 */
void TestPWP_read_disconnect_if_peer_requests_while_choked(
    CuTest * tc
)
{
    CuAssertTrue(tc, 0);
}


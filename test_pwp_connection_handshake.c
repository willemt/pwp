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

static char* __mock_infohash = "abcdef12345678900000";
static char* __mock_their_peer_id = "00000000000000000000";
static char* __mock_my_peer_id = "00000000000000000001";

#define PROTOCOL_NAME "BitTorrent protocol"

/**
 * Set the HandshakeSent state when we send the handshake
 */
void TestPWP_sending_handshake_sets_handshake_sent_state(
    CuTest * tc
)
{
    void *pc;
    test_sender_t sender;
    pwp_connection_functions_t funcs = {
        .send = __FUNC_send
    };

    __sender_set(&sender);
    pc = bt_peerconn_new();
    bt_peerconn_set_my_peer_id(pc, __mock_my_peer_id);
    bt_peerconn_set_their_peer_id(pc, __mock_their_peer_id);
    bt_peerconn_set_infohash(pc,__mock_infohash);
    bt_peerconn_set_functions(pc, &funcs, &sender);
    bt_peerconn_send_handshake(pc);
    CuAssertTrue(tc, (PC_HANDSHAKE_SENT & bt_peerconn_get_state(pc)));
}

/**
 * Don't set handshake sent state when the send fails
 * */
void TestPWP_handshake_sent_state_not_set_when_send_failed(
    CuTest * tc
)
{
    void *pc;
    test_sender_t sender;
    pwp_connection_functions_t funcs = {
        .send = __FUNC_failing_send
    };

    __sender_set(&sender);
    pc = bt_peerconn_new();
    bt_peerconn_set_functions(pc, &funcs, &sender);
    /*  this sending function fails on every send */
    bt_peerconn_set_functions(pc, &funcs, &sender);
    bt_peerconn_set_my_peer_id(pc, __mock_my_peer_id);
    bt_peerconn_set_infohash(pc,__mock_infohash);
    bt_peerconn_send_handshake(pc);
    CuAssertTrue(tc, 0 == (PC_HANDSHAKE_SENT & bt_peerconn_get_state(pc)));
}

/*
 * Disconnect if non-handshake data arrives before the handshake
 */
void TestPWP_no_reading_without_handshake(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .recv = __FUNC_peercon_recv,
        .disconnect = __FUNC_disconnect,
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[1000], *ptr;

    memset(&reader, 0, sizeof(test_reader_t));

    /*  choke */
    ptr = __reader_set(&reader, msg);
    bitstream_write_uint32(&ptr, 1);       /*  length = 1 */
    bitstream_write_ubyte(&ptr, 0);        /*  0 = choke */
    /*  peer connection */
    pc = bt_peerconn_new();
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_set_state(pc, PC_CONNECTED);
    bt_peerconn_set_my_peer_id(pc, __mock_my_peer_id);
    /*  handshaking requires infohash */
    strcpy(reader.infohash, "00000000000000000000");
    bt_peerconn_set_infohash(pc,__mock_infohash);
    bt_peerconn_process_msg(pc);
    /* we have disconnected */
    CuAssertTrue(tc, 1 == reader.has_disconnected);
}

/**
 * Note this is not true as of 20130430. Bitfields are optional
 *
 * If bitfield not first message sent after handshake, then disconnect
 */
#if 0
void TxestPWP_read_msg_other_than_bitfield_after_handshake_disconnects(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .recv = __FUNC_peercon_recv,
        .disconnect = __FUNC_disconnect,
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[1000], *ptr;

    memset(&reader, 0, sizeof(test_reader_t));
    ptr = __reader_set(&reader, msg);
    bitstream_write_uint32(&ptr, 5);
    bitstream_write_ubyte(&ptr, 4);        /*  have */
    bitstream_write_uint32(&ptr, 1);       /*  piece 1 */

    /*  peer connection */
    pc = bt_peerconn_new();
    /*  current state is with handshake just completed */
    bt_peerconn_set_state(pc,
                          PC_CONNECTED | PC_HANDSHAKE_SENT |
                          PC_HANDSHAKE_RECEIVED);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_process_msg(pc);

    CuAssertTrue(tc, 1 == reader.has_disconnected);
}
#endif

void TestPWP_handshake_read_disconnect_if_handshake_has_invalid_name_length(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .recv = __FUNC_peercon_recv,
        .disconnect = __FUNC_disconnect,
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[1000], *ptr;
    int ii;

    memset(&reader, 0, sizeof(test_reader_t));

    /* handshake */
    ptr = __reader_set(&reader, msg);
    bitstream_write_ubyte(&ptr, 0); /* pn len */
//    bitstream_write_ubyte(&ptr, strlen(PROTOCOL_NAME)); /* pn len */
//    bitstream_write_string(&ptr, PROTOCOL_NAME, strlen(PROTOCOL_NAME); /* pn */
    for (ii=0;ii<8;ii++)
        bitstream_write_ubyte(&ptr, 0);        /*  reserved */
    bitstream_write_string(&ptr, __mock_infohash, 20); /* ih */
    bitstream_write_string(&ptr, __mock_their_peer_id, 20); /* pi */

    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc, PC_CONNECTED | PC_HANDSHAKE_SENT);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 1 == reader.has_disconnected);
}

void TestPWP_handshake_read_disconnect_if_handshake_has_invalid_protocol_name(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .recv = __FUNC_peercon_recv,
        .disconnect = __FUNC_disconnect,
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[1000], *ptr;
    int ii;

    memset(&reader, 0, sizeof(test_reader_t));

    /* handshake */
    ptr = __reader_set(&reader, msg);
    bitstream_write_ubyte(&ptr, strlen("Garbage Protocol")); /* pn len */
    bitstream_write_string(&ptr, "Garbage Protocol", strlen("Garbage Protocol")); /* pn */
    for (ii=0;ii<8;ii++)
        bitstream_write_ubyte(&ptr, 0);        /*  reserved */
    bitstream_write_string(&ptr, __mock_infohash, 20); /* ih */
    bitstream_write_string(&ptr, __mock_their_peer_id, 20); /* pi */

    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc, PC_CONNECTED | PC_HANDSHAKE_SENT);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 1 == reader.has_disconnected);
}

void TestPWP_handshake_read_disconnect_if_handshake_has_used_reserved_eight_bytes(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .recv = __FUNC_peercon_recv,
        .disconnect = __FUNC_disconnect,
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[1000], *ptr;
    int ii;

    memset(&reader, 0, sizeof(test_reader_t));

    /* handshake */
    ptr = __reader_set(&reader, msg);
    bitstream_write_ubyte(&ptr, strlen(PROTOCOL_NAME)); /* pn len */
    bitstream_write_string(&ptr, PROTOCOL_NAME, strlen(PROTOCOL_NAME)); /* pn */
    for (ii=0;ii<7;ii++)
        bitstream_write_ubyte(&ptr, 0);        /*  reserved */
    bitstream_write_ubyte(&ptr, 1);        /*  reserved */
    bitstream_write_string(&ptr, __mock_infohash, 20); /* ih */
    bitstream_write_string(&ptr, __mock_their_peer_id, 20); /* pi */

    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc, PC_CONNECTED | PC_HANDSHAKE_SENT);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_their_peer_id(pc,__mock_their_peer_id);
    bt_peerconn_set_infohash(pc,__mock_infohash);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 1 == reader.has_disconnected);
}

void TestPWP_handshake_read_disconnect_if_handshake_has_infohash_that_is_same_as_ours(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .recv = __FUNC_peercon_recv,
        .disconnect = __FUNC_disconnect,
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[1000], *ptr;
    int ii;

    memset(&reader, 0, sizeof(test_reader_t));

    /* handshake */
    ptr = __reader_set(&reader, msg);
    bitstream_write_ubyte(&ptr, strlen(PROTOCOL_NAME)); /* pn len */
    bitstream_write_string(&ptr, PROTOCOL_NAME, strlen(PROTOCOL_NAME)); /* pn */
    bitstream_write_ubyte(&ptr, 1);        /*  reserved */
    for (ii=0;ii<7;ii++)
        bitstream_write_ubyte(&ptr, 0);        /*  reserved */
    bitstream_write_string(&ptr, __mock_infohash, strlen(__mock_infohash)); /* ih */
    bitstream_write_string(&ptr, __mock_my_peer_id, strlen(__mock_my_peer_id)); /* pi */

    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc, PC_CONNECTED | PC_HANDSHAKE_SENT);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_infohash(pc,"00000000000000000001");
    bt_peerconn_set_my_peer_id(pc,__mock_my_peer_id);
    bt_peerconn_set_their_peer_id(pc,__mock_my_peer_id);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 1 == reader.has_disconnected);
}

/**
 * we obtain the Peer ID from a third party. The Peer ID as per the PWP connection must match.
 */
void TestPWP_handshake_read_disconnect_if_handshake_shows_a_peer_with_different_peer_id_than_expected(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .recv = __FUNC_peercon_recv,
        .disconnect = __FUNC_disconnect,
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[1000], *ptr;
    int ii;

    memset(&reader, 0, sizeof(test_reader_t));

    /* handshake */
    ptr = __reader_set(&reader, msg);
    bitstream_write_ubyte(&ptr, strlen(PROTOCOL_NAME)); /* pn len */
    bitstream_write_string(&ptr, PROTOCOL_NAME, strlen(PROTOCOL_NAME)); /* pn */
    bitstream_write_ubyte(&ptr, 1);        /*  reserved */
    for (ii=0;ii<7;ii++)
        bitstream_write_ubyte(&ptr, 0);        /*  reserved */
    bitstream_write_string(&ptr, __mock_infohash, 20); /* ih */
    bitstream_write_string(&ptr, "00000000000000000000", 20); /* pi */

    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc, PC_CONNECTED | PC_HANDSHAKE_SENT);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_their_peer_id(pc,__mock_their_peer_id);
    bt_peerconn_set_infohash(pc,__mock_infohash);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 1 == reader.has_disconnected);
}

/*
 * If this name matches the local peers own ID name, the connection MUST be dropped
 */
void TestPWP_handshake_read_disconnect_if_handshake_shows_peer_with_our_peer_id(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .recv = __FUNC_peercon_recv,
        .disconnect = __FUNC_disconnect,
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[1000], *ptr;
    int ii;

    memset(&reader, 0, sizeof(test_reader_t));

    /* handshake */
    ptr = __reader_set(&reader, msg);
    bitstream_write_ubyte(&ptr, strlen(PROTOCOL_NAME)); /* pn len */
    bitstream_write_string(&ptr, PROTOCOL_NAME, strlen(PROTOCOL_NAME)); /* pn */
    bitstream_write_ubyte(&ptr, 1);        /*  reserved */
    for (ii=0;ii<7;ii++)
        bitstream_write_ubyte(&ptr, 0);        /*  reserved */
    bitstream_write_string(&ptr, __mock_infohash, 20); /* ih */
    bitstream_write_string(&ptr, __mock_my_peer_id, 20); /* pi */

    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc, PC_CONNECTED | PC_HANDSHAKE_SENT);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_infohash(pc,__mock_infohash);
    bt_peerconn_set_my_peer_id(pc,__mock_my_peer_id);
    bt_peerconn_set_their_peer_id(pc,__mock_my_peer_id);
    bt_peerconn_set_infohash(pc,__mock_infohash);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 1 == reader.has_disconnected);
}

/*
 * If any other peer has already identified itself to the local peer using the same peer ID then:
 * NOTE: The connection MUST be dropped
 */
void TestPWP_handshake_read_disconnect_if_handshake_shows_a_peer_with_same_peer_id_as_other(
    CuTest * tc
)
{
    CuAssertTrue(tc, 0);
}

void TestPWP_handshake_read_valid_handshake_results_in_state_changing_to_handshake_received(
    CuTest * tc
)
{    pwp_connection_functions_t funcs = {
        .recv = __FUNC_peercon_recv,
        .disconnect = __FUNC_disconnect,
    };
    void *pc;
    test_reader_t reader;
    unsigned char msg[1000], *ptr;
    int ii;

    memset(&reader, 0, sizeof(test_reader_t));

    /* handshake */
    ptr = __reader_set(&reader, msg);
    bitstream_write_ubyte(&ptr, strlen(PROTOCOL_NAME)); /* pn len */
    bitstream_write_string(&ptr, PROTOCOL_NAME, strlen(PROTOCOL_NAME)); /* pn */
    for (ii=0;ii<8;ii++)
        bitstream_write_ubyte(&ptr, 0);        /*  reserved */
    bitstream_write_string(&ptr, __mock_infohash, 20); /* ih */
    bitstream_write_string(&ptr, __mock_their_peer_id, 20); /* pi */

    pc = bt_peerconn_new();
    bt_peerconn_set_state(pc, PC_CONNECTED | PC_HANDSHAKE_SENT);
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_infohash(pc,__mock_infohash);
    bt_peerconn_set_my_peer_id(pc,__mock_my_peer_id);
    bt_peerconn_set_their_peer_id(pc,__mock_their_peer_id);
    bt_peerconn_set_infohash(pc,__mock_infohash);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 0 == reader.has_disconnected);
    CuAssertTrue(tc, bt_peerconn_get_state(pc) == (PC_CONNECTED | PC_HANDSHAKE_SENT | PC_HANDSHAKE_RECEIVED));
}


#if 0
void TxestPWP_readPiece_disconnectsIfBlockTooBig(
    CuTest * tc
)
{
    void *pc;
    test_reader_t reader;
    unsigned char msg[1000], *ptr;

    memset(&reader, 0, sizeof(test_reader_t));

    /*  piece */
    ptr = __reader_set(&reader, msg);
    bitstream_write_uint32(&ptr, 11);
    bitstream_write_ubyte(&ptr, 7);
    bitstream_write_uint32(&ptr, 1);
    bitstream_write_uint32(&ptr, 0);
    bitstream_write_ubyte(&ptr, 0xDE);
    pc = bt_peerconn_new();
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &reader);
    bt_peerconn_set_func_disconnect(pc, (void *) __FUNC_disconnect);
    bt_peerconn_set_func_pushblock(pc, (void *) __FUNC_push_block);
    bt_peerconn_set_func_recv(pc, (void *) __FUNC_peercon_recv);
    bt_peerconn_process_msg(pc);
    CuAssertTrue(tc, 1 == reader.has_disconnected);
}
#endif

/*----------------------------------------------------------------------------*/

#if 0
void TxestPWP_send_HandShake_is_wellformed(
    CuTest * tc
)
{
    int data[10], len;
    unsigned char *msg;
    void *pc;
    test_sender_t sender;

    __sender_set(&sender);

    pc = bt_peerconn_new();
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_func_send(pc, __send);
    bt_peerconn_set_functions(pc, &funcs, &sender);
    /*  handshaking requires infohash */
    strcpy(sender.infohash, "00000000000000000000");
    bt_peerconn_set_infohash(pc,__mock_infohash);
    bt_peerconn_set_my_peer_id(pc, __mock_peer_id);
    bt_peerconn_send_handshake(pc);
}
#endif

void TestPWP_handshake_wont_send_unless_receieved_handshake(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .send = __FUNC_failing_send,
    };
    void *pc;
    test_sender_t sender;

    __sender_set(&sender);
    pc = bt_peerconn_new();
    bt_peerconn_set_piece_info(pc,20,20);
    bt_peerconn_set_functions(pc, &funcs, &sender);
    bt_peerconn_set_active(pc, 1);
    /*  handshaking requires infohash */
    strcpy(sender.infohash, "00000000000000000000");
    bt_peerconn_set_infohash(pc,__mock_infohash);
    bt_peerconn_set_my_peer_id(pc, __mock_my_peer_id);
    bt_peerconn_send_handshake(pc);
    CuAssertTrue(tc, 0 == bt_peerconn_is_active(pc));
}


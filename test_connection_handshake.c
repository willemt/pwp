#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "CuTest.h"

#include "bitfield.h"
#include "pwp_connection.h"
#include "bitstream.h"
#include "bt_block_readwriter_i.h"
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
    unsigned char msg[1000];
    void *pc;
    test_sender_t sender;
    pwp_connection_functions_t funcs = {
        .send = __FUNC_send
    };

    __sender_set(&sender,NULL,msg);
    pc = pwp_conn_new();
    pwp_conn_set_my_peer_id(pc, __mock_my_peer_id);
    pwp_conn_set_their_peer_id(pc, __mock_their_peer_id);
    pwp_conn_set_infohash(pc,__mock_infohash);
    pwp_conn_set_functions(pc, &funcs, &sender);
    pwp_conn_send_handshake(pc);
    CuAssertTrue(tc, (PC_HANDSHAKE_SENT & pwp_conn_get_state(pc)));
}

/**
 * Don't set handshake sent state when the send fails
 * */
void TestPWP_handshake_sent_state_not_set_when_send_failed(
    CuTest * tc
)
{
    unsigned char msg[1000];
    void *pc;
    test_sender_t sender;
    pwp_connection_functions_t funcs = {
        .send = __FUNC_failing_send
    };

    __sender_set(&sender,NULL,msg);
    pc = pwp_conn_new();
    pwp_conn_set_functions(pc, &funcs, &sender);
    /*  this sending function fails on every send */
    pwp_conn_set_functions(pc, &funcs, &sender);
    pwp_conn_set_my_peer_id(pc, __mock_my_peer_id);
    pwp_conn_set_infohash(pc,__mock_infohash);
    pwp_conn_send_handshake(pc);
    CuAssertTrue(tc, 0 == (PC_HANDSHAKE_SENT & pwp_conn_get_state(pc)));
}

/**
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
    test_sender_t sender;
    unsigned char msg[1000], *ptr = msg;

    memset(&sender, 0, sizeof(test_sender_t));

    /*  choke */
    __sender_set(&sender,msg,NULL);
    bitstream_write_uint32(&ptr, 1);       /*  length = 1 */
    bitstream_write_ubyte(&ptr, 0);        /*  0 = choke */
    /*  peer connection */
    pc = pwp_conn_new();
    pwp_conn_set_piece_info(pc,20,20);
    pwp_conn_set_functions(pc, &funcs, &sender);
    pwp_conn_set_state(pc, PC_CONNECTED);
    pwp_conn_set_my_peer_id(pc, __mock_my_peer_id);
    /*  handshaking requires infohash */
    strcpy(sender.infohash, "00000000000000000000");
    pwp_conn_set_infohash(pc,__mock_infohash);
    pwp_conn_process_msg(pc);
    /* we have disconnected */
    CuAssertTrue(tc, 1 == sender.has_disconnected);
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
    test_sender_t sender;
    unsigned char msg[1000], *ptr;

    memset(&sender, 0, sizeof(test_sender_t));
    ptr = __sender_set(&sender, msg);
    bitstream_write_uint32(&ptr, 5);
    bitstream_write_ubyte(&ptr, 4);        /*  have */
    bitstream_write_uint32(&ptr, 1);       /*  piece 1 */

    /*  peer connection */
    pc = pwp_conn_new();
    /*  current state is with handshake just completed */
    pwp_conn_set_state(pc,
                          PC_CONNECTED | PC_HANDSHAKE_SENT |
                          PC_HANDSHAKE_RECEIVED);
    pwp_conn_set_piece_info(pc,20,20);
    pwp_conn_set_functions(pc, &funcs, &sender);
    pwp_conn_process_msg(pc);

    CuAssertTrue(tc, 1 == sender.has_disconnected);
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
    test_sender_t sender;
    unsigned char msg[1000], *ptr = msg;
    int ii;

    memset(&sender, 0, sizeof(test_sender_t));

    /* handshake */
    __sender_set(&sender,msg,NULL);
    bitstream_write_ubyte(&ptr, 0); /* pn len */
//    bitstream_write_ubyte(&ptr, strlen(PROTOCOL_NAME)); /* pn len */
//    bitstream_write_string(&ptr, PROTOCOL_NAME, strlen(PROTOCOL_NAME); /* pn */
    for (ii=0;ii<8;ii++)
        bitstream_write_ubyte(&ptr, 0);        /*  reserved */
    bitstream_write_string(&ptr, __mock_infohash, 20); /* ih */
    bitstream_write_string(&ptr, __mock_their_peer_id, 20); /* pi */

    pc = pwp_conn_new();
    pwp_conn_set_state(pc, PC_CONNECTED | PC_HANDSHAKE_SENT);
    pwp_conn_set_piece_info(pc,20,20);
    pwp_conn_set_functions(pc, &funcs, &sender);
    pwp_conn_process_msg(pc);
    CuAssertTrue(tc, 1 == sender.has_disconnected);
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
    test_sender_t sender;
    unsigned char msg[1000], *ptr = msg;
    int ii;

    memset(&sender, 0, sizeof(test_sender_t));

    /* handshake */
    __sender_set(&sender,msg,NULL);
    bitstream_write_ubyte(&ptr, strlen("Garbage Protocol")); /* pn len */
    bitstream_write_string(&ptr, "Garbage Protocol", strlen("Garbage Protocol")); /* pn */
    for (ii=0;ii<8;ii++)
        bitstream_write_ubyte(&ptr, 0);        /*  reserved */
    bitstream_write_string(&ptr, __mock_infohash, 20); /* ih */
    bitstream_write_string(&ptr, __mock_their_peer_id, 20); /* pi */

    pc = pwp_conn_new();
    pwp_conn_set_state(pc, PC_CONNECTED | PC_HANDSHAKE_SENT);
    pwp_conn_set_piece_info(pc,20,20);
    pwp_conn_set_functions(pc, &funcs, &sender);
    pwp_conn_process_msg(pc);
    CuAssertTrue(tc, 1 == sender.has_disconnected);
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
    test_sender_t sender;
    unsigned char msg[1000], *ptr;
    int ii;

    memset(&sender, 0, sizeof(test_sender_t));

    ptr = msg;

    /* handshake */
    __sender_set(&sender,msg,NULL);
    bitstream_write_ubyte(&ptr, strlen(PROTOCOL_NAME)); /* pn len */
    bitstream_write_string(&ptr, PROTOCOL_NAME, strlen(PROTOCOL_NAME)); /* pn */
    for (ii=0;ii<7;ii++)
        bitstream_write_ubyte(&ptr, 0);        /*  reserved */
    bitstream_write_ubyte(&ptr, 1);        /*  reserved */
    bitstream_write_string(&ptr, __mock_infohash, 20); /* ih */
    bitstream_write_string(&ptr, __mock_their_peer_id, 20); /* pi */

    pc = pwp_conn_new();
    pwp_conn_set_state(pc, PC_CONNECTED | PC_HANDSHAKE_SENT);
    pwp_conn_set_piece_info(pc,20,20);
    pwp_conn_set_their_peer_id(pc,__mock_their_peer_id);
    pwp_conn_set_infohash(pc,__mock_infohash);
    pwp_conn_set_functions(pc, &funcs, &sender);
    pwp_conn_process_msg(pc);
    CuAssertTrue(tc, 1 == sender.has_disconnected);
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
    test_sender_t sender;
    unsigned char msg[1000], *ptr;
    int ii;

    memset(&sender, 0, sizeof(test_sender_t));

    ptr = msg;

    /* handshake */
    __sender_set(&sender,msg,NULL);
    bitstream_write_ubyte(&ptr, strlen(PROTOCOL_NAME)); /* pn len */
    bitstream_write_string(&ptr, PROTOCOL_NAME, strlen(PROTOCOL_NAME)); /* pn */
    bitstream_write_ubyte(&ptr, 1);        /*  reserved */
    for (ii=0;ii<7;ii++)
        bitstream_write_ubyte(&ptr, 0);        /*  reserved */
    bitstream_write_string(&ptr, __mock_infohash, strlen(__mock_infohash)); /* ih */
    bitstream_write_string(&ptr, __mock_my_peer_id, strlen(__mock_my_peer_id)); /* pi */

    pc = pwp_conn_new();
    pwp_conn_set_state(pc, PC_CONNECTED | PC_HANDSHAKE_SENT);
    pwp_conn_set_piece_info(pc,20,20);
    pwp_conn_set_infohash(pc,"00000000000000000001");
    pwp_conn_set_my_peer_id(pc,__mock_my_peer_id);
    pwp_conn_set_their_peer_id(pc,__mock_my_peer_id);
    pwp_conn_set_functions(pc, &funcs, &sender);
    pwp_conn_process_msg(pc);
    CuAssertTrue(tc, 1 == sender.has_disconnected);
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
    test_sender_t sender;
    unsigned char msg[1000], *ptr;
    int ii;

    memset(&sender, 0, sizeof(test_sender_t));

    ptr = msg;

    /* handshake */
    __sender_set(&sender,msg,NULL);
    bitstream_write_ubyte(&ptr, strlen(PROTOCOL_NAME)); /* pn len */
    bitstream_write_string(&ptr, PROTOCOL_NAME, strlen(PROTOCOL_NAME)); /* pn */
    bitstream_write_ubyte(&ptr, 1);        /*  reserved */
    for (ii=0;ii<7;ii++)
        bitstream_write_ubyte(&ptr, 0);        /*  reserved */
    bitstream_write_string(&ptr, __mock_infohash, 20); /* ih */
    bitstream_write_string(&ptr, "00000000000000000000", 20); /* pi */

    pc = pwp_conn_new();
    pwp_conn_set_state(pc, PC_CONNECTED | PC_HANDSHAKE_SENT);
    pwp_conn_set_piece_info(pc,20,20);
    pwp_conn_set_their_peer_id(pc,__mock_their_peer_id);
    pwp_conn_set_infohash(pc,__mock_infohash);
    pwp_conn_set_functions(pc, &funcs, &sender);
    pwp_conn_process_msg(pc);
    CuAssertTrue(tc, 1 == sender.has_disconnected);
}

/**
 * If this matches the local peers own ID , the connection MUST be dropped
 *
 * Note: some clients don't seem to have peer IDs
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
    test_sender_t sender;
    unsigned char msg[1000], *ptr;
    int ii;

    memset(&sender, 0, sizeof(test_sender_t));

    ptr = msg;

    /* handshake */
    __sender_set(&sender,msg,NULL);
    bitstream_write_ubyte(&ptr, strlen(PROTOCOL_NAME)); /* pn len */
    bitstream_write_string(&ptr, PROTOCOL_NAME, strlen(PROTOCOL_NAME)); /* pn */
    bitstream_write_ubyte(&ptr, 1);        /*  reserved */
    for (ii=0;ii<7;ii++)
        bitstream_write_ubyte(&ptr, 0);        /*  reserved */
    bitstream_write_string(&ptr, __mock_infohash, 20); /* ih */
    bitstream_write_string(&ptr, __mock_my_peer_id, 20); /* pi */

    pc = pwp_conn_new();
    pwp_conn_set_state(pc, PC_CONNECTED | PC_HANDSHAKE_SENT);
    pwp_conn_set_piece_info(pc,20,20);
    pwp_conn_set_infohash(pc,__mock_infohash);
    pwp_conn_set_my_peer_id(pc,__mock_my_peer_id);
    pwp_conn_set_their_peer_id(pc,__mock_my_peer_id);
    pwp_conn_set_infohash(pc,__mock_infohash);
    pwp_conn_set_functions(pc, &funcs, &sender);
    pwp_conn_process_msg(pc);
    CuAssertTrue(tc, 1 == sender.has_disconnected);
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
{
    pwp_connection_functions_t funcs = {
        .recv = __FUNC_peercon_recv,
        .disconnect = __FUNC_disconnect,
    };
    void *pc;
    test_sender_t sender;
    unsigned char msg[1000], *ptr;
    int ii;

    memset(&sender, 0, sizeof(test_sender_t));
    ptr = msg;

    /* handshake */
    __sender_set(&sender,msg,NULL);
    bitstream_write_ubyte(&ptr, strlen(PROTOCOL_NAME)); /* pn len */
    bitstream_write_string(&ptr, PROTOCOL_NAME, strlen(PROTOCOL_NAME)); /* pn */
    for (ii=0;ii<8;ii++)
        bitstream_write_ubyte(&ptr, 0);        /*  reserved */
    bitstream_write_string(&ptr, __mock_infohash, 20); /* ih */
    bitstream_write_string(&ptr, __mock_their_peer_id, 20); /* pi */

    pc = pwp_conn_new();
    pwp_conn_set_state(pc, PC_CONNECTED | PC_HANDSHAKE_SENT);
    pwp_conn_set_piece_info(pc,20,20);
    pwp_conn_set_infohash(pc,__mock_infohash);
    pwp_conn_set_my_peer_id(pc,__mock_my_peer_id);
    pwp_conn_set_their_peer_id(pc,__mock_their_peer_id);
    pwp_conn_set_infohash(pc,__mock_infohash);
    pwp_conn_set_functions(pc, &funcs, &sender);
    pwp_conn_process_msg(pc);
    CuAssertTrue(tc, 0 == sender.has_disconnected);
    CuAssertTrue(tc, pwp_conn_get_state(pc) == (PC_CONNECTED | PC_HANDSHAKE_SENT | PC_HANDSHAKE_RECEIVED));
}

void TestPWP_handshake_read_valid_handshake_results_in_sending_valid_handshake_and_bitfield(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .recv = __FUNC_peercon_recv,
        .disconnect = __FUNC_disconnect,
        .send = __FUNC_send,
        .getpiece = __FUNC_sender_get_piece,
        .piece_is_complete = __FUNC_pieceiscomplete,
    };
    void *pc;
    test_sender_t sender;
    unsigned char msg[1000], *ptr;
    unsigned char msg_s[1000];
    char buf_read[1000];
    int ii;

    memset(&sender, 0, sizeof(test_sender_t));
    ptr = msg;

    /* receive handshake */
    __sender_set(&sender,msg,msg_s);
    bitstream_write_ubyte(&ptr, strlen(PROTOCOL_NAME)); /* pn len */
    bitstream_write_string(&ptr, PROTOCOL_NAME, strlen(PROTOCOL_NAME)); /* pn */
    for (ii=0;ii<8;ii++)
        bitstream_write_ubyte(&ptr, 0);        /*  reserved */
    bitstream_write_string(&ptr, __mock_infohash, 20); /* ih */
    bitstream_write_string(&ptr, __mock_their_peer_id, 20); /* pi */

    pc = pwp_conn_new();
    pwp_conn_set_state(pc, PC_CONNECTED);
    pwp_conn_set_piece_info(pc,20,20);
    pwp_conn_set_infohash(pc,__mock_infohash);
    pwp_conn_set_my_peer_id(pc,__mock_my_peer_id);
    pwp_conn_set_their_peer_id(pc,__mock_their_peer_id);
    pwp_conn_set_infohash(pc,__mock_infohash);
    pwp_conn_set_functions(pc, &funcs, &sender);
    pwp_conn_process_msg(pc);
    CuAssertTrue(tc, 0 == sender.has_disconnected);
    CuAssertTrue(tc, pwp_conn_get_state(pc) == (PC_CONNECTED | PC_HANDSHAKE_SENT | PC_HANDSHAKE_RECEIVED));

    /* send handshake */
    ptr = msg_s;
    CuAssertTrue(tc, strlen(PROTOCOL_NAME) == bitstream_read_ubyte(&ptr)); /* pn len */
    bitstream_read_string(&ptr, buf_read, strlen(PROTOCOL_NAME)); /* pn */
    CuAssertTrue(tc, 0 == strncmp(buf_read, PROTOCOL_NAME, strlen(PROTOCOL_NAME)));
    for (ii=0;ii<8;ii++)
        CuAssertTrue(tc, 0 == bitstream_read_ubyte(&ptr));        /*  reserved */
    bitstream_read_string(&ptr, buf_read, 20); /* ih */
    CuAssertTrue(tc, 0 == strncmp(buf_read, __mock_infohash, 20));
    bitstream_read_string(&ptr, buf_read, 20); /* pi */
    CuAssertTrue(tc, 0 == strncmp(buf_read, __mock_my_peer_id, 20));

    /* send bitfield */
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


#if 0
void TxestPWP_readPiece_disconnectsIfBlockTooBig(
    CuTest * tc
)
{
    void *pc;
    test_sender_t sender;
    unsigned char msg[1000], *ptr;

    memset(&sender, 0, sizeof(test_sender_t));

    /*  piece */
    ptr = __sender_set(&sender, msg);
    bitstream_write_uint32(&ptr, 11);
    bitstream_write_ubyte(&ptr, 7);
    bitstream_write_uint32(&ptr, 1);
    bitstream_write_uint32(&ptr, 0);
    bitstream_write_ubyte(&ptr, 0xDE);
    pc = pwp_conn_new();
    pwp_conn_set_piece_info(pc,20,20);
    pwp_conn_set_functions(pc, &funcs, &sender);
    pwp_conn_set_func_disconnect(pc, (void *) __FUNC_disconnect);
    pwp_conn_set_func_pushblock(pc, (void *) __FUNC_push_block);
    pwp_conn_set_func_recv(pc, (void *) __FUNC_peercon_recv);
    pwp_conn_process_msg(pc);
    CuAssertTrue(tc, 1 == sender.has_disconnected);
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

    pc = pwp_conn_new();
    pwp_conn_set_piece_info(pc,20,20);
    pwp_conn_set_func_send(pc, __send);
    pwp_conn_set_functions(pc, &funcs, &sender);
    /*  handshaking requires infohash */
    strcpy(sender.infohash, "00000000000000000000");
    pwp_conn_set_infohash(pc,__mock_infohash);
    pwp_conn_set_my_peer_id(pc, __mock_peer_id);
    pwp_conn_send_handshake(pc);
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

    __sender_set(&sender,NULL,NULL);
    pc = pwp_conn_new();
    pwp_conn_set_piece_info(pc,20,20);
    pwp_conn_set_functions(pc, &funcs, &sender);
    pwp_conn_set_active(pc, 1);
    /*  handshaking requires infohash */
    strcpy(sender.infohash, "00000000000000000000");
    pwp_conn_set_infohash(pc,__mock_infohash);
    pwp_conn_set_my_peer_id(pc, __mock_my_peer_id);
    pwp_conn_send_handshake(pc);
    CuAssertTrue(tc, 0 == pwp_conn_is_active(pc));
}

void TestPWP_handshake_stepping_wont_send_unless_receieved_handshake(
    CuTest * tc
)
{
    unsigned char msg[1000];
    pwp_connection_functions_t funcs = {
        .send = __FUNC_send,
        .connect = __FUNC_connect,
    };
    void *pc;
    test_sender_t sender;

    __sender_set(&sender,NULL,msg);
    pc = pwp_conn_new();
    pwp_conn_set_piece_info(pc,20,20);
    pwp_conn_set_functions(pc, &funcs, &sender);
    pwp_conn_set_active(pc, 1);
    /*  handshaking requires infohash */
    strcpy(sender.infohash, "00000000000000000000");
    pwp_conn_set_infohash(pc,__mock_infohash);
    pwp_conn_set_my_peer_id(pc, __mock_my_peer_id);
    pwp_conn_send_handshake(pc);
    CuAssertTrue(tc, 1 == sender.nsent_messages); 

    /* stepping results in the client sending interested messages.
     * Lets make sure it doesn't do that, since it hasn't received a handshake yet. */
    pwp_conn_step(pc);
    CuAssertTrue(tc, 1 == sender.nsent_messages); 
    pwp_conn_step(pc);
    CuAssertTrue(tc, 1 == sender.nsent_messages); 
    pwp_conn_step(pc);
    CuAssertTrue(tc, 1 == sender.nsent_messages); 
}


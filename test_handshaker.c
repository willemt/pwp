#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "CuTest.h"

#include "bitfield.h"
#include "pwp_connection.h"
#include "pwp_handshaker.h"
#include "bitstream.h"
#include "bt_block_readwriter_i.h"
#include "mock_piece.h"
#include "test_connection.h"

static char* __mock_infohash = "abcdef12345678900000";
static char* __mock_their_peer_id = "00000000000000000000";
static char* __mock_my_peer_id = "00000000000000000001";

#define PROTOCOL_NAME "BitTorrent protocol"

/**
 * Note this is not true as of 20130430. Bitfields are optional
 *
 * If bitfield not first message sent after handshake, then disconnect
 */
#if 0
void T_estPWP_read_msg_other_than_bitfield_after_handshake_disconnects(
    CuTest * tc
)
{
    pwp_connection_functions_t funcs = {
        .recv = __FUNC_peercon_recv,
        .disconnect = __FUNC_disconnect,
    };
    void *hs;
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
    void *hs;
    unsigned char msg[1000], *ptr = msg;
    int ii, ret;

    /* handshake */
    bitstream_write_ubyte(&ptr, 0); /* pn len */
    for (ii=0;ii<8;ii++)
        bitstream_write_ubyte(&ptr, 0);        /*  reserved */
    bitstream_write_string(&ptr, __mock_infohash, 20); /* ih */
    bitstream_write_string(&ptr, __mock_their_peer_id, 20); /* pi */

    /* setup */
    hs = pwp_handshaker_new(__mock_infohash, __mock_my_peer_id);

    /* receive */
    ret = pwp_handshaker_dispatch_from_buffer(hs, msg, 1 + 8 + 20 + 20);
    CuAssertTrue(tc, -1 == ret);
}

void TestPWP_handshake_read_disconnect_if_handshake_has_invalid_protocol_name(
    CuTest * tc
)
{
    void *hs;
    unsigned char msg[1000], *ptr = msg;
    int ii, ret;

    /* handshake */
    bitstream_write_ubyte(&ptr, strlen("Garbage Protocol")); /* pn len */
    bitstream_write_string(&ptr, "Garbage Protocol", strlen("Garbage Protocol")); /* pn */
    for (ii=0;ii<8;ii++)
        bitstream_write_ubyte(&ptr, 0);        /*  reserved */
    bitstream_write_string(&ptr, __mock_infohash, 20); /* ih */
    bitstream_write_string(&ptr, __mock_their_peer_id, 20); /* pi */

    /* setup */
    hs = pwp_handshaker_new(__mock_infohash, __mock_my_peer_id);

    /* receive */
    ret = pwp_handshaker_dispatch_from_buffer(hs, msg, 1 + 
            strlen("Garbage Protocol") + 8 + 20 + 20);
    CuAssertTrue(tc, -1 == ret);
}

void TestPWP_handshake_read_disconnect_if_handshake_has_used_reserved_eight_bytes(
    CuTest * tc
)
{
    void *hs;
    unsigned char msg[1000], *ptr;
    int ii, ret;

    ptr = msg;

    /* handshake */
    bitstream_write_ubyte(&ptr, strlen(PROTOCOL_NAME)); /* pn len */
    bitstream_write_string(&ptr, PROTOCOL_NAME, strlen(PROTOCOL_NAME)); /* pn */
    for (ii=0;ii<7;ii++)
        bitstream_write_ubyte(&ptr, 0);        /*  reserved */
    bitstream_write_ubyte(&ptr, 1);        /*  reserved */
    bitstream_write_string(&ptr, __mock_infohash, 20); /* ih */
    bitstream_write_string(&ptr, __mock_their_peer_id, 20); /* pi */

    /* setup */
    hs = pwp_handshaker_new(__mock_infohash, __mock_my_peer_id);
    
    /* receive */
    ret = pwp_handshaker_dispatch_from_buffer(hs, msg, 1 + 8 + 20 + 20);
    CuAssertTrue(tc, -1 == ret);
}

void TestPWP_handshake_read_disconnect_if_handshake_has_infohash_that_is_same_as_ours(
    CuTest * tc
)
{
    void *hs;
    unsigned char msg[1000], *ptr;
    int ii, ret;

    ptr = msg;

    /* handshake */
    bitstream_write_ubyte(&ptr, strlen(PROTOCOL_NAME)); /* pn len */
    bitstream_write_string(&ptr, PROTOCOL_NAME, strlen(PROTOCOL_NAME)); /* pn */
    bitstream_write_ubyte(&ptr, 1);        /*  reserved */
    for (ii=0;ii<7;ii++)
        bitstream_write_ubyte(&ptr, 0);        /*  reserved */
    bitstream_write_string(&ptr, __mock_infohash, strlen(__mock_infohash)); /* ih */
    bitstream_write_string(&ptr, __mock_my_peer_id, strlen(__mock_my_peer_id)); /* pi */

    /* setup */
    hs = pwp_handshaker_new(__mock_infohash, __mock_my_peer_id);

    /* receive */
    ret = pwp_handshaker_dispatch_from_buffer(hs, msg, 1 + 8 + 20 + 20);
    CuAssertTrue(tc, -1 == ret);
}

/**
 * we obtain the Peer ID from a third party. The Peer ID as per the PWP connection must match.
 */
void TestPWP_handshake_read_disconnect_if_handshake_shows_a_peer_with_different_peer_id_than_expected(
    CuTest * tc
)
{
    void *hs;
    unsigned char msg[1000], *ptr;
    int ii, ret;

    ptr = msg;

    /* handshake */
    bitstream_write_ubyte(&ptr, strlen(PROTOCOL_NAME)); /* pn len */
    bitstream_write_string(&ptr, PROTOCOL_NAME, strlen(PROTOCOL_NAME)); /* pn */
    bitstream_write_ubyte(&ptr, 1);        /*  reserved */
    for (ii=0;ii<7;ii++)
        bitstream_write_ubyte(&ptr, 0);        /*  reserved */
    bitstream_write_string(&ptr, __mock_infohash, 20); /* ih */
    bitstream_write_string(&ptr, "00000000000000000000", 20); /* pi */

    /* setup */
    hs = pwp_handshaker_new(__mock_infohash, __mock_my_peer_id);
    
    /* receive */
    ret = pwp_handshaker_dispatch_from_buffer(hs, msg, 1 + 8 + 20 + 20);
    CuAssertTrue(tc, -1 == ret);
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
    void *hs;
    unsigned char msg[1000], *ptr;
    int ii, ret;

    ptr = msg;

    /* handshake */
    bitstream_write_ubyte(&ptr, strlen(PROTOCOL_NAME)); /* pn len */
    bitstream_write_string(&ptr, PROTOCOL_NAME, strlen(PROTOCOL_NAME)); /* pn */
    bitstream_write_ubyte(&ptr, 1);        /*  reserved */
    for (ii=0;ii<7;ii++)
        bitstream_write_ubyte(&ptr, 0);        /*  reserved */
    bitstream_write_string(&ptr, __mock_infohash, 20); /* ih */
    bitstream_write_string(&ptr, __mock_my_peer_id, 20); /* pi */

    /* setup */
    hs = pwp_handshaker_new(__mock_infohash, __mock_my_peer_id);

    /* receive */
    ret = pwp_handshaker_dispatch_from_buffer(hs, msg, 1 + 8 + 20 + 20);
    CuAssertTrue(tc, -1 == ret);
}

#if 0
/*&
 * If any other peer has already identified itself to the local peer using the same peer ID then:
 * NOTE: The connection MUST be dropped
 */
void T_estPWP_handshake_read_disconnect_if_handshake_shows_a_peer_with_same_peer_id_as_other(
    CuTest * tc
)
{
    CuAssertTrue(tc, 0);
}
#endif


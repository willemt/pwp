
/**
 * @file
 * @brief Manage a connection with a peer
 * @author  Willem Thiart himself@willemthiart.com
 * @version 0.1
 *
 * @section LICENSE
 * Copyright (c) 2011, Willem-Hendrik Thiart
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * The names of its contributors may not be used to endorse or promote
      products derived from this software without specific prior written
      permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL WILLEM-HENDRIK THIART BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* for uint32_t */
#include <stdint.h>

#include <assert.h>

/* for varags */
#include <stdarg.h>

#include "pwp_connection.h"
#include "linked_list_hashmap.h"
#include "linked_list_queue.h"
#include "bitfield.h"
#include "bitstream.h"

#define TRUE 1
#define FALSE 0

#define PROTOCOL_NAME "BitTorrent protocol"
#define INFOKEY_LEN 20
#define BLOCK_SIZE 1 << 14      // 16kb
#define PWP_HANDSHAKE_RESERVERD "\0\0\0\0\0\0\0\0"
#define VERSION_NUM 1000
#define PEER_ID_LEN 20
#define INFO_HASH_LEN 20


// f_f_lywA ==(m€kb€kb (m) ?€ü "pa€kb" :0f=df,xj0
#define bt_pwp_msgtype_to_string(m)\
    PWP_MSGTYPE_CHOKE == (m) ? "CHOKE" :\
    PWP_MSGTYPE_UNCHOKE == (m) ? "UNCHOKE" :\
    PWP_MSGTYPE_INTERESTED == (m) ? "INTERESTED" :\
    PWP_MSGTYPE_UNINTERESTED == (m) ? "UNINTERESTED" :\
    PWP_MSGTYPE_HAVE == (m) ? "HAVE" :\
    PWP_MSGTYPE_BITFIELD == (m) ? "BITFIELD" :\
    PWP_MSGTYPE_REQUEST == (m) ? "REQUEST" :\
    PWP_MSGTYPE_PIECE == (m) ? "PIECE" :\
    PWP_MSGTYPE_CANCEL == (m) ? "CANCEL" : "none"\

//static const bt_pwp_cfg_t __default_cfg = {.max_pending_requests = 10 };

/*  state */
typedef struct
{
    /* this bitfield indicates which pieces the peer has */
    bitfield_t have_bitfield;

    unsigned int flags;

    /* count number of failed connections */
    int failed_connections;

} peer_connection_state_t;

typedef struct
{
//    int timestamp;
    bt_block_t blk;
} request_t;

/*  peer connection */
typedef struct
{
    peer_connection_state_t state;

    /*  requests that we are waiting to get */
    hashmap_t *pendreqs;

    /* requests we are fufilling for the peer */
    linked_list_queue_t *pendpeerreqs;

    int isactive;

    int piece_len;
    int num_pieces;

    /* info of who we are connected to */
    void *peer_udata;

    const char *my_peer_id;
    const char *their_peer_id;
    const char *infohash;

    pwp_connection_functions_t *func;

    void *caller;
} bt_peer_connection_t;

static unsigned long __request_hash(const void *obj)
{
    const bt_block_t *req = obj;

    return req->piece_idx + req->block_len + req->block_byte_offset;
}

static long __request_compare(const void *obj, const void *other)
{
    const bt_block_t *req1 = obj, *req2 = other;

    if (req1->piece_idx == req2->piece_idx &&
        req1->block_len == req2->block_len &&
        req1->block_byte_offset == req2->block_byte_offset)
        return 0;

    return 1;
}

/*----------------------------------------------------------------------------*/

static void __log(bt_peer_connection_t * me, const char *format, ...)
{
    char buffer[1000];

    va_list args;

    if (NULL == me->func || NULL == me->func->log)
        return;

    va_start(args, format);
    (void)vsnprintf(buffer, 1000, format, args);

    me->func->log(me->caller, me->peer_udata, buffer);
}

/*----------------------------------------------------------------------------*/
static void __disconnect(bt_peer_connection_t * me, const char *reason, ...)
{
    char buffer[128];

    va_list args;

    va_start(args, reason);
    (void)vsnprintf(buffer, 128, reason, args);
    if (me->func->disconnect)
    {
       (void)me->func->disconnect(me->caller, me->peer_udata, buffer);
    }
}

static int __read_byte_from_peer(bt_peer_connection_t * me, unsigned char * val)
{
    unsigned char buf[1000], *ptr;
    int len, ret;

    len = 1;
    ptr = buf;

    assert(NULL != me->func);
    assert(NULL != me->func->recv);

    if (0 == (ret = me->func->recv(me->caller, me->peer_udata, (char*)ptr, &len)))
    {
        return 0;
    }

#if 0
    if (0 == me->isr.recv(me->isr_udata, me->peer_udata, ptr, &len))
    {
        assert(FALSE);
        return 0;
    }
#endif

    *val = bitstream_read_ubyte(&ptr);

    return 1;
}

static int __read_uint32_from_peer(bt_peer_connection_t * me, uint32_t * val)
{
    unsigned char buf[1000], *ptr = buf;
    int len = 4;

    assert(NULL != me->func && NULL != me->func->recv);

    if (0 == me->func->recv(me->caller, me->peer_udata, (char*)ptr, &len))
    {
        assert(FALSE);
        return 0;
    }

    *val = bitstream_read_uint32(&ptr);
    return 1;
}

/*----------------------------------------------------------------------------*/
void bt_peerconn_set_active(void *pco, int opt)
{
    bt_peer_connection_t *me = pco;

    me->isactive = opt;
}

static int __send_to_peer(bt_peer_connection_t * me, void *data, const int len)
{
    int ret;

    if (NULL != me->func && NULL != me->func->send)
    {
        ret = me->func->send(me->caller, me->peer_udata, data, len);

        if (0 == ret)
        {
            __disconnect(me, "peer dropped connection");
//            bt_peerconn_set_active(me, 0);
            return 0;
        }
    }

    return 1;
}

/*----------------------------------------------------------------------------*/

void *bt_peerconn_new()
{
    bt_peer_connection_t *me;

    if(!(me = calloc(1, sizeof(bt_peer_connection_t))))
    {
        perror("out of memory");
        exit(0);
    }

    me->state.flags = PC_IM_CHOKING | PC_PEER_CHOKING;
    me->pendreqs = hashmap_new(__request_hash, __request_compare, 11);
    me->pendpeerreqs = llqueue_new();
    return me;
}

void bt_peerconn_release(void* pco)
{
//    bt_peer_connection_t *me = pco;
//    if (me->my_peer_id)
//        free(me->my_peer_id);
}

void *bt_peerconn_get_peer(void *pco)
{
    bt_peer_connection_t *me = pco;

    return me->peer_udata;
}

/**
 * Let the caller know if this peerconnection is working. */
int bt_peerconn_is_active(void *pco)
{
    bt_peer_connection_t *me = pco;

    return me->isactive;
}

void bt_peerconn_set_piece_info(void *pco, int num_pieces, int piece_len)
{
    bt_peer_connection_t *me = pco;

    me->num_pieces = num_pieces;
    bitfield_init(&me->state.have_bitfield, me->num_pieces);
    me->piece_len = piece_len;
}

void bt_peerconn_set_my_peer_id(void *pco, const char *peer_id)
{
    bt_peer_connection_t *me = pco;
    me->my_peer_id = peer_id;
}

void bt_peerconn_set_their_peer_id(void *pco, const char *peer_id)
{
    bt_peer_connection_t *me = pco;
    me->their_peer_id = peer_id;
}

void bt_peerconn_set_infohash(void *pco, const char *infohash)
{
    bt_peer_connection_t *me = pco;
    me->infohash = infohash;
}

void bt_peerconn_set_functions(void *pco, pwp_connection_functions_t* funcs, void* caller)
{
    bt_peer_connection_t *me = pco;

    me->func = funcs;
    me->caller = caller;
}

void bt_peerconn_set_peer(void *pco, void * peer)
{
    bt_peer_connection_t *me = pco;

    me->peer_udata = peer;
}

/*----------------------------------------------------------------------------*/
int bt_peerconn_peer_is_interested(void *pco)
{
    bt_peer_connection_t *me = pco;

    return 0 != (me->state.flags & PC_PEER_INTERESTED);
}

int bt_peerconn_peer_is_choked(void *pco)
{
    bt_peer_connection_t *me = pco;

    return 0 != (me->state.flags & PC_IM_CHOKING);
}


/**
 *
 */
int bt_peerconn_flag_is_set(void *pco, const int flag)
{
    bt_peer_connection_t *me = pco;

    return 0 != (me->state.flags & flag);
}

/**
 * @return whether I am choked or not
 */
int bt_peerconn_im_choked(void *pco)
{
    bt_peer_connection_t *me = pco;

    return 0 != (me->state.flags & PC_PEER_CHOKING);
}

int bt_peerconn_im_interested(void *pco)
{
    bt_peer_connection_t *me = pco;

    return 0 != (me->state.flags & PC_IM_INTERESTED);
}

void bt_peerconn_set_im_interested(void * pco)
{
    bt_peer_connection_t *me = pco;

    if (bt_peerconn_send_statechange(me, PWP_MSGTYPE_INTERESTED))
    {
        me->state.flags |= PC_IM_INTERESTED;
    }
}

void bt_peerconn_choke(void * pco)
{
    bt_peer_connection_t *me = pco;

    me->state.flags |= PC_IM_CHOKING;

    /* expunge requests */
    while (0 < llqueue_count(me->pendpeerreqs))
    {
        free(llqueue_poll(me->pendpeerreqs));
    }

    bt_peerconn_send_statechange(me, PWP_MSGTYPE_CHOKE);
}

void bt_peerconn_unchoke(void * pco)
{
    bt_peer_connection_t *me = pco;

    me->state.flags &= ~PC_IM_CHOKING;
    bt_peerconn_send_statechange(me, PWP_MSGTYPE_UNCHOKE);
}

/*----------------------------------------------------------------------------*/

static void *__get_piece(bt_peer_connection_t * me, const unsigned int piece_idx)
{
    assert(NULL != me->func->getpiece);
    return me->func->getpiece(me->caller, piece_idx);
}

/*----------------------------------------------------------------------------*/

int bt_peerconn_get_download_rate(const void * pco __attribute__((__unused__)))
{
//    const bt_peer_connection_t *me = pco;
    return 0;
}

int bt_peerconn_get_upload_rate(const void * pco __attribute__((__unused__)))
{
//    const bt_peer_connection_t *me = pco;
    return 0;
}

/*----------------------------------------------------------------------------*/

/**
 * unchoke, choke, interested, uninterested,
 * @return non-zero if unsucessful */
int bt_peerconn_send_statechange(void * pco, const unsigned char msg_type)
{
    bt_peer_connection_t *me = pco;
    unsigned char data[5], *ptr = data;

    bitstream_write_uint32(&ptr, 1);
    bitstream_write_ubyte(&ptr, msg_type);

    __log(me, "send,%s", bt_pwp_msgtype_to_string(msg_type));

    if (!__send_to_peer(me, data, 5))
    {
        return 0;
    }

    return 1;
}

/**
 * Send the piece highlighted by this request.
 * @pararm req - the requesting block
 * */
void bt_peerconn_send_piece(void *pco, bt_block_t * req)
{
    bt_peer_connection_t *me = pco;
    unsigned char *data = NULL;
    unsigned char *ptr;
    void *pce;
    unsigned int size;

    assert(NULL != me);
    assert(NULL != me->func->write_block_to_stream);

    /* TODO use a circular buffer for sending */
    /*  get data to send */
    pce = __get_piece(me, req->piece_idx);
    size = 4 * 3 + 1 + req->block_len;
    if (!(data = malloc(sizeof(char)*size)))
    {
        perror("out of memory");
        exit(0);
    }

    ptr = data;
    bitstream_write_uint32(&ptr, size - 4);
    bitstream_write_ubyte(&ptr, PWP_MSGTYPE_PIECE);
    bitstream_write_uint32(&ptr, req->piece_idx);
    bitstream_write_uint32(&ptr, req->block_byte_offset);
    me->func->write_block_to_stream(pce,req,(unsigned char**)&ptr);
    __send_to_peer(me, data, size);

#if 0
    #define BYTES_SENT 1

    for (ii = req->block_len; ii > 0;)
    {
        int len = BYTES_SENT < ii ? BYTES_SENT : ii;

        bt_piece_write_block_to_str(pce,
                                    req->block_byte_offset +
                                    req->block_len - ii, len, block);
        __send_to_peer(me, block, len);
        ii -= len;
    }
#endif

    __log(me, "send,piece,pieceidx=%d block_byte_offset=%d block_len=%d",
          req->piece_idx, req->block_byte_offset, req->block_len);

    free(data);
}

/**
 * Tell peer we have this piece 
 * @return 0 on error, 1 otherwise */
int bt_peerconn_send_have(void *pco, const int piece_idx)
{
    bt_peer_connection_t *me = pco;
    unsigned char data[12], *ptr = data;

/*

Implementer's Note: That is the strict definition, in reality some games may be
played. In particular because peers are extremely unlikely to download pieces
that they already have, a peer may choose not to advertise having a piece to a
peer that already has that piece. At a minimum "HAVE suppression" will result
in a 50% reduction in the number of HAVE messages, this translates to around a
25-35% reduction in protocol overhead. At the same time, it may be worthwhile
to send a HAVE message to a peer that has that piece already since it will be
useful in determining which piece is rare.

*/

    bitstream_write_uint32(&ptr, 5);
    bitstream_write_ubyte(&ptr, PWP_MSGTYPE_HAVE);
    bitstream_write_uint32(&ptr, piece_idx);
    __send_to_peer(me, data, 9);
    __log(me, "send,have,pieceidx=%d", piece_idx);
    return 1;
}

/**
 * Send request for a block */
void bt_peerconn_send_request(void *pco, const bt_block_t * request)
{
    bt_peer_connection_t *me = pco;
    unsigned char data[32], *ptr;

    ptr = data;
    bitstream_write_uint32(&ptr, 13);
    bitstream_write_ubyte(&ptr, PWP_MSGTYPE_REQUEST);
    bitstream_write_uint32(&ptr, request->piece_idx);
    bitstream_write_uint32(&ptr, request->block_byte_offset);
    bitstream_write_uint32(&ptr, request->block_len);
    __send_to_peer(me, data, 17);
    __log(me, "send,request,pieceidx=%d block_byte_offset=%d block_len=%d",
          request->piece_idx, request->block_byte_offset, request->block_len);
}

/**
 * Tell peer we are cancelling the request for this block */
void bt_peerconn_send_cancel(void *pco, bt_block_t * cancel)
{
    bt_peer_connection_t *me = pco;
    unsigned char data[32], *ptr;

    ptr = data;
    bitstream_write_uint32(&ptr, 13);
    bitstream_write_ubyte(&ptr, PWP_MSGTYPE_CANCEL);
    bitstream_write_uint32(&ptr, cancel->piece_idx);
    bitstream_write_uint32(&ptr, cancel->block_byte_offset);
    bitstream_write_uint32(&ptr, cancel->block_len);
    __send_to_peer(me, data, 17);
    __log(me, "send,cancel,pieceidx=%d block_byte_offset=%d block_len=%d",
          cancel->piece_idx, cancel->block_byte_offset, cancel->block_len);
}

static void __write_bitfield_to_stream_from_getpiece_func(bt_peer_connection_t* me,
        unsigned char ** ptr)
{
    int ii;
    unsigned char bits;

    assert(NULL != me->func->getpiece);
    assert(NULL != me->func->piece_is_complete);

    /*  for all pieces set bit = 1 if we have the completed piece */
    for (bits = 0, ii = 0; ii < me->num_pieces; ii++)
    {
        void *pce;

        pce = me->func->getpiece(me->caller, ii);
        bits |= me->func->piece_is_complete(me, pce) << (7 - (ii % 8));
        /* ...up to eight bits, write to byte */
        if (((ii + 1) % 8 == 0) || me->num_pieces - 1 == ii)
        {
            bitstream_write_ubyte(ptr, bits);
            bits = 0;
        }
    }
}

/**
 * Send a bitfield to peer, telling them what we have */
void bt_peerconn_send_bitfield(void *pco)
{
    bt_peer_connection_t *me = pco;
    unsigned char data[1000], *ptr;
    uint32_t size;

    if (!me->func->getpiece)
        return;

    /*
     * Bitfield
     * This message has ID 5 and a variable payload length.The payload is a
     * bitfield representing the pieces that the sender has successfully
     * downloaded, with the high bit in the first byte corresponding to piece in
     * dex 0. If a bit is cleared it is to be interpreted as a missing piece. A
     * peer MUST send this message immediately after the handshake operation,
     * and MAY choose not to send it if it has no pieces at all.This message
     * MUST not be sent at any other time during the communication.
     */

    ptr = data;
    size =
        sizeof(uint32_t) + sizeof(unsigned char) + (me->num_pieces / 8) +
        ((me->num_pieces % 8 == 0) ? 0 : 1);
    bitstream_write_uint32(&ptr, size - sizeof(uint32_t));
    bitstream_write_ubyte(&ptr, PWP_MSGTYPE_BITFIELD);
    __write_bitfield_to_stream_from_getpiece_func(me, &ptr);
#if 0
    /*  ensure padding */
    if (ii % 8 != 0)
    {
//        bitstream_write_ubyte(&ptr, bits);
    }
#endif

    __send_to_peer(me, data, size);
    __log(me, "send,bitfield");

}

/*----------------------------------------------------------------------------*/

/**
 *  Receive handshake from other end
 *  Disconnect on any errors
 *
 *  @return 1 on success; otherwise 0 */
int bt_peerconn_recv_handshake(void *pco, const char *expected_info_hash)
{
    bt_peer_connection_t *me = pco;

    int ii;
    unsigned char name_len;

    /* other peers name protocol name */
    char peer_pname[strlen(PROTOCOL_NAME)];
    char peer_infohash[INFO_HASH_LEN];
    char peer_id[PEER_ID_LEN];
    char peer_reserved[8 + 1];

    __log(me, "got,handshake");

    /* Name Length:
    The unsigned value of the first byte indicates the length of a character
    string containing the protocol name. In BTP/1.0 this number is 19. The
    local peer knows its own protocol name and hence also the length of it.
    If this length is different than the value of this first byte, then the
    connection MUST be dropped. */
    if (0 == __read_byte_from_peer(me, &name_len))
    {
        __disconnect(me, "handshake: invalid name length: '%d'", name_len);
        return 0;
    }

    if (name_len != strlen(PROTOCOL_NAME))
    {
        __disconnect(me, "handshake: invalid protocol name length: '%d'",
                     name_len);
        return FALSE;
    }

    /* Protocol Name:
    This is a character string which MUST contain the exact name of the 
    protocol in ASCII and have the same length as given in the Name Length
    field. The protocol name is used to identify to the local peer which
    version of BTP the remote peer uses.
    If this string is different from the local peers own protocol name, then
    the connection is to be dropped. */
    for (ii = 0; ii < name_len; ii++)
    {
        if (0 == __read_byte_from_peer(me, (unsigned char*)&peer_pname[ii]))
        {
            __disconnect(me, "handshake: invalid protocol name char");
            return 0;
        }
    }
//    strncpy(peer_pname, &handshake[1], name_len);
    if (strncmp(peer_pname, PROTOCOL_NAME, name_len))
    {
        __disconnect(me, "handshake: invalid protocol name: '%s'",
                     peer_pname);
        return FALSE;
    }

    /* Reserved:
    The next 8 bytes in the string are reserved for future extensions and
    should be read without interpretation. */
    for (ii = 0; ii < 8; ii++)
    {
        if (0 == __read_byte_from_peer(me, (unsigned char*)&peer_reserved[ii]) || 
                peer_reserved[ii] != 0)
        {
            __disconnect(me, "handshake: reserved bytes not empty");
            return 0;
        }
    }

    /* Info Hash:
    The next 20 bytes in the string are to be interpreted as a 20-byte SHA1
    of the info key in the metainfo file. Presumably, since both the local
    and the remote peer contacted the tracker as a result of reading in the
    same .torrent file, the local peer will recognize the info hash value and
    will be able to serve the remote peer. If this is not the case, then the
    connection MUST be dropped. This situation can arise if the local peer
    decides to no longer serve the file in question for some reason. The info
    hash may be used to enable the client to serve multiple torrents on the
    same port. */
    assert(NULL != expected_info_hash);
    for (ii = 0; ii < INFO_HASH_LEN; ii++)
    {
        if (0 == __read_byte_from_peer(me, (unsigned char*)&peer_infohash[ii]))
        {
            __disconnect(me, "handshake: infohash bytes not empty");
            return 0;
        }
    }
    /* check info hash matches expected */
    if (strncmp(peer_infohash, expected_info_hash, 20))
    {
        __log(me, "handshake: invalid infohash: '%s' vs '%s'", peer_infohash,
               peer_infohash);
        __disconnect(me, "handshake: infohash bytes not empty");
        return 0;
    }

    /* At this stage, if the connection has not been dropped, then the local
    peer MUST send its own handshake back, which includes the last step: */

    /* Peer ID:
    The last 20 bytes of the handshake are to be interpreted as the
    self-designated name of the peer. The local peer must use this name to id
    entify the connection hereafter. Thus, if this name matches the local
    peers own ID name, the connection MUST be dropped. Also, if any other
    peer has already identified itself to the local peer using that same peer
    ID, the connection MUST be dropped. */
    //assert(NULL != me->their_peer_id);
    for (ii = 0; ii < PEER_ID_LEN; ii++)
    {
        if (0 == __read_byte_from_peer(me, (unsigned char*)&peer_id[ii]))
        {
            __disconnect(me, "handshake: peer_id length invalid");
            return 0;
        }
    }

    /* disconnect if peer's ID is the same as ours */
    if (!strncmp(peer_id,me->my_peer_id,20))
    {
        __disconnect(me, "handshake: peer_id same as ours (us: %s them: %.*s)",
                me->my_peer_id, 20, peer_id);
        return 0;
    }

    __log(me, "read,handshake,me:%.*s,them:%.*s", 20, me->my_peer_id, 20, peer_id);
    return TRUE;
}

/**
 * Send the handshake
 *
 * Steps taken:
 * 1. send handshake
 * 2. receive handshake
 * 3. show interest
 *
 * @return 0 on failure; 1 otherwise */
int bt_peerconn_send_handshake(void *pco)
{
    bt_peer_connection_t *me = pco;
    char buf[1024], *protocol_name = PROTOCOL_NAME, *ptr;
    int size, ii;

    assert(NULL != me->infohash);
    assert(NULL != me->my_peer_id);

//    sprintf(buf, "%c%s" PWP_PC_HANDSHAKE_RESERVERD "%s%s",
//            strlen(protocol_name), protocol_name, infohash, peerid);

    ptr = buf;

    /* protocol name length */
    bitstream_write_ubyte((unsigned char**)&ptr, strlen(protocol_name));

    /* protocol name */
    bitstream_write_string((unsigned char**)&ptr, protocol_name, strlen(protocol_name));

    /* reserved characters */
    for (ii=0;ii<8;ii++)
        bitstream_write_ubyte((unsigned char**)&ptr, 0);

    /* infohash */
    bitstream_write_string((unsigned char**)&ptr, me->infohash, 20);

    /* peerid */
    bitstream_write_string((unsigned char**)&ptr, me->my_peer_id, 20);

    /* calculate total handshake size */
    size = 1 + strlen(protocol_name) + 8 + 20 + 20;


    if (0 == __send_to_peer(me, buf, size))
    {
        __log(me, "send,handshake,fail");
        bt_peerconn_set_active(me, 0);
        return 0;
    }

    me->state.flags |= PC_HANDSHAKE_SENT;

    __log(me, "send,handshake,mypeerid:%s",me->my_peer_id);

    return 1;
}

void bt_peerconn_set_state(void *pco, const int state)
{
    bt_peer_connection_t *me = pco;

    me->state.flags = state;
}

int bt_peerconn_get_state(void *pco)
{
    bt_peer_connection_t *me = pco;

    return me->state.flags;
}

/**
 * Peer told us they have this piece.
 * @return 0 on error, 1 otherwise */
int bt_peerconn_mark_peer_has_piece(void *pco, const int piece_idx)
{
    bt_peer_connection_t *me = pco;
    int bf_len;

    /* make sure piece is within bitfield length */
    bf_len = bitfield_get_length(&me->state.have_bitfield);
    if (bf_len <= piece_idx || piece_idx < 0)
    {
        __disconnect(me, "piece idx fits outside of boundary");
        return 0;
    }

    /* remember that they have this piece */
    bitfield_mark(&me->state.have_bitfield, piece_idx);

    return 1;
}

/*----------------------------------------------------------------------------*/

/**
 * Respond to a peer's request for a block
 * @return 0 on error, 1 otherwise */
int bt_peerconn_process_request(void * pco, bt_block_t * request)
{
    bt_peer_connection_t *me = pco;
    void *pce;

    /* We're choking - we aren't obligated to respond to this request */
    if (bt_peerconn_peer_is_choked(me))
    {
        return 0;
    }

    /*  ensure we have correct piece_idx */
    if (me->num_pieces < request->piece_idx)
    {
        __disconnect(me, "requested piece %d has invalid idx",
                     request->piece_idx);
        return 0;
    }

    /*  ensure that we have this piece */
    if (!(pce = __get_piece(me, request->piece_idx)))
    {
        __disconnect(me, "requested piece %d is not available",
                     request->piece_idx);
        return 0;
    }

    /* ensure that the peer needs this piece
     * If the peer doesn't need the piece then that means the peer is
     * potentially invalid */
    if (bt_peerconn_peer_has_piece(me, request->piece_idx))
    {
        __disconnect(me, "peer requested pce%d which they confirmed they had",
                     request->piece_idx);
        return 0;
    }

    /* ensure that block request length is valid  */
    if (request->block_len == 0 || me->piece_len < request->block_byte_offset + request->block_len)
    {
        __disconnect(me, "invalid block request"); 
        return 0;
    }

    /* ensure that we have completed this piece.
     * The peer should know if we have completed this piece or not, so
     * asking for it is an indicator of a invalid peer. */
    assert(NULL != me->func->piece_is_complete);
    if (0 == me->func->piece_is_complete(me, pce))
    {
        __disconnect(me, "requested piece %d is not completed",
                     request->piece_idx);
        return 0;
    }

    /* append block to our pending request queue */
    /* don't append the block twice */
    if (!llqueue_get_item_via_cmpfunction(
                me->pendpeerreqs,request,(void*)__request_compare))
    {
        bt_block_t* blk;

        blk = malloc(sizeof(bt_block_t));
        memcpy(blk,request, sizeof(bt_block_t));
        llqueue_offer(me->pendpeerreqs,blk);
    }

    return 1;
}

static int __recv_request(bt_peer_connection_t * me,
                  const int payload_len,
//                  int (*fn_read_byte) (bt_peer_connection_t *, unsigned char *),
                  int (*fn_read_uint32) (bt_peer_connection_t *, uint32_t *))
{
    bt_block_t request;

    /* check that the client doesn't request when they are choked */
    if (bt_peerconn_peer_is_choked(me))
    {
        __disconnect(me, "peer requested when they were choked");
        return 0;
    }

    /*  ensure payload length is correct */
    if (payload_len != 12)
    {
        __disconnect(me, "invalid payload size for request: %d", payload_len);
        return 0;
    }

    /* ensure request indices are valid */
    if (0 == fn_read_uint32(me, (uint32_t*)&request.piece_idx) ||
        0 == fn_read_uint32(me, (uint32_t*)&request.block_byte_offset) ||
        0 == fn_read_uint32(me, (uint32_t*)&request.block_len))
    {
        return 0;
    }

    __log(me, "read,request,pieceidx=%d offset=%d length=%d",
          request.piece_idx, request.block_byte_offset, request.block_len);

    return bt_peerconn_process_request(me, &request);
}

/*
6.3.10 Piece

This message has ID 7 and a variable length payload. The payload holds 2
integers indicating from which piece and with what offset the block data in the
3rd member is derived. Note, the data length is implicit and can be calculated
by subtracting 9 from the total message length.

The payload has the following structure:

-------------------------------------------
| Piece Index | Block Offset | Block Data |
-------------------------------------------
*/
static int __recv_piece(bt_peer_connection_t * me,
                        const int msg_len,
                        int (*fn_read_byte) (bt_peer_connection_t *,
                                             unsigned char *),
                        int (*fn_read_uint32) (bt_peer_connection_t *,
                                               uint32_t *))
{
    int ii;
    unsigned char *block_data;
    bt_block_t request;
    request_t *req;

    if (0 == fn_read_uint32(me, (uint32_t*)&request.piece_idx))
        return 0;
    if (0 == fn_read_uint32(me, (uint32_t*)&request.block_byte_offset))
        return 0;

    /* compensate for request header length */
    request.block_len = msg_len - 9;
    
    /* remove pending request */
    req = hashmap_remove(me->pendreqs, &request);

    /* ensure that the peer is sending us a piece we requested */
    if (!req)
    {
        __disconnect(me,
                     "err: received a block we did not request: %d %d %d\n",
                     request.piece_idx, request.block_byte_offset,
                     request.block_len);
        return 0;
    }

    /* read block data */
    block_data = malloc(sizeof(char) * request.block_len);
    for (ii = 0; ii < request.block_len; ii++)
    {
        if (0 == fn_read_byte(me, &block_data[ii]))
            return 0;
    }

    __log(me, "read,piece,pieceidx=%d offset=%d length=%d",
          request.piece_idx, request.block_byte_offset, request.block_len);

    /* Insert block into database.
     * Should result in the caller having other peerconns send HAVE messages */
    me->func->pushblock(me->caller, me->peer_udata, &request, block_data);

    /*  there should have been a request polled */
    assert(NULL != req);
    free(req);
    free(block_data);

    return 1;
}

/**
 * Read the bits of a bitfield and mark the peer as having those pieces */
static int __mark_peer_as_have_from_have_payload(
        bt_peer_connection_t* me,
        int payload_len,
        int (*fn_read_byte) (bt_peer_connection_t *, unsigned char *))
{
    int ii, piece_idx;

    for (piece_idx = 0, ii = 0; ii < payload_len; ii++)
    {
        unsigned char cur_byte;
        int bit;

        if (0 == fn_read_byte(me, &cur_byte))
        {
            __disconnect(me, "bitfield, can't read expected byte");
            return 0;
        }

        for (bit = 0; bit < 8; bit++)
        {
            /*  bit is ON */
            if (((cur_byte << bit) >> 7) & 1)
            {
                if (me->num_pieces <= piece_idx)
                {
                    __disconnect(me, "bitfield has more than expected");
                }

                bt_peerconn_mark_peer_has_piece(me, piece_idx);
            }

            piece_idx += 1;
        }
    }

    return 1;
}

static int __recv_bitfield(bt_peer_connection_t * me, int payload_len,
                          int (*fn_read_byte) (bt_peer_connection_t *, unsigned char *))
{
    char *str;

    if (payload_len * 8 < me->num_pieces)
    {
        __disconnect(me, "payload length less than npieces");
        return 0;
    }

    if (0 == __mark_peer_as_have_from_have_payload(me,payload_len,fn_read_byte))
    {
        return 0;
    }

    str = bitfield_str(&me->state.have_bitfield);
    __log(me, "read,bitfield,%s", str);
    free(str);

    me->state.flags |= PC_BITFIELD_RECEIVED;

    return 1;
}

/*
 *
 * -----------------------------------------
 * | Message Length | Message ID | Payload |
 * -----------------------------------------
 *
 * All integer members in PWP messages are encoded as a 4-byte big-endian
 * number. Furthermore, all index and offset members in PWP messages are zero-
 * based.
 *
 * @return 1 on sucess; 0 otherwise
 */
static int __process_msg(void *pco,
                         int (*fn_read_uint32) (bt_peer_connection_t *, uint32_t *),
                         int (*fn_read_byte) (bt_peer_connection_t *, unsigned char *))
{
    bt_peer_connection_t *me = pco;
    uint32_t msg_len;

    assert(NULL != fn_read_uint32);
    assert(NULL != fn_read_byte);

    if (0 == fn_read_uint32(me, &msg_len))
    {
        return 0;
    }

    /* keep alive message */
    if (0 == msg_len)
    {
        /* TODO Implement timeout function */
        __log(me, "read,keep alive");
    }
    /* payload */
    else if (1 <= msg_len)
    {
        unsigned char msg_id;
        uint32_t payload_len;

        payload_len = msg_len - 1;
        if (0 == fn_read_byte(me, &msg_id))
        {
            return 0;
        }

#if 0 /* this has been removed as bitfields are optional */
        /*  make sure bitfield is received after handshake */
        if (!(me->state.flags & PC_BITFIELD_RECEIVED))
        {
            if (msg_id != PWP_MSGTYPE_BITFIELD)
            {
                __disconnect(me, "unexpected message; expected bitfield");
            }
            else if (0 == __recv_bitfield(me, payload_len,fn_read_byte))
            {
                __disconnect(me, "bad bitfield");
                return 0;
            }

            return 1;
        }
#endif

        switch (msg_id)
        {
        case PWP_MSGTYPE_CHOKE:
            {
                request_t *req;
                hashmap_iterator_t iter;

                me->state.flags |= PC_PEER_CHOKING;
                __log(me, "read,choke");


                /*  clear pending requests */
                for (hashmap_iterator(me->pendreqs, &iter);
                     (req = hashmap_iterator_next(me->pendreqs, &iter));)
                {
                    req = hashmap_remove(me->pendreqs, req);
                    free(req);
                }
            }
            break;
        case PWP_MSGTYPE_UNCHOKE:
            me->state.flags &= ~PC_PEER_CHOKING;
            __log(me, "read,unchoke");
            break;
        case PWP_MSGTYPE_INTERESTED:
            me->state.flags |= PC_PEER_INTERESTED;
#if 0
            if (bt_peerconn_peer_is_choked(me))
            {
                bt_peerconn_unchoke(me);
            }
#endif
            __log(me, "read,interested");
            break;
        case PWP_MSGTYPE_UNINTERESTED:
            me->state.flags &= ~PC_PEER_INTERESTED;
            __log(me, "read,uninterested");
            break;
        case PWP_MSGTYPE_HAVE:
            {
                uint32_t piece_idx;

                assert(payload_len == 4);

                if (0 == fn_read_uint32(me, &piece_idx))
                {
                    return 0;
                }

                if (1 == bt_peerconn_mark_peer_has_piece(me, piece_idx))
                {
//                    assert(bt_peerconn_peer_has_piece(me, piece_idx));
                }

//                bitfield_mark(&me->state.have_bitfield, piece_idx);
                __log(me, "read,have,pieceidx=%d", piece_idx);

                /* tell the peer we are intested if we don't have this piece */
                if (!__get_piece(me, piece_idx))
                {
                    bt_peerconn_set_im_interested(me);
                }
                
            }
            break;
        case PWP_MSGTYPE_BITFIELD:
            /* A peer MUST send this message immediately after the handshake
             * operation, and MAY choose not to send it if it has no pieces at
             * all. This message MUST not be sent at any other time during the
             * communication. */
            if (0 == __recv_bitfield(me, payload_len,fn_read_byte))
            {
                __disconnect(me, "bad bitfield");
                return 0;
            }
            break;
        case PWP_MSGTYPE_REQUEST:
            return __recv_request(me, payload_len, fn_read_uint32);
        case PWP_MSGTYPE_PIECE:
            return __recv_piece(me, msg_len, fn_read_byte, fn_read_uint32);
        case PWP_MSGTYPE_CANCEL:
            /* ---------------------------------------------
             * | Piece Index | Block Offset | Block Length |
             * ---------------------------------------------*/
            {
                bt_block_t request;
                bt_block_t *removed;

                assert(payload_len == 12);

                /* check request is valid */
                if (0 == fn_read_uint32(me, (uint32_t*)&request.piece_idx))
                    return 0;
                if (0 == fn_read_uint32(me, (uint32_t*)&request.block_byte_offset))
                    return 0;
                if (0 == fn_read_uint32(me, (uint32_t*)&request.block_len))
                    return 0;

                __log(me, "read,cancel,pieceidx=%d offset=%d length=%d",
                      request.piece_idx, request.block_byte_offset,
                      request.block_len);

                /* remove from linked list queue */
                removed = llqueue_remove_item_via_cmpfunction(
                        me->pendpeerreqs, &request, (void*)__request_compare);

                free(removed);

//                FIXME_STUB;
//                queue_remove(peer->request_queue);
            }
            break;
        }
    }

    return 1;
}

/**
 * fit the request in the piece size so that we don't break anything */
static void __request_fit(bt_block_t * request, const unsigned int piece_len)
{
    if (piece_len < request->block_byte_offset + request->block_len)
    {
        request->block_len =
            request->block_byte_offset + request->block_len - piece_len;
    }
}

/**
 * read current message from receiving end
 * @return 1 on sucess; 0 otherwise */
int bt_peerconn_process_msg(void *pco)
{
    bt_peer_connection_t *me = pco;

    /* ensure that we are connected */
    if (!bt_peerconn_flag_is_set(me, PC_CONNECTED))
    {
        return -1;
    }

    /* ensure we receive the handshake next */
    if (!bt_peerconn_flag_is_set(me,PC_HANDSHAKE_RECEIVED))
    {
        if (1 == bt_peerconn_recv_handshake(pco, me->infohash))
        {
            me->state.flags |= PC_HANDSHAKE_RECEIVED;

            __log(me, "[connection],gothandshake,%.*s", 20, me->their_peer_id);

            /*  send handshake */
            if (!bt_peerconn_flag_is_set(me,PC_HANDSHAKE_SENT))
            {
                bt_peerconn_send_handshake(me);
            }

            bt_peerconn_send_bitfield(me);

            return 1;
        }
    }

    /* business as usual messages received below: */
    return __process_msg(pco, __read_uint32_from_peer, __read_byte_from_peer);
}

/**
 * @return number of requests we required from the peer */
int bt_peerconn_get_npending_requests(const void* pco)
{
    const bt_peer_connection_t * me = pco;
    return hashmap_count(me->pendreqs);
}

/**
 * @return number of requests we required from the peer */
int bt_peerconn_get_npending_peer_requests(const void* pco)
{
    const bt_peer_connection_t * me = pco;
    return llqueue_count(me->pendpeerreqs);
}

/**
 * pend a block request */
void bt_peerconn_request_block(void * pco, bt_block_t * blk)
{
    bt_peer_connection_t * me = pco;
    request_t *req;

    __log(me, "request block: %d %d %d",
           blk->piece_idx, blk->block_byte_offset, blk->block_len);

    req = malloc(sizeof(request_t));
    __request_fit(blk, me->piece_len);
    bt_peerconn_send_request(me, blk);
    memcpy(&req->blk, blk, sizeof(bt_block_t));

    /* remember that we requested it */
    hashmap_put(me->pendreqs, &req->blk, req);
}

static void __make_request(bt_peer_connection_t * me)
{
    bt_block_t blk;

    if (0 == me->func->pollblock(me->caller, &me->state.have_bitfield, &blk))
    {
        bt_peerconn_request_block(me, &blk);
    }
}

/**
 * Tells the peerconn that the connection worked.
 * We are now connected */
void bt_peerconn_connected(void* pco)
{
    bt_peer_connection_t *me = pco;

    __log(me, "[connected],%.*s", 20, me->their_peer_id);

    assert(!bt_peerconn_flag_is_set(me,PC_HANDSHAKE_SENT));

    me->state.flags |= PC_CONNECTED;

    /* send handshake */
    bt_peerconn_send_handshake(me);

    //bt_peerconn_recv_handshake(me, me->infohash);
}

/**
 * Tells the peerconn that the connection failed */
void bt_peerconn_connect_failed(void *pco)
{
    bt_peer_connection_t *me = pco;

    /* check if we haven't failed before too many times
     * we do not want to stay in an end-less loop */
    me->state.failed_connections += 1;

    if (5 < me->state.failed_connections)
    {
        me->state.flags = PC_UNCONTACTABLE_PEER;
    }
    assert(0);
}

void bt_peerconn_step(void *pco)
{
    bt_peer_connection_t *me;

    me = pco;

    if (bt_peerconn_flag_is_set(me, PC_UNCONTACTABLE_PEER))
        return;

    /*  if the peer is not connected and is contactable */
    if (!bt_peerconn_flag_is_set(me, PC_CONNECTED))
    {
        assert(NULL != me->func);
        assert(NULL != me->func->connect);

        /* connect to this peer  */
        //__log(me, "[connecting],%.*s", 20, me->their_peer_id);
        me->func->connect(me->caller, me, me->peer_udata);

        return;
    }
    /* don't do any processing unless we have received a handshake */
    else if (!bt_peerconn_flag_is_set(me, PC_HANDSHAKE_RECEIVED))
    {
        return;
    }

    /* send one pending request to the peer */
    if (0 < llqueue_count(me->pendpeerreqs))
    {
        bt_block_t* blk;

        blk = llqueue_poll(me->pendpeerreqs);
        bt_peerconn_send_piece(me, blk);
        free(blk);
    }

    /* unchoke interested peer */
    if (bt_peerconn_peer_is_interested(me))
    {
        if (bt_peerconn_peer_is_choked(me))
        {
            bt_peerconn_unchoke(me);
        }
    }

    /* request piece */
    if (bt_peerconn_im_interested(me))
    {
        int ii, end;

        if (bt_peerconn_im_choked(me))
        {
            __log(me,"peer is choking us %lx", (long unsigned int) pco);
            return;
        }

        /*  max out pipeline */
        end = 10 - bt_peerconn_get_npending_requests(me);

        for (ii = 0; ii < end; ii++)
        {
            __make_request(me);
        }
    }
    else
    {
        bt_peerconn_set_im_interested(me);
    }
}

/** 
 *  @return 1 if the peer has this piece; otherwise 0 */
int bt_peerconn_peer_has_piece(void *pco, const int piece_idx)
{
    bt_peer_connection_t *me = pco;

    return bitfield_is_marked(&me->state.have_bitfield, piece_idx);
}

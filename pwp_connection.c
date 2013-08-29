
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

#include "bitfield.h"
#include "pwp_connection.h"
#include "pwp_local.h"
#include "linked_list_hashmap.h"
#include "linked_list_queue.h"
#include "bitstream.h"

#define TRUE 1
#define FALSE 0

#define pwp_msgtype_to_string(m)\
    PWP_MSGTYPE_CHOKE == (m) ? "CHOKE" :\
    PWP_MSGTYPE_UNCHOKE == (m) ? "UNCHOKE" :\
    PWP_MSGTYPE_INTERESTED == (m) ? "INTERESTED" :\
    PWP_MSGTYPE_UNINTERESTED == (m) ? "UNINTERESTED" :\
    PWP_MSGTYPE_HAVE == (m) ? "HAVE" :\
    PWP_MSGTYPE_BITFIELD == (m) ? "BITFIELD" :\
    PWP_MSGTYPE_REQUEST == (m) ? "REQUEST" :\
    PWP_MSGTYPE_PIECE == (m) ? "PIECE" :\
    PWP_MSGTYPE_CANCEL == (m) ? "CANCEL" : "none"\

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
} pwp_connection_t;

/**
 * Flip endianess
 **/
static uint32_t fe(uint32_t i)
{
    uint32_t o;
    unsigned char *c = (unsigned char *)&i;
    unsigned char *p = (unsigned char *)&o;

    p[0] = c[3];
    p[1] = c[2];
    p[2] = c[1];
    p[3] = c[0];

    return o;
}

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

static void __log(pwp_connection_t * me, const char *format, ...)
{
    char buffer[1000];

    va_list args;

    if (NULL == me->func || NULL == me->func->log)
        return;

    va_start(args, format);
    (void)vsnprintf(buffer, 1000, format, args);

    me->func->log(me->caller, me->peer_udata, buffer);
}

static void __disconnect(pwp_connection_t * me, const char *reason, ...)
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

void pwp_conn_set_active(void *pco, int opt)
{
    pwp_connection_t *me = pco;

    me->isactive = opt;
}

static int __send_to_peer(pwp_connection_t * me, void *data, const int len)
{
    int ret;

    if (NULL != me->func && NULL != me->func->send)
    {
        ret = me->func->send(me->caller, me->peer_udata, data, len);

        if (0 == ret)
        {
            __disconnect(me, "peer dropped connection");
//            pwp_conn_set_active(me, 0);
            return 0;
        }
    }

    return 1;
}

void *pwp_conn_get_peer(void *pco)
{
    pwp_connection_t *me = pco;

    return me->peer_udata;
}

void pwp_conn_set_peer(void *pco, void * peer)
{
    pwp_connection_t *me = pco;

    me->peer_udata = peer;
}

void *pwp_conn_new()
{
    pwp_connection_t *me;

    if(!(me = calloc(1, sizeof(pwp_connection_t))))
    {
        perror("out of memory");
        exit(0);
    }

    me->state.flags = PC_IM_CHOKING | PC_PEER_CHOKING;
    me->pendreqs = hashmap_new(__request_hash, __request_compare, 11);
    me->pendpeerreqs = llqueue_new();
    return me;
}

static void __expunge_their_pending_reqs(pwp_connection_t* me)
{
    while (0 < llqueue_count(me->pendpeerreqs))
    {
        free(llqueue_poll(me->pendpeerreqs));
    }
}

static void __expunge_my_pending_reqs(pwp_connection_t* me)
{
    request_t *req;
    hashmap_iterator_t iter;

    for (hashmap_iterator(me->pendreqs, &iter);
         (req = hashmap_iterator_next(me->pendreqs, &iter));)
    {
        req = hashmap_remove(me->pendreqs, req);
        free(req);
    }
}

void pwp_conn_release(void* pco)
{
    pwp_connection_t *me = pco;

    __expunge_their_pending_reqs(me);
    __expunge_my_pending_reqs(me);
    hashmap_free(me->pendreqs);
    llqueue_free(me->pendpeerreqs);
    free(pco);
}

/**
 * Let the caller know if this peerconnection is working. */
int pwp_conn_is_active(void *pco)
{
    pwp_connection_t *me = pco;

    return me->isactive;
}

void pwp_conn_set_piece_info(void *pco, int num_pieces, int piece_len)
{
    pwp_connection_t *me = pco;

    me->num_pieces = num_pieces;
    bitfield_init(&me->state.have_bitfield, me->num_pieces);
    me->piece_len = piece_len;
}

void pwp_conn_set_my_peer_id(void *pco, const char *peer_id)
{
    pwp_connection_t *me = pco;
    me->my_peer_id = peer_id;
}

void pwp_conn_set_their_peer_id(void *pco, const char *peer_id)
{
    pwp_connection_t *me = pco;
    me->their_peer_id = peer_id;
}

void pwp_conn_set_infohash(void *pco, const char *infohash)
{
    pwp_connection_t *me = pco;
    me->infohash = infohash;
}

void pwp_conn_set_functions(void *pco, pwp_connection_functions_t* funcs, void* caller)
{
    pwp_connection_t *me = pco;

    me->func = funcs;
    me->caller = caller;
}

int pwp_conn_peer_is_interested(void *pco)
{
    pwp_connection_t *me = pco;

    return 0 != (me->state.flags & PC_PEER_INTERESTED);
}

int pwp_conn_peer_is_choked(void *pco)
{
    pwp_connection_t *me = pco;

    return 0 != (me->state.flags & PC_IM_CHOKING);
}

/**
 *
 */
int pwp_conn_flag_is_set(void *pco, const int flag)
{
    pwp_connection_t *me = pco;

    return 0 != (me->state.flags & flag);
}

/**
 * @return whether I am choked or not
 */
int pwp_conn_im_choked(void *pco)
{
    pwp_connection_t *me = pco;

    return 0 != (me->state.flags & PC_PEER_CHOKING);
}

int pwp_conn_im_interested(void *pco)
{
    pwp_connection_t *me = pco;

    return 0 != (me->state.flags & PC_IM_INTERESTED);
}

void pwp_conn_set_im_interested(void * pco)
{
    pwp_connection_t *me = pco;

    if (pwp_conn_send_statechange(me, PWP_MSGTYPE_INTERESTED))
    {
        me->state.flags |= PC_IM_INTERESTED;
    }
}

void pwp_conn_choke_peer(void * pco)
{
    pwp_connection_t *me = pco;

    me->state.flags |= PC_IM_CHOKING;

    __expunge_their_pending_reqs(me);

    pwp_conn_send_statechange(me, PWP_MSGTYPE_CHOKE);
}

void pwp_conn_unchoke_peer(void * pco)
{
    pwp_connection_t *me = pco;

    me->state.flags &= ~PC_IM_CHOKING;
    pwp_conn_send_statechange(me, PWP_MSGTYPE_UNCHOKE);
}

static void *__get_piece(pwp_connection_t * me, const unsigned int piece_idx)
{
    assert(NULL != me->func->getpiece);
    return me->func->getpiece(me->caller, piece_idx);
}

#if 0
int pwp_conn_get_download_rate(const void * pco __attribute__((__unused__)))
{
//    const pwp_connection_t *me = pco;
    return 0;
}

int pwp_conn_get_upload_rate(const void * pco __attribute__((__unused__)))
{
//    const pwp_connection_t *me = pco;
    return 0;
}
#endif

/**
 * unchoke, choke, interested, uninterested,
 * @return non-zero if unsucessful */
int pwp_conn_send_statechange(void * pco, const unsigned char msg_type)
{
    pwp_connection_t *me = pco;
    unsigned char data[5], *ptr = data;

    bitstream_write_uint32(&ptr, fe(1));
    bitstream_write_ubyte(&ptr, msg_type);

    __log(me, "send,%s", pwp_msgtype_to_string(msg_type));

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
void pwp_conn_send_piece(void *pco, bt_block_t * req)
{
    pwp_connection_t *me = pco;
    unsigned char *data = NULL;
    unsigned char *ptr;
    void *pce;
    unsigned int size;

    assert(NULL != me);
    assert(NULL != me->func->write_block_to_stream);

    /*  get data to send */
    pce = __get_piece(me, req->piece_idx);

    /* prepare buf */
    size = 4 + 1 + 4 + 4 + req->block_len;
    if (!(data = malloc(size)))
    {
        perror("out of memory");
        exit(0);
    }

    ptr = data;
    bitstream_write_uint32(&ptr, fe(size - 4));
    bitstream_write_ubyte(&ptr, PWP_MSGTYPE_PIECE);
    bitstream_write_uint32(&ptr, fe(req->piece_idx));
    bitstream_write_uint32(&ptr, fe(req->block_byte_offset));
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

    __log(me, "send,piece,piece_idx=%d block_byte_offset=%d block_len=%d",
          req->piece_idx, req->block_byte_offset, req->block_len);

    free(data);
}

/**
 * Tell peer we have this piece 
 * @return 0 on error, 1 otherwise */
int pwp_conn_send_have(void *pco, const int piece_idx)
{
    pwp_connection_t *me = pco;
    unsigned char data[12], *ptr = data;

    bitstream_write_uint32(&ptr, fe(5));
    bitstream_write_ubyte(&ptr, PWP_MSGTYPE_HAVE);
    bitstream_write_uint32(&ptr, fe(piece_idx));
    __send_to_peer(me, data, 5+4);
    __log(me, "send,have,piece_idx=%d", piece_idx);
    return 1;
}

/**
 * Send request for a block */
void pwp_conn_send_request(void *pco, const bt_block_t * request)
{
    pwp_connection_t *me = pco;
    unsigned char data[32], *ptr;

    ptr = data;
    bitstream_write_uint32(&ptr, fe(13));
    bitstream_write_ubyte(&ptr, PWP_MSGTYPE_REQUEST);
    bitstream_write_uint32(&ptr, fe(request->piece_idx));
    bitstream_write_uint32(&ptr, fe(request->block_byte_offset));
    bitstream_write_uint32(&ptr, fe(request->block_len));
    __send_to_peer(me, data, 13+4);
    __log(me, "send,request,piece_idx=%d block_byte_offset=%d block_len=%d",
          request->piece_idx, request->block_byte_offset, request->block_len);
}

/**
 * Tell peer we are cancelling the request for this block */
void pwp_conn_send_cancel(void *pco, bt_block_t * cancel)
{
    pwp_connection_t *me = pco;
    unsigned char data[32], *ptr;

    ptr = data;
    bitstream_write_uint32(&ptr, fe(13));
    bitstream_write_ubyte(&ptr, PWP_MSGTYPE_CANCEL);
    bitstream_write_uint32(&ptr, fe(cancel->piece_idx));
    bitstream_write_uint32(&ptr, fe(cancel->block_byte_offset));
    bitstream_write_uint32(&ptr, fe(cancel->block_len));
    __send_to_peer(me, data, 17);
    __log(me, "send,cancel,piece_idx=%d block_byte_offset=%d block_len=%d",
          cancel->piece_idx, cancel->block_byte_offset, cancel->block_len);
}

static void __write_bitfield_to_stream_from_getpiece_func(pwp_connection_t* me,
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
        bits |= me->func->piece_is_complete(me->caller, pce) << (7 - (ii % 8));
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
void pwp_conn_send_bitfield(void *pco)
{
    pwp_connection_t *me = pco;
    unsigned char data[1000], *ptr;
    uint32_t size;

    if (!me->func->getpiece)
        return;

    ptr = data;
    size =
        sizeof(uint32_t) + sizeof(unsigned char) + (me->num_pieces / 8) +
        ((me->num_pieces % 8 == 0) ? 0 : 1);
    bitstream_write_uint32(&ptr, fe(size - sizeof(uint32_t)));
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

/**
 * Send the handshake
 *
 * Steps taken:
 * 1. send handshake
 * 2. receive handshake
 * 3. show interest
 *
 * @return 0 on failure; 1 otherwise */
int pwp_conn_send_handshake(void *pco)
{
    pwp_connection_t *me = pco;
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
        pwp_conn_set_active(me, 0);
        return 0;
    }

    me->state.flags |= PC_HANDSHAKE_SENT;

    __log(me, "send,handshake,mypeerid:%s",me->my_peer_id);

    return 1;
}

void pwp_conn_set_state(void *pco, const int state)
{
    pwp_connection_t *me = pco;

    me->state.flags = state;
}

int pwp_conn_get_state(void *pco)
{
    pwp_connection_t *me = pco;

    return me->state.flags;
}

/**
 * Peer told us they have this piece.
 * @return 0 on error, 1 otherwise */
int pwp_conn_mark_peer_has_piece(void *pco, const int piece_idx)
{
    pwp_connection_t *me = pco;
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
 * @return number of requests we required from the peer */
int pwp_conn_get_npending_requests(const void* pco)
{
    const pwp_connection_t * me = pco;
    return hashmap_count(me->pendreqs);
}

/**
 * @return number of requests we required from the peer */
int pwp_conn_get_npending_peer_requests(const void* pco)
{
    const pwp_connection_t * me = pco;
    return llqueue_count(me->pendpeerreqs);
}

/**
 * pend a block request */
void pwp_conn_request_block_from_peer(void * pco, bt_block_t * blk)
{
    pwp_connection_t * me = pco;
    request_t *req;

#if 0 /*  debugging */
    __log(me, "request block: %d %d %d",
           blk->piece_idx, blk->block_byte_offset, blk->block_len);
#endif

    __request_fit(blk, me->piece_len);
    pwp_conn_send_request(me, blk);

    /* remember that we requested it */
    req = malloc(sizeof(request_t));
    memcpy(&req->blk, blk, sizeof(bt_block_t));
    hashmap_put(me->pendreqs, &req->blk, req);
}

static void __make_request(pwp_connection_t * me)
{
    bt_block_t blk;

    if (0 == me->func->pollblock(me->caller, &me->state.have_bitfield, &blk))
    {
        pwp_conn_request_block_from_peer(me, &blk);
    }
}

/**
 * Tells the peerconn that the connection failed */
void pwp_conn_connect_failed(void *pco)
{
    pwp_connection_t *me = pco;

    /* check if we haven't failed before too many times
     * we do not want to stay in an end-less loop */
    me->state.failed_connections += 1;

    if (5 < me->state.failed_connections)
    {
        me->state.flags = PC_UNCONTACTABLE_PEER;
    }
    assert(0);
}

void pwp_conn_periodic(void *pco)
{
    pwp_connection_t *me;

    me = pco;

    if (pwp_conn_flag_is_set(me, PC_UNCONTACTABLE_PEER))
    {
        printf("uncontactable\n");
        return;
    }

    /* send one pending request to the peer */
    if (0 < llqueue_count(me->pendpeerreqs))
    {
        bt_block_t* blk;

        blk = llqueue_poll(me->pendpeerreqs);
        pwp_conn_send_piece(me, blk);
        free(blk);
    }

    /* unchoke interested peer */
    if (pwp_conn_peer_is_interested(me))
    {
        if (pwp_conn_peer_is_choked(me))
        {
            pwp_conn_unchoke(me);
        }
    }

    /* request piece */
    if (pwp_conn_im_interested(me))
    {
        int ii, end;

        if (pwp_conn_im_choked(me))
        {
            __log(me,"peer is choking us %lx", (long unsigned int) pco);
            return;
        }

        /*  max out pipeline */
        end = 10 - pwp_conn_get_npending_requests(me);
//        printf("pending requests: %d\n", pwp_conn_get_npending_requests(me));

        for (ii = 0; ii < end; ii++)
        {
            __make_request(me);
        }
    }
    else
    {
        pwp_conn_set_im_interested(me);
    }
}

/** 
 *  @return 1 if the peer has this piece; otherwise 0 */
int pwp_conn_peer_has_piece(void *pco, const int piece_idx)
{
    pwp_connection_t *me = pco;

    return bitfield_is_marked(&me->state.have_bitfield, piece_idx);
}

void pwp_conn_keepalive(void* pco __attribute__((__unused__)))
{

}

void pwp_conn_choke(void* pco)
{
    pwp_connection_t* me = pco;

    me->state.flags |= PC_PEER_CHOKING;
    __log(me, "read,choke");
    __expunge_my_pending_reqs(me);
}

void pwp_conn_unchoke(void* pco)
{
    pwp_connection_t* me = pco;

    me->state.flags &= ~PC_PEER_CHOKING;
    __log(me, "read,unchoke");
}

void pwp_conn_interested(void* pco)
{
    pwp_connection_t* me = pco;

    me->state.flags |= PC_PEER_INTERESTED;

#if 0
    if (pwp_conn_peer_is_choked(me))
    {
        pwp_conn_unchoke(me);
    }
#endif

    __log(me, "read,interested");
}

void pwp_conn_uninterested(void* pco)
{
    pwp_connection_t* me = pco;

    me->state.flags &= ~PC_PEER_INTERESTED;
            __log(me, "read,uninterested");
}

void pwp_conn_have(void* pco, msg_have_t* have)
{
    pwp_connection_t* me = pco;

//    assert(payload_len == 4);

    if (1 == pwp_conn_mark_peer_has_piece(me, have->piece_idx))
    {
//      assert(pwp_conn_peer_has_piece(me, piece_idx));
    }

//  bitfield_mark(&me->state.have_bitfield, piece_idx);

    __log(me, "read,have,piece_idx=%d", have->piece_idx);

    /* tell the peer we are intested if we don't have this piece */
    if (!__get_piece(me, have->piece_idx))
    {
        pwp_conn_set_im_interested(me);
    }
}

/**
 * Receive a bitfield */
void pwp_conn_bitfield(void* pco, msg_bitfield_t* bitfield)
{
    pwp_connection_t* me = pco;
    char *str;
    int ii;

     /* A peer MUST send this message immediately after the handshake
     * operation, and MAY choose not to send it if it has no pieces at
     * all. This message MUST not be sent at any other time during the
     * communication. */

#if 0
    if (me->num_pieces < bitfield_get_length(&bitfield->bf))
    {
        __disconnect(me, "too many pieces within bitfield");
    }
#endif

    if (pwp_conn_flag_is_set(me,PC_BITFIELD_RECEIVED))
    {
        __disconnect(me, "peer sent bitfield twice");
    }

    for (ii = 0; ii < me->num_pieces; ii++)
    {
        if (bitfield_is_marked(&bitfield->bf,ii))
        {
            pwp_conn_mark_peer_has_piece(me, ii);
        }
    }

    str = bitfield_str(&me->state.have_bitfield);
    __log(me, "read,bitfield,%s", str);
    free(str);

    me->state.flags |= PC_BITFIELD_RECEIVED;
}

/**
 * Respond to a peer's request for a block
 * @return 0 on error, 1 otherwise */
int pwp_conn_request(void* pco, bt_block_t *request)
{
    pwp_connection_t* me = pco;
    void *pce;

    /* check that the client doesn't request when they are choked */
    if (pwp_conn_peer_is_choked(me))
    {
        __disconnect(me, "peer requested when they were choked");
        return 0;
    }

    /* We're choking - we aren't obligated to respond to this request */
    if (pwp_conn_peer_is_choked(me))
    {
        return 0;
    }

    /* Ensure we have correct piece_idx */
    if (me->num_pieces < request->piece_idx)
    {
        __disconnect(me, "requested piece %d has invalid idx",
                     request->piece_idx);
        return 0;
    }

    /* Ensure that we have this piece */
    if (!(pce = __get_piece(me, request->piece_idx)))
    {
        __disconnect(me, "requested piece %d is not available",
                     request->piece_idx);
        return 0;
    }

    /* Ensure that the peer needs this piece.
     * If the peer doesn't need the piece then that means the peer is
     * potentially invalid */
    if (pwp_conn_peer_has_piece(me, request->piece_idx))
    {
        __disconnect(me, "peer requested pce%d which they confirmed they had",
                     request->piece_idx);
        return 0;
    }

    /* Ensure that block request length is valid  */
    if (request->block_len == 0 || me->piece_len < request->block_byte_offset + request->block_len)
    {
        __disconnect(me, "invalid block request"); 
        return 0;
    }

    /* Ensure that we have completed this piece.
     * The peer should know if we have completed this piece or not, so
     * asking for it is an indicator of a invalid peer. */
    assert(NULL != me->func->piece_is_complete);
    if (0 == me->func->piece_is_complete(me->caller, pce))
    {
        __disconnect(me, "requested piece %d is not completed",
                     request->piece_idx);
        return 0;
    }

    /* Append block to our pending request queue. */
    /* Don't append the block twice. */
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

/**
 * Receive a cancel message
 */
void pwp_conn_cancel(void* pco, bt_block_t *cancel)
{
    pwp_connection_t* me = pco;
    bt_block_t *removed;

    __log(me, "read,cancel,piece_idx=%d offset=%d length=%d",
          cancel->piece_idx, cancel->block_byte_offset,
          cancel->block_len);

    /* remove from linked list queue */
    removed = llqueue_remove_item_via_cmpfunction(
            me->pendpeerreqs, cancel, (void*)__request_compare);

    free(removed);

//  queue_remove(peer->request_queue);
}

/**
 * Receive a piece message
 * @return 1; otherwise 0 on failure */
int pwp_conn_piece(void* pco, msg_piece_t *piece)
{
    pwp_connection_t* me = pco;
    request_t *req_removed;

    __log(me, "READ,piece,piece_idx=%d offset=%d length=%d",
          piece->block.piece_idx,
          piece->block.block_byte_offset,
          piece->block.block_len);

    /* remove pending request */
    if (!(req_removed = hashmap_remove(me->pendreqs, &piece->block)))
    {
#if 0
        /* ensure that the peer is sending us a piece we requested */
        __disconnect(me, "err: received a block we did not request: %d %d %d\n",
                     piece->block.piece_idx,
                     piece->block.block_byte_offset,
                     piece->block.block_len);
        return 0;
#endif
    }

    /* Insert block into database.
     * Should result in the caller having other peerconns send HAVE messages */
    me->func->pushblock(
            me->caller,
            me->peer_udata,
            &piece->block,
            piece->data);

    /*  there should have been a request polled */
//    assert(NULL != req_removed);
    if (req_removed)
        free(req_removed);
    return 1;
}

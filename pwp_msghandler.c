
/**
 * Copyright (c) 2011, Willem-Hendrik Thiart
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. 
 *
 * @file
 * @brief pwp_msghandler is an adapter between a data channel and pwp_connection
 *        Bytes from data channel are converted into events for pwp_connection
 *        This module allows us to make pwp_connection event based
 * @author  Willem Thiart himself@willemthiart.com
 * @version 0.1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* for uint32_t */
#include <stdint.h>

#include "bitfield.h"
#include "pwp_connection.h"
#include "pwp_msghandler.h"

#undef min
#define min(a,b) ((a) < (b) ? (a) : (b))

typedef struct {
    uint32_t len;
    unsigned char id;
    unsigned int bytes_read;
    unsigned int tok_bytes_read;
    union {
        msg_have_t hve;
        msg_bitfield_t bf;
        bt_block_t blk;
        msg_piece_t pce;
    };
} msg_t;

typedef struct {
    /* current message we are reading */
    msg_t msg;

    /* peer connection */
    void* pc;
} bt_peer_connection_event_handler_t;

static void __endmsg(msg_t* msg)
{
    memset(msg,0,sizeof(msg_t));
}

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

static int __read_uint32(
        uint32_t* in,
        msg_t *msg,
        const unsigned char** buf,
        unsigned int *len)
{
    while (1)
    {
        if (msg->tok_bytes_read == 4)
        {
            *in = fe(*in);
            msg->tok_bytes_read = 0;
            return 1;
        }
        else if (*len == 0)
        {
            return 0;
        }

        *((unsigned char*)in + msg->tok_bytes_read) = **buf;
        msg->tok_bytes_read += 1;
        msg->bytes_read += 1;
        *buf += 1;
        *len -= 1;
    }
}

/**
 * @param in Read data into 
 * @param tot_bytes_read Running total of total number of bytes read
 * @param buf Read data from
 * @param len Length of stream left to read from */
static int __read_byte(
        unsigned char* in,
        unsigned int *tot_bytes_read,
        const unsigned char** buf,
        unsigned int *len)
{
    if (*len == 0)
        return 0;

    *in = **buf;
    *tot_bytes_read += 1;
    *buf += 1;
    *len -= 1;
    return 1;
}

void* pwp_msghandler_new(void *pc)
{
    bt_peer_connection_event_handler_t* me;

    me = calloc(1,sizeof(bt_peer_connection_event_handler_t));
    me->pc = pc;
    return me;
}

void pwp_msghandler_release(void *pc)
{
    free(pc);
}

int pwp_msghandler_dispatch_from_buffer(void *mh,
        const unsigned char* buf,
        unsigned int len)
{
    bt_peer_connection_event_handler_t* me = mh;
    msg_t* m = &me->msg;

    /* while we have a stream left to read... */
    while (0 < len)
    {
        /* read at least an int */
        if (m->bytes_read < 4)
        {
            /* read length of message */
            if (1 == __read_uint32(&m->len, m, &buf, &len))
            {
                /* it was a keep alive message */
                if (0 == m->len)
                {
                    pwp_conn_keepalive(me->pc);
                    __endmsg(m);
                }
            }
        }
        /* get message ID */
        else if (4 == m->bytes_read)
        {
            __read_byte(&m->id, &m->bytes_read, &buf, &len);

            /* payloadless messages */
            if (m->len != 1) continue;

            switch (m->id)
            {
            case PWP_MSGTYPE_CHOKE:
                pwp_conn_choke(me->pc);
                break;
            case PWP_MSGTYPE_UNCHOKE:
                pwp_conn_unchoke(me->pc);
                break;
            case PWP_MSGTYPE_INTERESTED:
                pwp_conn_interested(me->pc);
                break;
            case PWP_MSGTYPE_UNINTERESTED:
                pwp_conn_uninterested(me->pc);
                break;
            default: assert(0); break;
            }
            __endmsg(m);
        }
        /* messages with a payload: */
        else 
        {
            switch (m->id)
            {
            case PWP_MSGTYPE_HAVE:
                if (1 == __read_uint32(&m->hve.piece_idx, m, &buf, &len))
                {
                    pwp_conn_have(me->pc, &m->hve);
                    __endmsg(m);
                    continue;
                }

                break;
            case PWP_MSGTYPE_BITFIELD:
                {
                    unsigned char val = 0;
                    unsigned int ii;

                    if (1 + 4 == m->bytes_read)
                    {
                        bitfield_init(&m->bf.bf, (m->len - 1) * 8);
                    }

                    assert(m->bf.bf.bits);

                    /* read and mark bits from byte */
                    __read_byte(&val, &m->bytes_read, &buf, &len);
                    for (ii=0; ii<8; ii++)
                    {
                        if (0x1 == ((unsigned char)(val << ii) >> 7))
                        {
                            bitfield_mark(&m->bf.bf,
                                    (m->bytes_read - 5 - 1) * 8 + ii);
                        }
                    }

                    /* done reading bitfield */
                    if (4 + m->len == m->bytes_read)
                    {
                        pwp_conn_bitfield(me->pc, &m->bf);
                        bitfield_release(&m->bf.bf);
                        __endmsg(m);
                    }
                }
                break;
            case PWP_MSGTYPE_REQUEST:
                if (m->bytes_read < 1 + 4 + 4)
                {
                    __read_uint32(&m->blk.piece_idx, m, &buf, &len);
                }
                else if (m->bytes_read < 1 + 4 + 4 + 4)
                {
                    __read_uint32(&m->blk.offset, m, &buf, &len);
                }
                else if (1 == __read_uint32(&m->blk.len, m, &buf, &len))
                {
                    pwp_conn_request(me->pc, &m->blk);
                    __endmsg(m);
                }

                break;
            case PWP_MSGTYPE_CANCEL:
                if (m->bytes_read < 1 + 4 + 4)
                {
                    __read_uint32(&m->blk.piece_idx, m, &buf, &len);
                }
                else if (m->bytes_read < 1 + 4 + 4 + 4)
                {
                    __read_uint32(&m->blk.offset, m, &buf, &len);
                }
                else if (1 == __read_uint32(&m->blk.len, m, &buf, &len))
                {
                    pwp_conn_cancel(me->pc, &m->blk);
                    __endmsg(m);
                }
                break;
            case PWP_MSGTYPE_PIECE:
                if (m->bytes_read < 1 + 4 + 4)
                {
                    __read_uint32(&m->pce.blk.piece_idx, m, &buf, &len);
                }
                else if (m->bytes_read < 1 + 4 + 4 + 4)
                {
                    __read_uint32(&m->pce.blk.offset, m, &buf, &len);
                }
                else
                {
                    /* check it isn't bigger than what the message tells
                     * us we should be expecting */
                    int size = min(len, m->len - 1 - 4 - 4);

                    m->pce.data = buf;
                    m->pce.blk.len = size;
                    pwp_conn_piece(me->pc, &m->pce);

                    /* If we haven't received the full piece, why don't we
                     * just split it "virtually"? That's what we do here: */
                    m->len -= size;
                    m->pce.blk.offset += size;
                    buf += size;
                    len -= size;

                    /* if we received the whole message we're done */
                    if (9 == m->len)
                    {
                        __endmsg(m);
                    }

                }
                break;
            default:
                printf("ERROR: bad pwp msg type: '%d'\n", m->id);
                return 0;
                break;
            }
        }
    }

    return 1;
}


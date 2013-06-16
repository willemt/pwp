#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "bitfield.h"
#include "pwp_connection.h"
#include "pwp_handshaker.h"
#include "pwp_local.h"
#include "bitstream.h"

typedef struct {
    pwp_handshake_t hs;
    int bytes_read;

    char* cur;
    char* curr_value;

    /* expected infohash */
    unsigned char* expected_ih;

    /* my peer id */
    unsigned char* my_pi;
} pwp_handshaker_t;

void* pwp_handshaker_new(unsigned char* expected_info_hash, unsigned char* mypeerid)
{
    pwp_handshaker_t* me;

    me = calloc(1,sizeof(pwp_handshaker_t));
    me->expected_ih = expected_info_hash;
    me->my_pi = mypeerid;
    return me;
}

pwp_handshake_t* pwp_handshaker_get_handshake(void* me_)
{
    pwp_handshaker_t* me = me_;
    return &me->hs;
}

unsigned char __readbyte(unsigned int* bytes_read, unsigned char **buf, unsigned int* len)
{
    unsigned char val;

    val = **buf;
    *buf += 1;
    *bytes_read += 1;
    *len -= 1;
    return val;
}

/**
 *  Receive handshake from other end
 *  Disconnect on any errors
 *
 *  @return 1 on succesful handshake; otherwise 0; -1 on failed handshake */
int pwp_handshaker_dispatch_from_buffer(void* meo, unsigned char* buf, unsigned int len)
{
    pwp_handshaker_t* me = me;
    pwp_handshake_t* hs = &me->hs;

    while (0 < len)
    {

    /* protcol name length
     * The unsigned value of the first byte indicates the length of a
     * character string containing the prot name. In BTP/1.0 this number
     * is 19. The local peer knows its own prot name and hence also the
     * length of it. If this length is different than the value of this
     * first byte, then the connection MUST be dropped. */
        if (me->curr_value == NULL)
        {
            hs->pn_len = __readbyte(&me->bytes_read, &buf, &len);

            /* invalid length */
            if (0 == hs->pn_len)
            {
                return 0;
            }
            me->cur = me->curr_value = hs->pn = malloc(hs->pn_len);
        }
    /* protocol name
    This is a character string which MUST contain the exact name of the 
    prot in ASCII and have the same length as given in the Name Length
    field. The prot name is used to identify to the local peer which
    version of BTP the remote peer uses. If this string is different
    from the local peers own prot name, then the connection is to be
    dropped. */
        else if (me->curr_value == hs->pn)
        {
            *me->cur = __readbyte(&me->bytes_read, &buf, &len);
//            printf("%c\n", *me->cur);
//            printf("string %.*s\n", me->cur - me->curr_value, hs->pn);
            me->cur++;

            /* validate */
            if (me->cur - me->curr_value == hs->pn_len)
            {
                if (0 != strncmp(hs->pn, PROTOCOL_NAME,
                    hs->pn_len < strlen(PROTOCOL_NAME) ?
                        hs->pn_len : strlen(PROTOCOL_NAME)))
                {
                    return 0;
                }

                me->cur = me->curr_value = hs->reserved = malloc(8);
            }
        }
    /* Reserved
    The next 8 bytes in the string are reserved for future extensions and
    should be read without interpretation. */
        else if (me->curr_value == hs->reserved)
        {
            *(me->cur++) = __readbyte(&me->bytes_read, &buf, &len);
            if (me->cur - me->curr_value == 8)
            {
                me->cur = me->curr_value = hs->infohash = malloc(20);
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
        else if (me->curr_value == hs->infohash)
        {
            *(me->cur++) = __readbyte(&me->bytes_read, &buf, &len);

            /* validate */
            if (me->cur - me->curr_value == 20)
            {
                /* check info hash matches expected */
                if (0 != strncmp(hs->infohash, me->expected_ih, 20))
                {
//                    __log(me, "handshake: invalid infohash: '%s' vs '%s'",
//                            hs->peerid, me->expected_info_hash);
                    return 0;
                }

                me->cur = me->curr_value = hs->peerid = malloc(20);
            }
        }
    /* Peer ID:
    The last 20 bytes of the handshake are to be interpreted as the
    self-designated name of the peer. The local peer must use this name to id
    entify the connection hereafter. Thus, if this name matches the local
    peers own ID name, the connection MUST be dropped. Also, if any other
    peer has already identified itself to the local peer using that same peer
    ID, the connection MUST be dropped. */
        else if (me->curr_value == hs->peerid)
        {
            *(me->cur++) = __readbyte(&me->bytes_read, &buf, &len);
            if (me->cur - me->curr_value == 20)
            {
#if 0
                /* disconnect if peer's ID is the same as ours */
                if (!strncmp(peer_id,me->my_peer_id,20))
                {
                    __disconnect(me, "handshake: peer_id same as ours (us: %s them: %.*s)",
                            me->my_peer_id, 20, peer_id);
                    return 0;
                }
#endif

                return 1;
            }
        }
        else
        {
            return -1;
        }
    }

    return 0;
}

#if 0
void other()
{
    pwp_connection_t *me = pco;

    int ii;
    unsigned char name_len;

    /* other peers name prot name */
    char peer_pname[strlen(pn)];
    char peer_infohash[INFO_HASH_LEN];
    char peer_id[PEER_ID_LEN];
    char peer_reserved[8 + 1];

    __log(me, "got,handshake");

    for (ii = 0; ii < name_len; ii++)
    {
        if (0 == __read_byte_from_peer(me, (unsigned char*)&peer_pname[ii]))
        {
            __disconnect(me, "handshake: invalid prot name char");
            return 0;
        }
    }
//    strncpy(peer_pname, &handshake[1], name_len);
    if (strncmp(peer_pname, pn, name_len))
    {
        __disconnect(me, "handshake: invalid prot name: '%s'",
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

    __log(me, "read,handshake,me:%.*s,them:%.*s", 20, me->my_peer_id, 20, peer_id);
    return TRUE;
}
#endif

#if 1
void main()
{
    void* hss;
    int ii;
    unsigned char msg[1000], *ptr;

    ptr = msg;

    bitstream_write_ubyte(&ptr, strlen(PROTOCOL_NAME)); /* pn len */
    bitstream_write_string(&ptr, PROTOCOL_NAME, strlen(PROTOCOL_NAME)); /* pn */
    for (ii=0;ii<8;ii++)
        bitstream_write_ubyte(&ptr, 0);        /*  reserved */
    bitstream_write_string(&ptr, "00000000000000000000", 20); /* ih */
    bitstream_write_string(&ptr, "00000000000000000000", 20); /* pi */

    hss = pwp_handshaker_new("00000000000000000000", "00000000000000000000");

    pwp_handshake_t* hs;

    pwp_handshaker_dispatch_from_buffer(hss, msg, ptr - msg);
    hs = pwp_handshaker_get_handshake(hss);

    printf("done: \n"
            "%.*s\n"
            "%.*s\n"
            "%.*s\n"
            "%.*s\n",
            hs->pn_len, hs->pn,
            8, hs->reserved,
            20, hs->infohash,
            20, hs->peerid);
}
#endif

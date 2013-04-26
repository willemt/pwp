
/*  bt block */
typedef struct
{
    int piece_idx;
    int block_byte_offset;
    int block_len;
} bt_block_t;


typedef void (
    *func_log_f
)    (
    void *udata,
    void *src,
//    bt_peer_t * peer,
    const char *buf,
    ...
);


typedef int (
    *func_pollblock_f
)   (
    void *udata,
    void * peers_bitfield,
    bt_block_t * blk
);

typedef int (
    *func_have_f
)   (
    void *udata,
    void * peer,
    int piece
);

typedef int (
    *func_pushblock_f
)   (
    void *udata,
    void * peer,
    bt_block_t * block,
    void *data
);


typedef int (
    *func_send_f
)   (
    void *udata,
    void * peer,
    void *send_data,
    const int len
);

typedef int (
    *func_recv_f
)   (
    void *udata,
    void * peer,
    char *buf,
    int *len
);

typedef int (
    *func_disconnect_f
)   (
    void *udata,
    void * peer,
    char *reason
);

typedef int (
    *func_connect_f
)   (
    void *bto,
    void *pc,
    void * peer
);

typedef int (
    *func_get_int_f
)   (
    void *,
    void *pr
);

/*  send/receiver */
typedef struct
{
    func_send_f send;
    func_recv_f recv;
    func_disconnect_f disconnect;
    func_connect_f connect;
} sendreceiver_i;

#include "bt_piece_i.h"

#define PC_NONE 0
#define PC_HANDSHAKE_SENT 1<<0
#define PC_HANDSHAKE_RECEIVED 1<<1
#define PC_DISCONNECTED 1<<2
#define PC_BITFIELD_RECEIVED 1<<3
/*  connected to peer */
#define PC_CONNECTED 1<<4
/*  we can't communicate with the peer */
#define PC_UNCONTACTABLE_PEER 1<<5

typedef enum
{
    PWP_MSGTYPE_CHOKE = 0,
    PWP_MSGTYPE_UNCHOKE = 1,
    PWP_MSGTYPE_INTERESTED = 2,
    PWP_MSGTYPE_UNINTERESTED = 3,
    PWP_MSGTYPE_HAVE = 4,
    PWP_MSGTYPE_BITFIELD = 5,
    PWP_MSGTYPE_REQUEST = 6,
    PWP_MSGTYPE_PIECE = 7,
    PWP_MSGTYPE_CANCEL = 8,
} pwp_msg_type_e;

/*  peer wire protocol configuration */
typedef struct
{
    int max_pending_requests;
} bt_pwp_cfg_t;

void *bt_peerconn_get_peer(void *pco);

void *bt_peerconn_new();

void bt_peerconn_set_active(void *pco, int opt);

int bt_peerconn_peer_is_interested(void *pco);

int bt_peerconn_is_active(void *pco);

//void bt_peerconn_set_pieceinfo(void *pco, bt_piece_info_t * pi);

void bt_peerconn_set_peer_id(void *pco, const char *peer_id);

void bt_peerconn_set_infohash(void *pco, const char *infohash);

//void bt_peerconn_set_pwp_cfg(void *pco, bt_pwp_cfg_t * cfg);

//void bt_peerconn_set_func_get_infohash(void *pco, func_get_infohash_f func);

void bt_peerconn_set_func_send(void *pco, func_send_f func);

//void bt_peerconn_set_func_getpiece(void *pco, func_getpiece_f func);

void bt_peerconn_set_func_pollblock(void *pco, func_pollblock_f func);

void bt_peerconn_set_func_have(void *pco, func_have_f func);

void bt_peerconn_set_func_connect(void *pco, func_connect_f func);

void bt_peerconn_set_func_disconnect(void *pco, func_disconnect_f func);

void bt_peerconn_set_func_pushblock(void *pco, func_pushblock_f func);

void bt_peerconn_set_func_recv(void *pco, func_recv_f func);

void bt_peerconn_set_func_log(void *pco, func_log_f func);

//void bt_peerconn_set_func_piece_is_complete(void *pco, func_get_int_f func_piece_is_complete);

void bt_peerconn_set_isr_udata(void *pco, void *udata);

void bt_peerconn_set_peer(void *pco, void * peer);

int bt_peerconn_peer_is_interested(void *pco);

int bt_peerconn_peer_is_choked(void *pco);

int bt_peerconn_im_choked(void *pco);

int bt_peerconn_im_interested(void *pco);

void bt_peerconn_choke(void * pc);

void bt_peerconn_unchoke(void * pco);

int bt_peerconn_get_download_rate(const void * pco);

int bt_peerconn_get_upload_rate(const void * pco);

int bt_peerconn_send_statechange(void * pco, const int msg_type);

void bt_peerconn_send_piece(void *pco, bt_block_t * req);

int bt_peerconn_send_have(void *pco, const int piece_idx);

void bt_peerconn_send_request(void *pco, const bt_block_t * request);

void bt_peerconn_send_cancel(void *pco, bt_block_t * cancel);

void bt_peerconn_send_bitfield(void *pco);

int bt_peerconn_recv_handshake(void *pco, const char *info_hash);

int bt_peerconn_send_handshake(void *pco);

void bt_peerconn_set_num_pieces(void *pco, int num_pieces);

void bt_peerconn_set_piece_len(void *pco, int piece_len);

void bt_peerconn_set_state(void *pco, const int state);

int bt_peerconn_get_state(void *pco);

int bt_peerconn_mark_peer_has_piece(void *pco, const int piece_idx);

int bt_peerconn_process_request(void * pco, bt_block_t * request);

void bt_peerconn_process_msg(void *pco);

int bt_peerconn_get_npending_requests(const void * pco);

void bt_peerconn_request_block(void * pco, bt_block_t * blk);

void bt_peerconn_step(void *pco);

int bt_peerconn_peer_has_piece(void *pco, const int piece_idx);

void bt_peerconn_set_ipce(void *pco,
    func_write_block_to_stream_f func_write_block_to_stream,
    func_get_int_f func_piece_is_complete,
    func_getpiece_f func_getpiece,
    void* caller);


typedef struct {
    /* protocol name */
    int pn_len;
    unsigned char* pn;
    unsigned char* reserved;
    unsigned char* infohash;
    unsigned char* peerid;
} pwp_handshake_t;

void* pwp_handshaker_new(unsigned char* expected_info_hash, unsigned char* mypeerid);
pwp_handshake_t* pwp_handshaker_get_handshake(void* me_);
int pwp_handshaker_dispatch_from_buffer(void* me_, const unsigned char** buf, unsigned int* len);
void pwp_handshaker_release(void* hs);

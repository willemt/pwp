
typedef struct {
    /* protocol name */
    int pn_len;
    unsigned char* pn;
    unsigned char* reserved;
    unsigned char* infohash;
    unsigned char* peerid;
} pwp_handshake_t;

/**
 * Create a new handshaker */
void* pwp_handshaker_new(unsigned char* expected_info_hash, unsigned char* mypeerid);

/**
 * Release memory used by handshaker */
void pwp_handshaker_release(void* hs);

/**
 * @return null if handshake was successful */
pwp_handshake_t* pwp_handshaker_get_handshake(void* me_);

/**
 *  Receive handshake from other end
 *  Disconnect on any errors
 *  @return 1 on succesful handshake; 0 on unfinished reading; -1 on failed handshake */
int pwp_handshaker_dispatch_from_buffer(void* me_, const unsigned char** buf, unsigned int* len);


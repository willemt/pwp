
/**
 * @return new msg handler */
void* pwp_msghandler_new(void *mh);

/**
 * Release memory used by message handler */
void pwp_msghandler_release(void *mh);

/**
 * Receive this much data.
 * If there is enough data this function will dispatch pwp_connection events
 * @param mh The message handler object
 * @param buf The data to be read in
 * @param len The length of the data to be read in
 * @return 1 if successful, 0 if the peer needs to be disconnected */
int pwp_msghandler_dispatch_from_buffer(void *mh,
        const unsigned char* buf,
        unsigned int len);


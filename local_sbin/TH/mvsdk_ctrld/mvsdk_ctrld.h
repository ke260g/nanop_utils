#ifndef MVSDK_CTRLD_H
#define MVSDK_CTRLD_H

/* mvsdk_ctrld comminucation protocol
 * needs to be consulted by client and daemon
 * daemon: mvsdk_ctrld.c
 * client: mvsdk_iface.c
 */
#define IPCKEY_PATHNAME "/home/"
#define shm_key_proj_id (0xfa)
#define msq_key_MISO_proj_id (0xfb)
#define msq_key_MOSI_proj_id (0xfc)

#define PRODUCT_NAME_FILE "/tmp/mvsdk_ctrld.product"

typedef struct _msgbuf_t {
	long mtype;
	char mtext[4];
} msgbuf_t;

enum {
    ROWS_ROLS_GET = 1,
	EXPOUSE_TIME_GET,
	EXPOUSE_TIME_SET,
	MONO_OR_BAYER, // query mono or bayer type
	TRI_AND_GET_IMG, // soft trigger and get image
};
#endif /* MVSDK_CTRLD_H */

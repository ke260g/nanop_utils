#ifndef MVSDK_CTRLD_H
#define MVSDK_CTRLD_H

/* mvsdk_ctrld comminucation protocol
 * needs to be consulted by client and daemon
 * daemon: mvsdk_ctrld.c
 * client: mvsdk_iface.c
 *
 * high level protocol for control
 *
 */
#define IPCKEY_PATHNAME "/tmp"
#define shm_key_proj_id (0xfa)

#define ECHO_PORT (2000)
#define PRODUCT_NAME_FILE "/tmp/mvsdk_ctrld.product"

/* comminucation protocol */
#define HEADER_ID ((uint8_t)(0x50))
#define SET_EXPOUSE_TIME  ((uint8_t)(0x08)) // 8'b0000_1000
#define GET_EXPOUSE_TIME  ((uint8_t)(0x00)) // 8'b0000_0000
#define GET_MONO_OR_BAYER ((uint8_t)(0x01)) // 8'b0000_0001
#define GET_ROWS_COLS     ((uint8_t)(0x02)) // 8'b0000_0010
#define GET_IMAGES        ((uint8_t)(0x03)) // 8'b0000_0011
#define GET_IMAGES_ETH    ((uint8_t)(0x04)) // 8'b0000_0100

#define MAX_ASK_LENGTH (16) // max ask length
#define MAX_ACK_LENGTH (32)
#endif /* MVSDK_CTRLD_H */

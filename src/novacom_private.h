/*
 * novacom_private.h
 *
 *  Created on: Jun 24, 2009
 *      Author: mike.gaffney
 *
 * Hopefully this name is temporary. This is the internal header
 * for the novacom / pmux stuff. It's mainly here so we can do
 * lower level testing.
 *
 * You should only be using this header if you
 * are working IN novacom, not WITH it.
 */

#ifndef NOVACOM_PRIVATE_H_
#define NOVACOM_PRIVATE_H_
#include <usb.h>

#define uint32                         unsigned int
#define uint16                         short

#define USB_VENDOR_PALM             0x0830
#define USB_TIMEOUT                 5000
#define USB_BUFLEN                  32768

#define NOVACOM_USB_CLASS           255
#define NOVACOM_USB_SUBCLASS        71
#define NOVACOM_USB_PROTOCOL        17

#define NOVACOM_CMD_NOP             0
#define NOVACOM_CMD_ANNOUNCEMENT    2
#define NOVACOM_CMD_PMUX            3

#define PMUX_HEADER_VERSION         1

#define PMUX_HEADER_TYPE_DATA       0
#define PMUX_HEADER_TYPE_ERR        1
#define PMUX_HEADER_TYPE_OOB        2

#define PMUX_OOB_EOF                0
#define PMUX_OOB_SIGNAL             1
#define PMUX_OOB_RETURN             2
#define PMUX_OOB_RESIZE             3

#define PMUX_FILENO_STDIN           0
#define PMUX_FILENO_STDOUT          1
#define PMUX_FILENO_STDERR          2

static char *NOVACOM_COMMANDS[] = {
    "NOP",
    "",
    "ANNOUNCEMENT",
    "PMUX"
} ;

#define PMUX_HEADER_MAGIC           0x7573626c

#define PMUX_ASCII_MAGIC             0x706d7578
#define PMUX_TX                     0x0037
#define PMUX_RX                     0x0162

typedef struct {
    uint32 magic ;
    uint32 direction ;
    char *command ;
} novacom_ascii_t ;

typedef struct {
    // nduid is NOT null terminated here
    char nduid[40] ;
    uint32 mtu ;
    uint32 heartbeat ;
    uint32 timeout ;
} novacom_announcement_t ;

typedef struct {
    // nduid IS null terminated here
    char nduid[40] ;
} novacom_nop_t ;

typedef struct {
    uint32 type ;
    uint32 payload ;
    uint32 zero[3] ;
} pmux_oob_t ;

// Structure to open the tty
typedef struct {
    uint32 three ;
    uint32 ten ;
    uint32 dee ;
} pmux_channel_open_t ;

typedef struct {
    char request[0] ;
} pmux_cmd_request_t ;

typedef struct {
    char reply[0] ;
} pmux_cmd_reply_t ;

// Structure for the tty
typedef struct {
    uint32 magic ;
    uint32 version ;
    uint32 length ;
    uint32 type ;
    char payload[0] ;
} pmux_data_payload_t ;

typedef struct {
    uint32 magic ;
    char mode ;
    char direction ;
    uint16 ack_synx ;
    uint16 status ;
    uint32 sequence_num ;
    uint32 length_payload ;
    uint32 length_pmux_packet ;
    uint32 zero ;
    char payload[0] ;
} pmux_packet_t ;

struct usb_dev_handle* novacom_find_endpoints( uint32 *ep_in, uint32 *ep_out );
int is_interface_novacom(struct usb_interface_descriptor *interface);
int novacom_init( novacom_device_t *dev );
void print_buf( char *buf, int size );
void novacom_payload_print( uint32 command, char payload[], uint32 size );
int novacom_packet_read( novacom_device_t *dev, uint32 size, uint32 timeout );
int novacom_packet_write( novacom_device_t *dev, uint32 size, uint32 timeout );
void novacom_packet_print( novacom_packet_t *packet, int size );
int novacom_reply_nop( novacom_device_t *dev, uint32 len );
int novacom_reply_announcement( novacom_device_t *dev, uint32 len );
int novacom_packet_process( novacom_device_t *dev, int len );
int error_check( int ret, int quit, char *msg );


#endif /* NOVACOM_PRIVATE_H_ */

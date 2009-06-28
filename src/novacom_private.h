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

#define uint32                      unsigned int
#define uint16                      unsigned short
#define uint8                       unsigned char
#define int8                        char

#define USB_VENDOR_PALM             0x0830
#define USB_TIMEOUT                 1000/30 /* 1/30th of a second, for good keyboard response */
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

#define PMUX_CMD_OPEN               3
#define PMUX_CMD_CLOSE              4

static char *NOVACOM_COMMANDS[] = {
    "NOP",
    "",
    "ANNOUNCEMENT",
    "PMUX"
} ;

#define PMUX_HEADER_MAGIC           0x7573626c  /* usbl */

#define PMUX_ASCII_MAGIC             0x706d7578  /* pmux */
#define PMUX_TX                     0x0037
#define PMUX_RX                     0x0162

#define PMUX_DATA_MAGIC 0xdecafbad

#define PMUX_MODE_NORMAL 3

#define PMUX_IN 0x62
#define PMUX_OUT 0x37

#define PMUX_SYN 0x0000
#define PMUX_ACK 0x0001

#define PMUX_NOT_CONNECTED 0x0001
#define PMUX_ESTABLISHED 0x1000

#define STATE_WAIT_ANNOUNCE 0
#define STATE_OPEN_ACK 1
#define STATE_GOT_ANNOUNCE 2
#define STATE_LIMBO 3
#define STATE_COMMAND_ACK 4
#define STATE_WAIT_OK 5
#define STATE_TTY 6
#define STATE_CLOSING 7
#define STATE_CLOSED 8

#define MODE_TTY 0
#define MODE_READ 1
#define MODE_WRITE 2

typedef struct {
    uint32 magic ;
    uint32 direction ;
    uint8 *command ;
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


// pmux payload for control
typedef struct {
    uint32 command;
    uint32 hex1000;
    uint32 hex000c;
} pmux_control_payload_t;

#if 0
// Structure to open the tty
typedef struct {
    uint32 three ;
    uint32 ten ;
    uint32 dee ;
} pmux_channel_open_t ;
#endif

typedef struct {
    uint8 request[0] ;
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
    uint8 payload[0] ;
} pmux_data_payload_t ;

typedef struct {
    uint32 magic ;
    uint8 version ;
    uint8 pad ;
    uint16 ack_synx ;
    uint16 flags ;
    uint16 channel_num ;
    // Channel 1 is control
    // Channel 4096 is start of host -> device comms
    // Channel 0x80000000 is start of device -> host comms
    uint32 sequence_num ;
    uint32 length_payload ;
    uint32 length_pmux_packet ;
    uint32 zero ;
    uint8 payload[0] ;
} pmux_packet_t ;

struct usb_dev_handle* novacom_find_endpoints( uint32 *ep_in, uint32 *ep_out );
int is_interface_novacom(struct usb_interface_descriptor *interface);
int novacom_init( novacom_device_t *dev );
void print_buf( char *buf, uint32 size );
void novacom_payload_print( uint32 command, uint8 payload[], uint32 size );
int novacom_packet_read( novacom_device_t *dev, uint32 size, uint32 timeout );
int novacom_packet_write( novacom_device_t *dev, uint32 size, uint32 timeout );
void novacom_packet_print( novacom_packet_t *packet, uint32 size );
int novacom_reply_nop( novacom_device_t *dev, uint32 len, const char *serial);
int novacom_reply_announcement( novacom_device_t *dev);
int novacom_packet_process( novacom_device_t *dev, uint32 len );
int error_check( int ret, int quit, char *msg );


#endif /* NOVACOM_PRIVATE_H_ */

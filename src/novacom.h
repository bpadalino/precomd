/*
 * novacom.h
 *
 *  Created on: Jun 24, 2009
 *      Author: mike.gaffney
 */

#ifndef NOVACOM_H_
#define NOVACOM_H_
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

static char *NOVACOM_COMMANDS[] = {
    "NOP",
    "",
    "ANNOUNCEMENT",
    "PMUX"
} ;

#define PMUX_HEADER_MAGIC           0x7573626c
#define PMUX_HEADER_VERSION         0x00000001

#define PMUX_ASCII_MAGIC             0x706d7578
#define PMUX_TX                     0x0037
#define PMUX_RX                     0x0162

typedef struct {
    uint32 magic ;
    uint32 version ;
    uint32 id_tx ;
    uint32 id_rx ;
    uint32 command ;
    char payload[0] ;
} novacom_packet_t ;

typedef struct {
    struct usb_dev_handle *phone ;
    uint32 ep_rx ;
    uint32 ep_tx ;
    uint32 id_host ;
    uint32 id_device ;
    char id_session[40] ;
    novacom_packet_t packet ;
} novacom_device_t ;

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

struct usb_dev_handle* novacom_find_endpoints( uint32 *ep_in, uint32 *ep_out );
int is_interface_novacom(struct usb_interface_descriptor interface);
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


#endif /* NOVACOM_H_ */

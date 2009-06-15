/* 
  To compile:
    $ gcc -o precomd precomd.c -lusb

  To run:
    $ sudo ./precomd
*/

#define uint32 unsigned int

#define USB_VENDOR_PALM             0x0830
#define USB_TIMEOUT                 5000
#define USB_BUFLEN                  2048

#define NOVACOM_USB_CLASS           255
#define NOVACOM_USB_SUBCLASS        71
#define NOVACOM_USB_PROTOCOL        17

#define NOVACOM_CMD_NOP             0
#define NOVACOM_CMD_ANNOUNCEMENT    2
#define NOVACOM_CMD_PMUX            3

char *NOVACOM_COMMANDS[] = {
    "NOP",
    "",
    "ANNOUNCEMENT",
    "PMUX"
} ;

#define PMUX_HEADER_MAGIC   0x7573626c
#define PMUX_HEADER_VERSION 0x00000001

#define PMUX_ASCII_MAGIC 0x706d7578
#define PMUX_ASCII_TX 0x00370370
#define PMUX_ASCII_RX 0x0162

#include <stdio.h>
#include <usb.h>
#include <unistd.h>
#include <string.h>

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
    char serial[40] ;
    char forty ;
    char zero ;
    uint32 y ;
    uint32 z ;
} novacom_announce_t ;

typedef struct {
    char serial[40] ;
    uint32 x ;
    uint32 y ;
    uint32 z ;
} novacom_announcement_t ;

typedef struct {
    char serial[40] ;
} novacom_nop_t ;

typedef struct {
    uint32 magic ;
    uint32 version ;
    uint32 length ;
    uint32 zero ;
    char payload[0] ;
} pmux_term_t ;

struct usb_dev_handle* novacom_find_endpoints( uint32 *ep_in, uint32 *ep_out ) {
    int c, i, a, ep ;
    usb_dev_handle *retval = 0 ;
    int ret = -1 ;
    struct usb_device *dev;
    struct usb_bus *bus;
    
    usb_init();
    usb_find_busses();
    usb_find_devices();
    
    /* Get all the USB busses to iterate through ... */
    for (bus = usb_get_busses(); bus; bus = bus->next) {

        /* For each device .... */
        for (dev = bus->devices; dev; dev = dev->next) {
            
            /* ... that's a Palm device! */
            if( dev->descriptor.idVendor != USB_VENDOR_PALM ) continue ;

            /* Loop through all of the configurations */
            for (c = 0; c < dev->descriptor.bNumConfigurations; c++) {
                /* Loop through all of the interfaces */
                for (i = 0; i < dev->config[c].bNumInterfaces; i++) {
                    /* Loop through all of the alternate settings */
                    for (a = 0; a < dev->config[c].interface[i].num_altsetting; a++) {
                        /* Check if this interface is novacom on the phone */
                        if (dev->config[c].interface[i].altsetting[a].bInterfaceClass == NOVACOM_USB_CLASS &&
                            dev->config[c].interface[i].altsetting[a].bInterfaceSubClass == NOVACOM_USB_SUBCLASS && 
                            dev->config[c].interface[i].altsetting[a].bInterfaceProtocol == NOVACOM_USB_PROTOCOL ) {
                                /* Open the device, set the alternate setting, claim the interface and do your processing */
                                // printf( "Novacom found!\n") ;
                                retval = usb_open( dev ) ;
                                
                                if( (ret=usb_claim_interface(retval, i)) < 0 ) {
                                    printf( "Error claiming interface %i: %i\n", i, ret ) ;
                                    exit( 1 ) ;
                                }
                                
                                if( (ret=usb_set_altinterface( retval, a)) < 0 ) {
                                    printf( "Error setting altinterface %i: %i\n", a, ret ) ;
                                    exit( 1 ) ;
                                }
                                
                                for( ep = 0 ; ep < dev->config[c].interface[i].altsetting[a].bNumEndpoints ; ep++ ) {
                                    if( dev->config[c].interface[i].altsetting[a].endpoint[ep].bmAttributes == USB_ENDPOINT_TYPE_BULK ) {
                                        if( dev->config[c].interface[i].altsetting[a].endpoint[ep].bEndpointAddress & USB_ENDPOINT_DIR_MASK ) {
                                            *ep_in = dev->config[c].interface[i].altsetting[a].endpoint[ep].bEndpointAddress ;
                                        }
                                        else {
                                            *ep_out = dev->config[c].interface[i].altsetting[a].endpoint[ep].bEndpointAddress ;
                                        }
                                    }
                                }
                        }
                    }
                }
            }
        }
    }

    return retval ;
    
}

int novacom_init( novacom_device_t *dev ) {
    
    if( (dev->phone=novacom_find_endpoints( &(dev->ep_rx), &(dev->ep_tx) )) == 0 ) {
        fprintf( stderr, "ERROR: Novacom not found - are you sure your pre is plugged in?\n" ) ;
        return -1 ;
    }
    
    printf( "Novacom found: bulk_ep_in: 0x%2.2x bulk_ep_out: 0x%2.2x\n\n", dev->ep_rx, dev->ep_tx ) ;
    
    return 0 ;
}

void print_buf( char *buf, int size ) {
    int x ;
    char c ;
    printf( "\nhex:\n\n    ") ;
    for( x = 0 ; x < size ; x++ ) {
        printf( "%2.2x ", buf[x]&0xff ) ;
        if( (x+1) % 8 == 0 ) printf("\n    " ) ;
    }
    printf( "\nascii:\n\n    ") ;
    for( x = 0 ; x < size ; x++ ) {
        c = (char)buf[x]&0xff ;
        c = (c < 32 || c > 126) ? '.' : c ;
        printf( "%c ", c ) ;
        if( (x+1) % 8 == 0 ) printf( "\n    " ) ;
    }
    printf( "\n" ) ;
    
    return ;
}

void novacom_payload_print( uint32 command, char payload[], uint32 size ) {
    switch( command ) {
        case NOVACOM_CMD_NOP           :
            printf( "  serial: %s\n", ((novacom_nop_t *)payload)->serial ) ;
            break ;
        case NOVACOM_CMD_ANNOUNCEMENT  :
            printf( "  serial: %s\n", ((novacom_announce_t *)payload)->serial ) ;
            break ;
        case NOVACOM_CMD_PMUX         :
            print_buf( payload, size ) ;
    }
    
    return ;
    
}

int novacom_packet_read( novacom_device_t *dev, uint32 size, uint32 timeout ) {
    return usb_bulk_read( dev->phone, dev->ep_rx, (char *)&dev->packet, size, timeout ) ;
}

int novacom_packet_write( novacom_device_t *dev, uint32 size, uint32 timeout ) {
    return usb_bulk_write( dev->phone, dev->ep_tx, (char *)&dev->packet, size, timeout ) ;
}

void novacom_packet_print( novacom_packet_t *packet, int size ) {
    
    printf( "magic string   : 0x%8.8x\n", packet->magic ) ;
    printf( "version        : 0x%8.8x\n", packet->version ) ;
    printf( "sender id      : 0x%8.8x\n", packet->id_tx ) ;
    printf( "receiver id    : 0x%8.8x\n", packet->id_rx ) ;
    printf( "command        : " ) ;
    switch( packet->command ) {
        case NOVACOM_CMD_NOP            :
        case NOVACOM_CMD_ANNOUNCEMENT   :
        case NOVACOM_CMD_PMUX          :   
                                            printf( "%s\n", NOVACOM_COMMANDS[packet->command] ) ; 
                                            novacom_payload_print( packet->command, packet->payload, size-sizeof(novacom_packet_t) ) ;
                                            break ;
        default                         :   printf( "UNKNOWN - 0x%8.8xi\n", packet->command ) ;
    }
    
    printf( "\n" ) ;
    
    return ;
}

int novacom_reply_nop( novacom_device_t *dev, uint32 len ) {
    novacom_nop_t *nop = (novacom_nop_t *) &(dev->packet.payload) ;
    dev->packet.id_tx = dev->id_host ;
    dev->packet.id_rx = dev->id_device ;    
    sprintf( nop->serial, "0123456789abcdef0123456789abcdefdecafbad") ;
    return novacom_packet_write( dev, len, USB_TIMEOUT ) ;
}

int novacom_reply_announcement( novacom_device_t *dev, uint32 len ) {
    novacom_announcement_t *announce = (novacom_announcement_t *) &(dev->packet.payload) ;
    dev->packet.id_tx = dev->id_host ;
    dev->packet.id_rx = dev->id_device ;
    sprintf( announce->serial, "0123456789abcdef0123456789abcdefdecafbad") ;
    announce->y = 0x000007d0 ;
    return novacom_packet_write( dev, len, USB_TIMEOUT ) ;
}

int novacom_packet_process( novacom_device_t *dev, int len ) {
    switch ( dev->packet.command ) {
        case NOVACOM_CMD_NOP    :
            printf( "Sending NOP reply\n" ) ;
            return novacom_reply_nop( dev, len ) ;
            break ;
        
        case NOVACOM_CMD_ANNOUNCEMENT :
            dev->id_host   = 0x00004ae1 ;
            dev->id_device = dev->packet.id_tx ;
            printf( "Sending ANNOUNCEMENT reply\n" ) ;
            return novacom_reply_announcement( dev, len ) ;
            break ;
            
        case NOVACOM_CMD_PMUX :
            printf( "Not implemented yet\n" ) ;
            break ;
        
        default :
            printf( "Not sure wtf to do\n" ) ;
            break ;
        
    }
    return -1 ;
}

int error_check( int ret, int quit, char *msg ) {
    if( ret < 0 ) {
        fprintf( stderr, "ERROR: %s\n", msg ) ;
        if( quit ) exit( 1 ) ;
    }
    return ret ;
}

// TODO: Fill in these stubs
int pmux_file_put( novacom_device_t *dev ) { return 0 ; }
int pmux_file_get( novacom_device_t *dev ) { return 0 ; }

int pmux_terminal_open( novacom_device_t *dev ) { return 0 ; }
int pmux_terminal_close( novacom_device_t *dev ) { return 0 ; }
int pmux_terminal_send( novacom_device_t *dev, char *cmd ) { return 0 ; }
int pmux_terminal_receive( novacom_device_t *dev, char *buf ) { return 0 ; }

int pmux_program_run( novacom_device_t *dev, char *cmd, uint32 argc, char **argv ) { return 0 ; }

int pmux_mem_put( novacom_device_t *dev, uint32 addr, uint32 data ) { return 0 ; }
int pmux_mem_boot( novacom_device_t *dev, uint32 addr ) { return 0 ; }

char TELNETD[] = "telnetd\n" ;

int main () {
    
    int ret, i ;
    
    /* The USB device with other relavent information */ 
    novacom_device_t *dev = (novacom_device_t *)malloc( sizeof(novacom_device_t)+USB_BUFLEN ) ;
    
    /* Check to see if we're root before we really get into this thing */
    if( geteuid() != 0 ) {
        fprintf( stderr, "ERROR: root required for USB privilidges.  Please re-run command as root.\n" ) ;
        exit( 1 ) ;
    }
    
    /* Initialize novacom communications */
    error_check( novacom_init( dev ), 1, "Unable to find or initialize Novacom - is your pre plugged in?\n" ) ;
    
    /* Iterate through a NOP loop */
    for( i = 0 ; i  < 10 ; i++ ) {
        printf( "Iteration: %i\n", i ) ;
        ret = error_check( novacom_packet_read( dev, USB_BUFLEN, USB_TIMEOUT ), 0, "Timeout or error reading USB!\n" ) ;
        if( ret > 0 ) {
            printf( "Read %i bytes - success!\n", ret ) ;
            novacom_packet_process( dev, ret ) ;
        }
    }
    
    return 0 ;
}
/* 
  To compile:
    $ gcc -o precomd precomd.c -lusb

  To run:
    $ sudo ./precomd
*/

#define uint32 unsigned int
#define uint16 short

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

#define PMUX_HEADER_MAGIC   0x7573626c  /* usbl */
#define PMUX_HEADER_VERSION 0x00000001

#define PMUX_ASCII_MAGIC 0x706d7578  /* pmux */
#define PMUX_ASCII_TX 0x00370370
#define PMUX_ASCII_RX 0x0162

#define PMUX_TTY_MAGIC 0xdecafbad

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
#define STATE_TERMINAL 6

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
    unsigned char payload[0] ;
} novacom_packet_t ;

typedef struct {
    struct usb_dev_handle *phone ;
    uint32 ep_rx ;
    uint32 ep_tx ;
    uint32 id_host ;
    uint32 id_device ;
    char id_session[40] ;
    int state;
    uint32 pmux_status;
    uint32 pmux_tty_seq_num;
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

// Structure to open the tty
typedef struct {
    uint32 three ;
    uint32 ten ;
    uint32 dee ;
} pmux_channel_open_t ;

typedef struct {
    char opentty[0] ;
} pmux_tty_open_request_t ;

typedef struct {
    char reply[0] ;
} pmux_tty_open_reply_t ;

// Structure for the tty
typedef struct {
    uint32 magic ;
    uint32 one ;
    uint32 length ;
    uint32 zero ;
    unsigned char payload[0] ;
} pmux_tty_payload_t ;

typedef struct {
    uint32 magic ;
    char mode ;
    char direction ;
    uint16 ack_synx ;
    uint16 status ;
    uint16 unknown;
    uint32 sequence_num ;
    uint32 length_payload ;
    uint32 length_pmux_packet ;
    uint32 zero ;
    unsigned char payload[0] ;
} pmux_packet_t ;

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

    dev->state = STATE_WAIT_ANNOUNCE;
    
    return 0 ;
}

void print_buf( unsigned char *buf, int size ) {
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

void novacom_payload_print( uint32 command, unsigned char payload[], uint32 size ) {
    switch( command ) {
        case NOVACOM_CMD_NOP           :
            printf( "  nduid: %s\n", ((novacom_nop_t *)payload)->nduid ) ;
            break ;
        case NOVACOM_CMD_ANNOUNCEMENT  :
            printf( "  nduid: %s\n", ((novacom_announcement_t *)payload)->nduid ) ;
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
    if (dev->state!=STATE_TERMINAL) {
        fprintf(stderr,"Writing packet of %d bytes\n",size);
    }
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

static const char *dummy_serial = "0c698734f53d02dcf0b0fa2335de38992e65d9ce";

int novacom_reply_nop( novacom_device_t *dev, uint32 len ) {
    novacom_nop_t *nop = (novacom_nop_t *) &(dev->packet.payload) ;
    dev->packet.id_tx = dev->id_host ;
    dev->packet.id_rx = dev->id_device ;    
    sprintf( nop->nduid, dummy_serial) ;
    return novacom_packet_write( dev, len, USB_TIMEOUT ) ;
}

int novacom_reply_announcement( novacom_device_t *dev, uint32 len ) {
    novacom_announcement_t *announce = (novacom_announcement_t *) &(dev->packet.payload) ;
    dev->packet.id_tx = dev->id_host ;
    dev->packet.id_rx = dev->id_device ;
    sprintf( announce->nduid, dummy_serial) ;
    announce->mtu = 16384 ;
    announce->heartbeat = 1000 ;
    announce->timeout = 10000 ;
    return novacom_packet_write( dev, len, USB_TIMEOUT ) ;
}

static void print_payload(unsigned char *payload,int len)
{
    fprintf(stderr,"payload:");
    int i = 0;
    for (;i!=len;++i) {
	fprintf(stderr," %02x",payload[i]);
    }
    fprintf(stderr,"\n");
}

static void print_pmux(pmux_packet_t *pmux)
{
    fprintf(stderr,"pmux direction: %02x\n",pmux->direction);
    fprintf(stderr,"pmux ack/syn: %04x\n",pmux->ack_synx);
    fprintf(stderr,"pmux status: %04x\n",pmux->status);
    fprintf(stderr,"pmux unknown: %04x\n",pmux->unknown);
    fprintf(stderr,"pmux sequence_num: %08x\n",pmux->sequence_num);
    fprintf(stderr,"pmux length_payload: %08x\n",pmux->length_payload);
    fprintf(stderr,"pmux length_pmux_packet: %08x\n",pmux->length_pmux_packet);
    print_payload(pmux->payload,pmux->length_payload);
}


static void pmux_ack(novacom_device_t *dev,int seq_num)
{
    if (dev->state!=STATE_TERMINAL) {
       fprintf(stderr,"Sending ack\n");
    }
    int len = 0;
    dev->packet.id_tx = dev->id_host ;
    dev->packet.id_rx = dev->id_device ;
    dev->packet.command = NOVACOM_CMD_PMUX ;
    len += sizeof(dev->packet);
    {
        pmux_packet_t *pmux = (pmux_packet_t *)dev->packet.payload;
        pmux->magic = PMUX_ASCII_MAGIC;
        pmux->mode = PMUX_MODE_NORMAL;
        pmux->direction = PMUX_OUT;
        pmux->ack_synx = PMUX_ACK;
        pmux->status = dev->pmux_status;
	pmux->unknown = 0;
        pmux->sequence_num = seq_num;
        pmux->length_payload = 0x00;
        pmux->length_pmux_packet = 0x1c;
        pmux->zero = 0x00000000;
	len += sizeof(*pmux);
	if (dev->state!=STATE_TERMINAL) {
	   print_pmux(pmux);
	}
    }
    novacom_packet_write(dev,len,USB_TIMEOUT);
}


static void pmux_write_tty(novacom_device_t *dev,char *str,int str_len)
{
    int len = 0;

    if (dev->state!=STATE_TERMINAL) {
        fprintf(stderr,"Sending text\n");
    }
    dev->packet.id_tx = dev->id_host ;
    dev->packet.id_rx = dev->id_device ;
    dev->packet.command = NOVACOM_CMD_PMUX ;
    len += sizeof(dev->packet);
    {
        int cmd_len = sizeof(pmux_tty_payload_t)+str_len;
        pmux_packet_t *pmux = (pmux_packet_t *)dev->packet.payload;
        pmux->magic = PMUX_ASCII_MAGIC;
        pmux->mode = PMUX_MODE_NORMAL;
        pmux->direction = PMUX_OUT;
        pmux->ack_synx = PMUX_SYN;
        pmux->status = PMUX_ESTABLISHED;
	dev->pmux_status = PMUX_ESTABLISHED;
	pmux->unknown = 0;
        pmux->sequence_num = dev->pmux_tty_seq_num;
        pmux->length_payload = cmd_len;
        pmux->length_pmux_packet = 0x1c+cmd_len;
        pmux->zero = 0x00000000;
	{
	  pmux_tty_payload_t *tty = (pmux_tty_payload_t *)pmux->payload;
	  tty->magic = PMUX_TTY_MAGIC;
	  tty->one = 0x00000001;
	  tty->length = str_len;
	  tty->zero = 0x00000000;
	  memcpy(tty->payload,str,str_len);
        }
	len += sizeof(*pmux);
	len += cmd_len;
#if 0
	print_pmux(pmux);
#endif
    }
    novacom_packet_write(dev,len,USB_TIMEOUT);
}


static void pmux_write_command(novacom_device_t *dev,char *cmd)
{
    fprintf(stderr,"Sending %s\n",cmd);
    int len = 0;
    dev->packet.id_tx = dev->id_host ;
    dev->packet.id_rx = dev->id_device ;
    dev->packet.command = NOVACOM_CMD_PMUX ;
    len += sizeof(dev->packet);
    {
        int cmd_len = strlen(cmd);
        pmux_packet_t *pmux = (pmux_packet_t *)dev->packet.payload;
        pmux->magic = PMUX_ASCII_MAGIC;
        pmux->mode = PMUX_MODE_NORMAL;
        pmux->direction = PMUX_OUT;
        pmux->ack_synx = PMUX_SYN;
        pmux->status = PMUX_ESTABLISHED;
	dev->pmux_status = PMUX_ESTABLISHED;
        pmux->sequence_num = 1;
	dev->pmux_tty_seq_num = 1;
        pmux->length_payload = cmd_len;
        pmux->length_pmux_packet = 0x1c+cmd_len;
        pmux->zero = 0x00000000;
	memcpy(pmux->payload,cmd,cmd_len);
	len += sizeof(*pmux);
	len += cmd_len;
	print_pmux(pmux);
    }
    novacom_packet_write(dev,len,USB_TIMEOUT);
}


static void print_payload_str(unsigned char *payload,int len)
{
    int i = 0;
    for (;i<len;++i) {
        fprintf(stderr,"%c",payload[i]);
    }
}


static int read_input(char *buf,int buf_size)
{
  fd_set readfds;
  struct timeval timeout;
  char *p = buf;

  FD_ZERO(&readfds);
  FD_SET(0,&readfds);
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;

  while (p<(buf+buf_size) && select(1,&readfds,0,0,&timeout)) {
     read(0,p++,1);
  }
  return p-buf;
}

int pmux_terminal_open( novacom_device_t *dev ) 
{ 
    int len = 0;

    dev->packet.id_tx = dev->id_host ;
    dev->packet.id_rx = dev->id_device ;
    dev->packet.command = NOVACOM_CMD_PMUX ;
    len += sizeof(dev->packet);
    {
        pmux_packet_t *pmux = (pmux_packet_t *)dev->packet.payload;
        pmux->magic = PMUX_ASCII_MAGIC;
        pmux->mode = PMUX_MODE_NORMAL;
        pmux->direction = PMUX_OUT;
        pmux->ack_synx = PMUX_SYN;
        pmux->status = PMUX_NOT_CONNECTED;
	dev->pmux_status = PMUX_NOT_CONNECTED;
        pmux->sequence_num = 1;
        pmux->length_payload = 0x0c;
        pmux->length_pmux_packet = 0x28;
        pmux->zero = 0x00000000;
	len += sizeof(*pmux);
        {
            pmux_channel_open_t *pmux_open = (pmux_channel_open_t *)pmux->payload;
            pmux_open->three = 0x00000003;
            pmux_open->ten = 0x00001000;
            pmux_open->dee = 0x0000000c;
	    len += sizeof(*pmux_open);
        }
        print_pmux(pmux);
    }
    dev->state = STATE_OPEN_ACK;
    return novacom_packet_write( dev, len, USB_TIMEOUT ) ;
}

int pmux_packet_process( novacom_device_t *dev ) { 
    pmux_packet_t *pmux = (pmux_packet_t *) &(dev->packet.payload) ;
    
    if (pmux->magic != PMUX_ASCII_MAGIC) {
        fprintf(stderr,"Invalid pmux magic: %08x\n",pmux->magic);
        exit(1) ;
    }

    if (pmux->mode != PMUX_MODE_NORMAL) {
	fprintf(stderr,"Invalid pmux mode\n");
	exit(1);
    }

    if (dev->state!=STATE_TERMINAL) {
        print_pmux(pmux);
    }

    if (pmux->ack_synx==PMUX_ACK) {
        if (dev->state!=STATE_TERMINAL) {
	    fprintf(stderr,"Got ack for %d\n",pmux->sequence_num);
	}
	if (dev->state==STATE_OPEN_ACK) {
	    fprintf(stderr,"Going to open tty state\n");
	    pmux_write_command(dev,"open tty://");
	    dev->state=STATE_COMMAND_ACK;
	}
	else if (dev->state==STATE_COMMAND_ACK) {
	    fprintf(stderr,"Going to wait ok state\n");
	    dev->state=STATE_WAIT_OK;
	    ++dev->pmux_tty_seq_num;
	}
	else if (dev->state==STATE_TERMINAL) {
	    ++dev->pmux_tty_seq_num;
	}
    }
    else {
        if (dev->state==STATE_WAIT_OK) {
	    print_payload_str(pmux->payload,pmux->length_payload);
	    pmux_ack(dev,pmux->sequence_num);
	    dev->state=STATE_TERMINAL;
	}
	else if (dev->state==STATE_TERMINAL) {
	    pmux_tty_payload_t *tty = (pmux_tty_payload_t *)pmux->payload;
	    if (tty->magic!=PMUX_TTY_MAGIC) {
		fprintf(stderr,"Bad terminal magic\n");
		dev->pmux_status = pmux->status;
		pmux_ack(dev,pmux->sequence_num);
		pmux_terminal_open(dev);
		dev->pmux_status = PMUX_ESTABLISHED;
		dev->state=STATE_TERMINAL;
	    }
	    else {
		int len = tty->length;
		print_payload_str(tty->payload,len);
		pmux_ack(dev,pmux->sequence_num);
	    }
	}
	else {
	    fprintf(stderr,"Unexpected syn\n");
	    pmux_ack(dev,pmux->sequence_num);
	    pmux_terminal_open(dev);
	}
    }

    return 0 ;
}

int novacom_packet_process( novacom_device_t *dev, int len ) {
    switch ( dev->packet.command ) {
        case NOVACOM_CMD_NOP    :
	    {
		novacom_packet_t *packet = &dev->packet;
		if (dev->state!=STATE_TERMINAL) {
	            printf("Got NOP: id_tx=%x, id_rx=%x",packet->id_tx,packet->id_rx);
	            printf( "Sending NOP reply\n" ) ;
	        }
	    }
	    novacom_reply_nop( dev, len ) ;
	    if (dev->state==STATE_GOT_ANNOUNCE) {
	        pmux_terminal_open(dev);
	    }
            break ;
        
        case NOVACOM_CMD_ANNOUNCEMENT :
            dev->id_host   = 0x00004ae1 ;
            dev->id_device = dev->packet.id_tx ;
            printf( "Sending ANNOUNCEMENT reply\n" ) ;
            if (novacom_reply_announcement( dev, len )<0) {
	      return -1;
	    }
	    dev->state = STATE_GOT_ANNOUNCE;
            break ;
            
        case NOVACOM_CMD_PMUX :
	    if (dev->state!=STATE_TERMINAL) {
                printf( "Processing PMUX packet\n" ) ;
	    }
            return pmux_packet_process( dev ) ;
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

int pmux_terminal_close( novacom_device_t *dev ) { return 0 ; }
int pmux_terminal_send( novacom_device_t *dev, char *cmd ) { return 0 ; }
int pmux_terminal_receive( novacom_device_t *dev, char *buf ) { return 0 ; }

int pmux_program_run( novacom_device_t *dev, char *cmd, uint32 argc, char **argv ) { return 0 ; }

int pmux_mem_put( novacom_device_t *dev, uint32 addr, uint32 data ) { return 0 ; }
int pmux_mem_boot( novacom_device_t *dev, uint32 addr ) { return 0 ; }

int main () {
    
    int ret;
    
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
    for(;;) {
        ret = error_check( novacom_packet_read( dev, USB_BUFLEN, USB_TIMEOUT ), 0, "Timeout or error reading USB!\n" ) ;
        if( ret > 0 ) {
	    if (dev->state!=STATE_TERMINAL) {
                printf( "Read %i bytes - success!\n", ret ) ;
            }
            novacom_packet_process( dev, ret ) ;
        }
	if (dev->state==STATE_TERMINAL) {
	  char buf[1024];
	  int n = read_input(buf,(sizeof buf)-1);
	  buf[n] = '\0';
	  if (dev->state!=STATE_TERMINAL) {
	      fprintf(stderr,"terminal input: '%s'\n",buf);
	  }
	  if (n>0) {
	    pmux_write_tty(dev,buf,n);
	  }
	}
    }
    
    return 0 ;
}

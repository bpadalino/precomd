/* 
  To compile:
    $ gcc -o precomd precomd.c -lusb
*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <usb.h>
#include <termios.h>
#include <assert.h>

#include "novacom.h"
#include "novacom_private.h"

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
                        if (is_interface_novacom(&(dev->config[c].interface[i].altsetting[a]))) {
                            /* Open the device, set the alternate setting, claim the interface and do your processing */
                            // fprintf(stderr, "Novacom found!\n") ;
                            retval = usb_open( dev ) ;

                            if( (ret=usb_claim_interface(retval, i)) < 0 ) {
                                fprintf(stderr, "Error claiming interface %i: %i\n", i, ret ) ;
                                exit( 1 ) ;
                            }

                            if( (ret=usb_set_altinterface( retval, a)) < 0 ) {
                                fprintf(stderr, "Error setting altinterface %i: %i\n", a, ret ) ;
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

int is_interface_novacom(struct usb_interface_descriptor *interface)
{
    return interface->bInterfaceClass == NOVACOM_USB_CLASS &&
           interface->bInterfaceSubClass == NOVACOM_USB_SUBCLASS &&
           interface->bInterfaceProtocol == NOVACOM_USB_PROTOCOL;
}

int novacom_init( novacom_device_t *dev ) {

    if( (dev->phone=novacom_find_endpoints( &(dev->ep_rx), &(dev->ep_tx) )) == 0 ) {
        fprintf( stderr, "ERROR: Novacom not found - are you sure your pre is plugged in?\n" ) ;
        return -1 ;
    }

    fprintf(stderr,"Novacom found: bulk_ep_in: 0x%2.2x bulk_ep_out: 0x%2.2x\n\n", dev->ep_rx, dev->ep_tx ) ;

    dev->state = STATE_WAIT_ANNOUNCE;
    dev->id_device = 0;
    dev->channel_num = 0;
    dev->mode = MODE_OPEN_TTY;
    dev->file_path = 0;
    dev->exit_code = 0;
    
    return 0 ;
}

void print_buf( char *buf, uint32 size ) {
    int x ;
    char c ;
    fprintf(stderr, "\nhex:\n\n    ") ;
    for( x = 0 ; x < size ; x++ ) {
        fprintf(stderr, "%2.2x ", buf[x]&0xff ) ;
        if( (x+1) % 8 == 0 ) fprintf(stderr,"\n    " ) ;
    }
    fprintf(stderr, "\nascii:\n\n    ") ;
    for( x = 0 ; x < size ; x++ ) {
        c = (char)buf[x]&0xff ;
        c = (c < 32 || c > 126) ? '.' : c ;
        fprintf(stderr, "%c ", c ) ;
        if( (x+1) % 8 == 0 ) fprintf(stderr, "\n    " ) ;
    }
    fprintf(stderr, "\n" ) ;

    return ;
}

void novacom_payload_print( uint32 command, uint8 payload[], uint32 size ) {
    switch( command ) {
        case NOVACOM_CMD_NOP           :
            fprintf(stderr, "  nduid: %s\n", ((novacom_nop_t *)payload)->nduid ) ;
            break ;
        case NOVACOM_CMD_ANNOUNCEMENT  :
            fprintf(stderr, "  nduid: %s\n", ((novacom_announcement_t *)payload)->nduid ) ;
            break ;
        case NOVACOM_CMD_PMUX         :
            print_buf( (char *)payload, size ) ;
    }

    return ;

}

int novacom_packet_read( novacom_device_t *dev, uint32 size, uint32 timeout ) {
    return usb_bulk_read( dev->phone, dev->ep_rx, (int8 *)&dev->packet, size, timeout ) ;
}

int novacom_packet_write( novacom_device_t *dev, uint32 size, uint32 timeout ) {
    if (dev->state!=STATE_TTY) {
        fprintf(stderr,"Writing packet of %d bytes\n",size);
    }
    return usb_bulk_write( dev->phone, dev->ep_tx, (int8 *)&dev->packet, size, timeout ) ;
}

void novacom_packet_print( novacom_packet_t *packet, uint32 size ) {

    fprintf(stderr, "magic string   : 0x%8.8x\n", packet->magic ) ;
    fprintf(stderr, "version        : 0x%8.8x\n", packet->version ) ;
    fprintf(stderr, "sender id      : 0x%8.8x\n", packet->id_tx ) ;
    fprintf(stderr, "receiver id    : 0x%8.8x\n", packet->id_rx ) ;
    fprintf(stderr, "command        : " ) ;
    switch( packet->command ) {
        case NOVACOM_CMD_NOP            :
        case NOVACOM_CMD_ANNOUNCEMENT   :
        case NOVACOM_CMD_PMUX          :
                                            fprintf(stderr, "%s\n", NOVACOM_COMMANDS[packet->command] ) ;
                                            novacom_payload_print( packet->command, packet->payload, size-sizeof(novacom_packet_t) ) ;
                                            break ;
        default                         :   fprintf(stderr, "UNKNOWN - 0x%8.8xi\n", packet->command ) ;
    }

    fprintf(stderr, "\n" ) ;

    return ;
}

static char dummy_serial[]  = "0c698734f53d02dcf0b0fa2335de38992e65d9ce";

int novacom_reply_nop( novacom_device_t *dev, uint32 len, const char *serial ) {
    if (dev->state!=STATE_TTY) {
        fprintf(stderr,"Sending NOP\n");
    }
    novacom_nop_t *nop = (novacom_nop_t *) &(dev->packet.payload) ;
    dev->packet.magic = PMUX_HEADER_MAGIC;
    dev->packet.version = 1;
    dev->packet.command = NOVACOM_CMD_NOP;
    dev->packet.id_tx = dev->id_host ;
    dev->packet.id_rx = dev->id_device ;    
    sprintf( nop->nduid, serial) ;
    return novacom_packet_write( dev, len, USB_TIMEOUT ) ;
}

int novacom_reply_announcement(novacom_device_t *dev) {
    fprintf(stderr,"Sending announcement\n");
    novacom_announcement_t *announce = (novacom_announcement_t *) &(dev->packet.payload) ;
    dev->packet.magic = PMUX_HEADER_MAGIC;
    dev->packet.version = 1;
    dev->packet.command = NOVACOM_CMD_ANNOUNCEMENT;
    dev->packet.id_tx = dev->id_host ;
    dev->packet.id_rx = dev->id_device ;
    sprintf( announce->nduid, dummy_serial) ;
    announce->mtu = 16384 ;
    announce->heartbeat = 1000 ;
    announce->timeout = 10000 ;
    return novacom_packet_write( dev, 72, USB_TIMEOUT ) ;
}

static void print_payload(uint8 *payload, uint32 len)
{
    int i = 0;
    for (;i!=len;++i) {
        fprintf(stderr," %02x",payload[i]);
    }
}

static void pmux_print(pmux_packet_t *pmux)
{
    fprintf(stderr,"pmux version: %02x\n",pmux->version);
    fprintf(stderr,"pmux ack_synx: %02x\n",pmux->ack_synx);
    fprintf(stderr,"pmux flags: %04x\n",pmux->flags);
    fprintf(stderr,"pmux channel_num: %04x\n",pmux->channel_num);
    fprintf(stderr,"pmux sequence_num: %08x\n",pmux->sequence_num);
    fprintf(stderr,"pmux length_payload: %08x\n",pmux->length_payload);
    fprintf(stderr,"pmux length_pmux_packet: %08x\n",pmux->length_pmux_packet);
    if (pmux->length_payload >= 4) {
      pmux_data_payload_t *data = (pmux_data_payload_t *)pmux->payload;
      if (data->magic==PMUX_DATA_MAGIC) {
        fprintf(stderr,"pmux payload\n");
        fprintf(stderr,"  version: %08x\n",data->version);
        fprintf(stderr,"  type: %08x\n",data->type);
        fprintf(stderr,"  payload: ");
        if (data->type==PMUX_HEADER_TYPE_OOB) {
            fprintf(stderr,"  oob\n");
            pmux_oob_t *oob = (pmux_oob_t *)data->payload;
            fprintf(stderr,"    type: %08x\n",oob->type);
            fprintf(stderr,"    payload: %08x\n",oob->payload);
            fprintf(stderr,"    zero: %08x\n",oob->zero[0]);
            fprintf(stderr,"    zero: %08x\n",oob->zero[1]);
            fprintf(stderr,"    zero: %08x\n",oob->zero[2]);
        }
        else {
          print_payload(data->payload,data->length);
          fprintf(stderr,"\n");
        }
        return;
      }
      else {
          fprintf(stderr,"not data magic: %08x\n",data->magic);
      }
    }
    fprintf(stderr,"pmux payload:");
    print_payload(pmux->payload,pmux->length_payload);
    fprintf(stderr,"\n");
}


static void pmux_ack(novacom_device_t *dev, uint32 seq_num)
{
    if (dev->state!=STATE_TTY) {
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
        pmux->version = PMUX_MODE_NORMAL;
        pmux->pad = PMUX_OUT;
        pmux->ack_synx = PMUX_ACK;
        pmux->flags = dev->pmux_flags;
        pmux->channel_num = dev->channel_num;
        pmux->sequence_num = seq_num;
        pmux->length_payload = 0x00;
        pmux->length_pmux_packet = 0x1c;
        pmux->zero = 0x00000000;
        len += sizeof(*pmux);
    }
    novacom_packet_write(dev,len,USB_TIMEOUT);
}


static void pmux_write_tty(novacom_device_t *dev,char *str,uint32 str_len)
{
    int len = 0;

    if (dev->state!=STATE_TTY) {
        fprintf(stderr,"Sending text\n");
    }
    dev->packet.id_tx = dev->id_host ;
    dev->packet.id_rx = dev->id_device ;
    dev->packet.command = NOVACOM_CMD_PMUX ;
    len += sizeof(dev->packet);
    int cmd_len = sizeof(pmux_data_payload_t)+str_len;
    pmux_packet_t *pmux = (pmux_packet_t *)dev->packet.payload;
    pmux->magic = PMUX_ASCII_MAGIC;
    pmux->version = PMUX_MODE_NORMAL;
    pmux->pad = PMUX_OUT;
    pmux->ack_synx = PMUX_SYN;
    pmux->flags = PMUX_ESTABLISHED;
    dev->pmux_flags = PMUX_ESTABLISHED;
    pmux->channel_num = dev->channel_num;
    pmux->sequence_num = dev->pmux_tty_seq_num;
    pmux->length_payload = cmd_len;
    pmux->length_pmux_packet = 0x1c+cmd_len;
    pmux->zero = 0x00000000;
    pmux_data_payload_t *tty = (pmux_data_payload_t *)pmux->payload;
    tty->magic = PMUX_DATA_MAGIC;
    tty->version = 0x00000001;
    tty->length = str_len;
    tty->type = 0x00000000;
    memcpy(tty->payload,str,str_len);
    len += sizeof(*pmux);
    len += cmd_len;

    novacom_packet_write(dev,len,USB_TIMEOUT);
}


static void pmux_write_command(novacom_device_t *dev,const char *cmd,const char *arg)
{
    fprintf(stderr,"Sending %s\n",cmd);
    int len = 0;
    dev->packet.id_tx = dev->id_host ;
    dev->packet.id_rx = dev->id_device ;
    dev->packet.command = NOVACOM_CMD_PMUX ;
    len += sizeof(dev->packet);
    {
        int cmd_len = strlen((char *)cmd)+1+strlen((char *)arg);
        pmux_packet_t *pmux = (pmux_packet_t *)dev->packet.payload;
        pmux->magic = PMUX_ASCII_MAGIC;
        pmux->version = PMUX_MODE_NORMAL;
        pmux->pad = PMUX_OUT;
        pmux->ack_synx = PMUX_SYN;
        pmux->flags = PMUX_ESTABLISHED;
        dev->pmux_flags = PMUX_ESTABLISHED;
        pmux->sequence_num = 1;
        pmux->channel_num = dev->channel_num;
        dev->pmux_tty_seq_num = 1;
        pmux->length_payload = cmd_len;
        pmux->length_pmux_packet = 0x1c+cmd_len;
        pmux->zero = 0x00000000;
        {
            char *payload = (char *)pmux->payload;
            sprintf(payload,"%s %s",cmd,arg);
            assert(strlen(payload)==cmd_len);
        }
        len += sizeof(*pmux);
        len += cmd_len;
    }
    novacom_packet_write(dev,len,USB_TIMEOUT);
}


static void print_payload_str(FILE *file,uint8 *payload,uint32 len)
{
    int i = 0;
    for (;i<len;++i) {
        fprintf(file,"%c",payload[i]);
    }
}


static int read_input(char *buf,uint32 buf_size)
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


int pmux_send_control(novacom_device_t *dev,uint32 seq_num,uint32 cmd)
{
    int len = 0;

    dev->packet.id_tx = dev->id_host ;
    dev->packet.id_rx = dev->id_device ;
    dev->packet.command = NOVACOM_CMD_PMUX ;
    len += sizeof(dev->packet);
    {
        pmux_packet_t *pmux = (pmux_packet_t *)dev->packet.payload;
        pmux->magic = PMUX_ASCII_MAGIC;
        pmux->version = PMUX_MODE_NORMAL;
        pmux->pad = PMUX_OUT;
        pmux->ack_synx = PMUX_SYN;
        pmux->flags = PMUX_NOT_CONNECTED;
        dev->pmux_flags = PMUX_NOT_CONNECTED;
        pmux->channel_num = dev->channel_num;
        pmux->sequence_num = seq_num;
        pmux->length_payload = 0x0c;
        pmux->length_pmux_packet = 0x28;
        pmux->zero = 0x00000000;
        len += sizeof(*pmux);
        {
            pmux_control_payload_t *pmux_open = (pmux_control_payload_t *)pmux->payload;
            pmux_open->command = cmd;
            pmux_open->hex1000 = 0x00001000;
            pmux_open->hex000c = 0x0000000c;
            len += sizeof(*pmux_open);
        }
    }
    return novacom_packet_write( dev, len, USB_TIMEOUT ) ;
}

void pmux_close(novacom_device_t *dev)
{
    fprintf(stderr,"Closing terminal\n");
    pmux_send_control(dev,2,PMUX_CMD_CLOSE);
    pmux_send_control(dev,3,1);
    dev->state = STATE_CLOSING;
}


void pmux_terminal_open( novacom_device_t *dev ) 
{ 
    fprintf(stderr,"Opening terminal\n");
    pmux_send_control(dev,1,PMUX_CMD_OPEN);
    dev->state = STATE_OPEN_ACK;
}


int pmux_packet_process( novacom_device_t *dev ) { 
    pmux_packet_t *pmux = (pmux_packet_t *) &(dev->packet.payload) ;
    
    if (pmux->magic != PMUX_ASCII_MAGIC) {
        fprintf(stderr,"Invalid pmux magic: %08x\n",pmux->magic);
        exit(1) ;
    }

    if (pmux->version != PMUX_MODE_NORMAL) {
        fprintf(stderr,"Invalid pmux version\n");
        exit(1);
    }

    if (pmux->ack_synx==PMUX_ACK) {
        if (dev->state!=STATE_TTY) {
            fprintf(stderr,"Got ack for %d\n",pmux->sequence_num);
            if (dev->state==STATE_CLOSING && pmux->sequence_num>=3) {
                dev->state=STATE_CLOSED;
            }
        }
        if (dev->state==STATE_OPEN_ACK) {
            if (dev->mode==MODE_OPEN_TTY) {
                fprintf(stderr,"Opening tty\n");
                pmux_write_command(dev,"open","tty://");
            }
            else if (dev->mode==MODE_GET_FILE) {
                fprintf(stderr,"Getting file\n");
                pmux_write_command(dev,"get",dev->file_path);
            }
            else if (dev->mode==MODE_PUT_FILE) {
                fprintf(stderr,"Putting file\n");
                pmux_write_command(dev,"put",dev->file_path);
            }
            dev->state=STATE_COMMAND_ACK;
        }
        else if (dev->state==STATE_COMMAND_ACK) {
            fprintf(stderr,"Going to wait ok state\n");
            dev->state=STATE_WAIT_OK;
            ++dev->pmux_tty_seq_num;
        }
        else if (dev->state==STATE_TTY) {
            ++dev->pmux_tty_seq_num;
        }
    }
    else {
        if (pmux->payload[0]==PMUX_CMD_CLOSE) {
            fprintf(stderr,"Channel closed.\n");
            pmux_ack(dev,pmux->sequence_num);
            dev->state=STATE_CLOSED;
        }
        else if (dev->state==STATE_WAIT_OK) {
            fprintf(stderr,"Got response: '");
            print_payload_str(stderr,pmux->payload,pmux->length_payload);
            fprintf(stderr,"'\n");
            pmux_ack(dev,pmux->sequence_num);
            {
                char *payload = (char *)pmux->payload;
                if (strncmp(payload,"ok ",3)==0) {
                    dev->state = STATE_TTY;
                    fprintf(stderr,"Going to tty state\n");
                }
                else {
                    dev->exit_code = 1;
                    pmux_close(dev);
                    fprintf(stderr,"Closing\n");
                }
            }
        }
        else if (dev->state==STATE_TTY) {
            pmux_data_payload_t *tty = (pmux_data_payload_t *)pmux->payload;
            if (tty->magic!=PMUX_DATA_MAGIC) {
                fprintf(stderr,"Unknown control.\n");
                pmux_print(pmux);
                dev->pmux_flags = pmux->flags;
                pmux_ack(dev,pmux->sequence_num);
                dev->state=STATE_LIMBO;
            }
            else {
                if (tty->type==PMUX_HEADER_TYPE_OOB) {
                    fprintf(stderr,"Received oob ");
                    pmux_oob_t *oob = (pmux_oob_t *)tty->payload;
                    if (oob->type==PMUX_OOB_EOF) {
                        fprintf(stderr,"eof");
                    }
                    else if (oob->type==PMUX_OOB_RETURN) {
                        fprintf(stderr,"return");
                    }
                    fprintf(stderr," %d\n",oob->payload);
                    if (oob->type==PMUX_OOB_RETURN) {
                        pmux_close(dev);
                    }
                }
                else if (tty->type==PMUX_HEADER_TYPE_ERR) {
                    fprintf(stderr,"Received error\n");
                    pmux_print(pmux);
                }
                else {
                  int len = tty->length;
                  print_payload_str(stdout,tty->payload,len);
                  fflush(stdout);
                }
                pmux_ack(dev,pmux->sequence_num);
            }
        }
        else {
            fprintf(stderr,"Unexpected syn\n");
            pmux_print(pmux);
            pmux_ack(dev,pmux->sequence_num);
#if 0
            pmux_terminal_open(dev);
#endif
        }
    }

    return 0 ;
}

static uint32 dummy_host_id = 0x00004ae1 ;

int novacom_packet_process( novacom_device_t *dev, uint32 len ) {
    // TODO: Perform checking for magic number and other header
    // information here
    switch ( dev->packet.command ) {
        case NOVACOM_CMD_NOP    :
            {
                novacom_packet_t *packet = &dev->packet;
                if (dev->state!=STATE_TTY) {
                    fprintf(stderr,"Got NOP: id_tx=%x, id_rx=%x\n",packet->id_tx,packet->id_rx);
                }
                if (dev->state!=STATE_WAIT_ANNOUNCE) {
                    novacom_reply_nop( dev, len, dummy_serial ) ;
                }
                else if (dev->state==STATE_WAIT_ANNOUNCE) {
                    fprintf(stderr,"Not sending NOP reply -- waiting for announce\n");
                }
            }
            break ;

        case NOVACOM_CMD_ANNOUNCEMENT :
            // TODO: Randomize this value
            dev->id_host   = dummy_host_id ;
            dev->id_device = dev->packet.id_tx ;
            fprintf(stderr, "Got announcement\n" ) ;
            if (novacom_reply_announcement(dev)<0) {
                return -1;
            }
            dev->state = STATE_GOT_ANNOUNCE;
            pmux_terminal_open(dev);
            break ;

        case NOVACOM_CMD_PMUX :
            if (dev->state!=STATE_TTY) {
                fprintf(stderr, "Processing PMUX packet\n" ) ;
            }
            return pmux_packet_process( dev ) ;
            break ;

        default :
            fprintf(stderr, "Not sure wtf to do\n" ) ;
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


static void make_raw_tty(int fd,struct termios *orig)
{
    struct termios attr;
    tcgetattr(fd,&attr);
    *orig = attr;
    attr.c_lflag &= ~ICANON;
    attr.c_lflag &= ~ECHO;
    attr.c_cc[VMIN] = 1;
    attr.c_cc[VTIME] = 0;
    attr.c_cc[VEOF] = 0;
    attr.c_cc[VINTR] = 0;
    attr.c_cc[VQUIT] = 0;
    tcsetattr(fd,TCSANOW,&attr);
}

static void restore_tty(int fd,struct termios *attrs)
{
    tcsetattr(fd,TCSANOW,attrs);
}

int pmux_file_put( novacom_device_t *dev ) { return 0 ; }
int pmux_file_get( novacom_device_t *dev ) { return 0 ; }

int pmux_terminal_close( novacom_device_t *dev ) { return 0 ; }
int pmux_terminal_send( novacom_device_t *dev, char *cmd ) { return 0 ; }
int pmux_terminal_receive( novacom_device_t *dev, char *buf ) { return 0 ; }

int pmux_program_run( novacom_device_t *dev, uint32 argc, char **argv ) { return 0 ; }

int pmux_mem_put( novacom_device_t *dev, uint32 addr, uint32 data ) { return 0 ; }
int pmux_mem_boot( novacom_device_t *dev, uint32 addr ) { return 0 ; }

int main (int argc, char **argv) {
    
    int ret;
    struct termios orig_tty_attr;

    /* The USB device with other relavent information */ 
    novacom_device_t *dev = (novacom_device_t *)malloc( sizeof(novacom_device_t)+USB_BUFLEN ) ;
    
    /* Check to see if we're root before we really get into this thing */
    if( geteuid() != 0 ) {
        fprintf( stderr, "ERROR: root required for USB privilidges.  Please re-run command as root.\n" ) ;
        exit( 1 ) ;
    }
    
    /* Initialize novacom communications */
    error_check( novacom_init( dev ), 1, "Unable to find or initialize Novacom - is your pre plugged in?\n" ) ;
    if (argc==1) {
        dev->mode = MODE_OPEN_TTY;
    }
    else {
        int is_get = strcmp(argv[1],"get")==0;
        int is_put = strcmp(argv[1],"put")==0;
        if (is_get || is_put) {
            if (argc<3) {
                fprintf(stderr,"No url specified for %s\n",argv[1]);
                exit(1);
            }
            dev->file_path = argv[2];
            dev->mode = is_get ? MODE_GET_FILE : MODE_PUT_FILE;
        }
    }


    if (dev->mode==MODE_OPEN_TTY) {
        make_raw_tty(0,&orig_tty_attr);
    }

    /* Iterate through a NOP loop */
    while (dev->state!=STATE_CLOSED) {
#if 0
        ret = error_check( novacom_packet_read( dev, USB_BUFLEN, USB_TIMEOUT ), 0, "Timeout or error reading USB!\n" ) ;
#else
        ret = novacom_packet_read( dev, USB_BUFLEN, USB_TIMEOUT );
#endif

        if( ret > 0 ) {
            if (dev->state!=STATE_TTY) {
                    fprintf(stderr, "Read %i bytes - success!\n", ret ) ;
            }
            novacom_packet_process( dev, ret ) ;
        }
        if (dev->state==STATE_TTY) {
            if (dev->mode==MODE_OPEN_TTY) {
                char buf[1024];
                int n = read_input(buf,(sizeof buf)-1);
                buf[n] = '\0';
                if (dev->state!=STATE_TTY) {
                    fprintf(stderr,"terminal input: '%s'\n",buf);
                }
                if (n>0) {
                    pmux_write_tty(dev,buf,n);
                }
            }
            else if (dev->mode==MODE_PUT_FILE) {
                char buf[1024];
                int n_read = fread(buf,1,sizeof buf,stdin);
                if (n_read>0) {
                    pmux_write_tty(dev,buf,n_read);
                }
                else {
                    pmux_close(dev);
                }
            }
        }
    }
    
    if (dev->mode==MODE_OPEN_TTY) {
        restore_tty(0,&orig_tty_attr);
    }
    
    return dev->exit_code;
}

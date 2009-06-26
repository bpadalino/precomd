/*
 * novacom.h
 *
 *  Created on: Jun 24, 2009
 *      Author: mike.gaffney
 *
 * This is the public interface for the novacom driver
 * it mainly exposes the pmux layer and the structures
 * needed to develop against it.
 *
 * There is also a private header file which is mainly
 * used to expose testing interfaces. If you're not working
 * IN novacom but with WITH novacom, you should be using this.
 */

#ifndef NOVACOM_H_
#define NOVACOM_H_

#define uint32 unsigned int
#define uint16 short

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


int pmux_packet_process( novacom_device_t *);
int pmux_file_put( novacom_device_t *);
int pmux_file_get( novacom_device_t *);

int pmux_terminal_open( novacom_device_t *);
int pmux_terminal_close( novacom_device_t *);
int pmux_terminal_send( novacom_device_t *, char *);
int pmux_terminal_receive( novacom_device_t *, char *);

int pmux_program_run( novacom_device_t *, uint32 , char **);

int pmux_mem_put( novacom_device_t *, uint32 , uint32);
int pmux_mem_boot( novacom_device_t *, uint32);

#endif /* NOVACOM_H_ */

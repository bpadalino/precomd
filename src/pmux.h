/*
 * pmux.h
 *
 *  Created on: Jun 24, 2009
 *      Author: mike.gaffney
 */

#ifndef PMUX_H_
#define PMUX_H_

#define uint32 unsigned int
#define uint16 short

#include "novacom.h"

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
    char payload[0] ;
} pmux_tty_payload_t ;

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

int pmux_packet_process( novacom_device_t *);
int pmux_file_put( novacom_device_t *);
int pmux_file_get( novacom_device_t *);

int pmux_terminal_open( novacom_device_t *);
int pmux_terminal_close( novacom_device_t *);
int pmux_terminal_send( novacom_device_t *, char *);
int pmux_terminal_receive( novacom_device_t *, char *);

int pmux_program_run( novacom_device_t *, char *, uint32 , char **);

int pmux_mem_put( novacom_device_t *, uint32 , uint32);
int pmux_mem_boot( novacom_device_t *, uint32);

#endif /* PMUX_H_ */

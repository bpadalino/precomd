/*
 * pmux.c
 *
 *  Created on: Jun 24, 2009
 *      Author: mike.gaffney
 */

#include "pmux.h"


int pmux_packet_process( novacom_device_t *dev ) {
    static uint32 seq_rx = 0 ;
    static uint32 seq_tx = 0 ;
    pmux_packet_t *pmux_packet = (pmux_packet_t *) &(dev->packet.payload) ;

    if( pmux_packet->magic != PMUX_HEADER_MAGIC ) exit( 1 ) ;

    if( seq_rx > pmux_packet->sequence_num ) exit( 1 ) ;

    if( seq_tx > pmux_packet->sequence_num ) exit( 1 ) ;

    // Check mode and process

    // Check syn/ack and process

    return 0 ;
}

int pmux_file_put( novacom_device_t *dev ) { return 0 ; }
int pmux_file_get( novacom_device_t *dev ) { return 0 ; }

int pmux_terminal_open( novacom_device_t *dev ) { return 0 ; }
int pmux_terminal_close( novacom_device_t *dev ) { return 0 ; }
int pmux_terminal_send( novacom_device_t *dev, char *cmd ) { return 0 ; }
int pmux_terminal_receive( novacom_device_t *dev, char *buf ) { return 0 ; }

int pmux_program_run( novacom_device_t *dev, uint32 argc, char **argv ) { return 0 ; }

int pmux_mem_put( novacom_device_t *dev, uint32 addr, uint32 data ) { return 0 ; }
int pmux_mem_boot( novacom_device_t *dev, uint32 addr ) { return 0 ; }

#include "fct.h"
#include <novacom.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>


FCT_BGN()
{
 /* A simple test case. No setup/teardown. */
 FCT_SUITE_BGN(integration)
 {
	FCT_TEST_BGN(test_is_novacom_interface)
	{
		struct usb_interface_descriptor novacom_descriptor = {0, 0, 0, 0, 0, NOVACOM_USB_CLASS, NOVACOM_USB_SUBCLASS, NOVACOM_USB_PROTOCOL};
		struct usb_interface_descriptor non_novacom_descriptor = {0, 0, 0, 0, 0, 0, NOVACOM_USB_SUBCLASS, NOVACOM_USB_PROTOCOL};

		fct_chk(is_interface_novacom(&novacom_descriptor));
		fct_chk(!is_interface_novacom(&non_novacom_descriptor));
	}
	FCT_TEST_END();

	FCT_TEST_BGN(test_open_pre_ping_and_close)
	{
		/* The USB device with other relavent information */
			novacom_device_t *dev = (novacom_device_t *)malloc( sizeof(novacom_device_t)+USB_BUFLEN ) ;

			/* Check to see if we're root before we really get into this thing */
			fct_chk(geteuid() == 0);

			/* Initialize novacom communications */
			int novacom_init_val = novacom_init( dev );
			printf("init value: %i\n", novacom_init_val);
			fct_chk(novacom_init_val >= 0);

			/* Iterate through a NOP loop */
			int i;
			int novacom_packet_read_return;
			for(i = 0 ; i  < 1 ; i++ ) {
				printf( "Iteration: %i\n", i ) ;
				novacom_packet_read_return = novacom_packet_read( dev, USB_BUFLEN, USB_TIMEOUT );
				fct_chk(novacom_packet_read_return >= 0);
				novacom_packet_read_return = error_check( novacom_packet_read( dev, USB_BUFLEN, USB_TIMEOUT ), 0, "Timeout or error reading USB!\n" ) ;
				if( novacom_packet_read_return > 0 ) {
					printf( "Read %i bytes - success!\n", novacom_packet_read_return ) ;
					novacom_packet_process( dev, novacom_packet_read_return ) ;
				}
			}
	}
	FCT_TEST_END();

 /* Every test suite must be closed. */
 }
 FCT_SUITE_END();

/* Every FCT scope has an end. */
}
FCT_END();

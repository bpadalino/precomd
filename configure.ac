AC_PREREQ(2.60)

AC_INIT(src/novacom.c)

AC_CONFIG_AUX_DIR([config])
AC_CONFIG_HEADER([src/config.h])

AM_INIT_AUTOMAKE(precomd,0.1)

AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL

AC_CONFIG_FILES([Makefile 
				 src/Makefile
				 test/Makefile])

AC_CHECK_LIB([usb], [usb_init], [], [echo "libusb-devel is required"; exit -1])

AC_CHECK_LIB([usb], [usb_init])

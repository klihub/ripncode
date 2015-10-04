SUBDIRS      = .
bin_PROGRAMS = 

AM_CFLAGS    = $(WARNING_CFLAGS) $(AM_CPPFLAGS) -I$(top_srcdir) -I$(top_builddir)


#########################
# ripncode
#

bin_PROGRAMS += ripncode

ripncode_SOURCES =		\
	ripncode.c

ripncode_CFLAGS =		\
	$(AM_CFLAGS)		\
	$(MURPHY_CFLAGS)	\
	$(CDIO_CFLAGS)

ripncode_LDADD =		\
	$(MURPHY_LIBS)		\
	$(CDIO_LIBS)


#########################
# rnc
#

bin_PROGRAMS += rnc

rnc_SOURCES =			\
	setup.c			\
	format.c		\
	device.c		\
	device-cdparanoia.c	\
	encoder.c		\
	encoder-flac.c		\
	rnc.c

rnc_CFLAGS =			\
	$(AM_CFLAGS)		\
	$(MURPHY_CFLAGS)	\
	$(CDIO_CFLAGS)		\
	$(FLAC_CFLAGS)

rnc_LDADD =			\
	$(MURPHY_LIBS)		\
	$(CDIO_LIBS)		\
	$(FLAC_LIBS)


SUBDIRS         = .
bin_PROGRAMS    =
noinst_PROGRAMS =

AM_CFLAGS = $(WARNING_CFLAGS) $(AM_CPPFLAGS) -I$(top_srcdir) -I$(top_builddir)


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
	buffer.c		\
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


#########################
# tests
#

if HAVE_CHECK
TESTS =
noinst_PROGRAMS += $(TESTS)

# buffer-test
TESTS += buffer-test

buffer_test_SOURCES =		\
	buffer.c		\
	tests/buffer-test.c

buffer_test_CFLAGS =		\
	$(AM_CFLAGS)		\
	$(CHECK_CLAGS)

buffer_test_LDADD =		\
	$(MURPHY_LIBS)		\
	$(CHECK_LIBS)

check: $(TESTS)
	for t in $(TESTS); do $$t; done

endif

noinst_PROGRAMS += seek-test

seek_test_SOURCES =		\
	buffer.c		\
	tests/seek-test.c

seek_test_CFLAGS =		\
	$(AM_CFLAGS)		\
	$(CHECK_CLAGS)

seek_test_LDADD =		\
	$(MURPHY_LIBS)		\
	$(CHECK_LIBS)

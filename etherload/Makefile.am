#
# $Header$
#
# Makefile.am for etherload.
#
# FIXME: need to define the OS in CFLAGS to one of the following:
#
##
# Also define one of the following in CFLAGS (see os.h)
#	IRIX40, IRIX51, SUNOS4, SUNOS5, SVR4, ULTRIX, OSF1,
#	FREEBSD, BSDI, LINUX
#	

## global variables
INCLUDES = -I$(top_srcdir)/include
LDFLAGS = -L$(top_srcdir)/lib
LDADD = -lsnips

CLEANFILES = ethertest

bin_PROGRAMS = etherload
etherload_SOURCES =	bpf.c  etherload.c  dlpi.c  main.c  nit.c \
			pfilt.c snoop.c linux.c util.c \
			snips_specific.c get_devlist.c

check_PROGRAMS = ethertest
ethertest_SOURCES = $(etherload_SOURCES)

INITDIR = $(prefix)/init.d
INITFILE_SRC = $(bindir)/snipsprog.init
INITFILE = etherload.init

install-exec-hook:
	@$(mkinstalldirs) $(INITDIR)
	@if test -f $(INITDIR)/$(INITFILE); then : ; else \
	  $(LN_S) $(INITFILE_SRC) $(INITDIR)/$(INITFILE); \
	fi

ethertest:
	@/bin/rm -f etherload main.o snips_specific.o util.o
	@$(MAKE) CFLAGS="$(CFLAGS) -DSTANDALONE" etherload
	@if [ -f etherload ]; then mv etherload ethertest ; fi

install-root: etherload
	@if [ `whoami` = "root" ]; then \
		chmod 4750 $(bindir)/etherload ;\
		chown root $(bindir)/etherload ;\
		echo "set $(bindir)/etherload to suid root" ;\
	else \
		echo "You need to be root to install etherload suid" ;\
		exit 1; \
	fi

# $Id$
#
# Makefile for multiping
#
# 

bin_PROGRAMS = multiping
multiping_SOURCES = multiping.c

install-root: multiping
	@if [ `whoami` = "root" ]; then \
		chmod 4750 $(bindir)/multiping ;\
		chown root $(bindir)/multiping ;\
		echo "set $(bindir)/multiping to suid root" ;\
	else \
		echo "You need to be root to install multiping suid" ;\
		exit 1; \
	fi

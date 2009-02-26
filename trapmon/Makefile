PROG = trapmon

SRCS = trapmon.c snips_specific.c

include ../buildsys.mk
include ../misc.defs.mk

$(top_srcdir)/cmu-snmp/snmp/libsnmp.a:
	$(MAKE) -C $(top_srcdir)/cmu-snmp/snmp libsnmp.a

LIBS += $(top_srcdir)/cmu-snmp/snmp/libsnmp.a
trapmon: $(top_srcdir)/cmu-snmp/snmp/libsnmp.a

CPPFLAGS += -I../cmu-snmp

clean: clean-cmu-snmp

clean-cmu-snmp:
	$(MAKE) -C ../cmu-snmp/snmp clean

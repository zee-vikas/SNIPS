SUBDIRS = include lib etherload nsmon ntpmon pingmon portmon radiusmon \
	snipslog snipstv tpmon trapmon tksnips snipsweb man utility \
	utility/eventselect utility/display_snips_datafile

include buildsys.mk
include misc.defs.mk

%.mk: %.mk.in config.status
	./config.status

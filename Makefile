SUBDIRS = include lib etherload nsmon ntpmon pingmon portmon radiusmon \
	snipslog snipstv tpmon trapmon tksnips snipsweb man utility

include buildsys.mk
include misc.defs.mk

%.mk: %.mk.in config.status
	./config.status

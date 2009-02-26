MAN_SRCS = $(wildcard man*/*.in)
MAN = $(patsubst %.in,%,$(MAN_SRCS))
mansubdir = .

CLEAN = $(MAN)

include ../buildsys.mk
include ../misc.defs.mk

all: $(MAN)

$(MAN): %: %.in prologue.man Makefile ../misc.defs.mk
	$(GENERATE_STATUS)
	if $(sed_subst) \
		-e '$$r prologue.man' \
		$< > $@; then \
		\
		$(GENERATE_OK); \
	else \
		rm -f $@; \
		$(GENERATE_FAILED); \
	fi

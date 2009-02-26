EXTRA =	crontab.snips \
	snipsprog.init

BUILT_SCRIPTS = \
	logmaint.pl \
	keepalive_monitors.pl \
	notifier.pl \
	snmpconf.pl

SCRIPTS = $(BUILT_SCRIPTS) \
	docrypt.pl \
	leapdates.pl \
	logstats.pl

CLEAN = $(BUILT_SCRIPTS) $(EXTRA)

all: $(CLEAN)

include ../buildsys.mk
include ../misc.defs.mk

$(SCRIPTS): %: %.in ../misc.defs.mk
	$(GENERATE_STATUS)
	if $(sed_subst) \
		$< > $@; then \
		chmod +x $@; \
		\
		$(GENERATE_OK); \
	else \
		rm -f $@; \
		$(GENERATE_FAILED); \
	fi

$(EXTRA): %: %.in ../misc.defs.mk
	$(GENERATE_STATUS)
	if $(sed_subst) \
		$< > $@; then \
		\
		$(GENERATE_OK); \
	else \
		rm -f $@; \
		$(GENERATE_FAILED); \
	fi

install:
	$(INSTALL) -d $(DESTDIR)$(libexecdir)/$(PACKAGE)
	for script in $(SCRIPTS); do \
		install -m 0755 $$script $(DESTDIR)$(libexecdir)/$(PACKAGE)/$$script; \
	done

	$(INSTALL) -d $(DESTDIR)$(datadir)/$(PACKAGE)
	for extra in $(EXTRA); do \
		install -m 0644 $$extra $(DESTDIR)$(datadir)/$(PACKAGE)/$$extra; \
	done

CGI = \
	genweb.cgi \
	genweb-filter.cgi \
	rrdgraph.cgi \
	snipsweb.cgi

EXTRA = snipsweb-confg

CLEAN = $(CGI) $(EXTRA)

CONFIG = snipsweb-confg updates webusers
HTMLDATA = chirp.au critical.wav error.wav warning.wav

all: $(CLEAN)

include ../buildsys.mk
include ../misc.defs.mk

$(CGI): %: %.in ../misc.defs.mk
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

install-extra:
	$(INSTALL) -d $(DESTDIR)$(libexecdir)/$(PACKAGE)
	for cgi in $(CGI); do \
		install -m 0755 $$cgi $(DESTDIR)$(libexecdir)/$(PACKAGE)/$$cgi; \
	done

	$(INSTALL) -d $(DESTDIR)$(ETCDIR)
	for config in $(CONFIG); do \
		install -m 0644 $$config $(DESTDIR)$(ETCDIR)/$$config; \
	done

	$(INSTALL) -d $(DESTDIR)$(datadir)/$(PACKAGE)/html
	for htmldata in $(HTMLDATA); do \
		install -m 0644 $$htmldata $(DESTDIR)$(datadir)/$(PACKAGE)/html/$$htmldata; \
	done

/* $Header$ */
/*
 * SNIPS perl interface.
 *
 * AUTHOR:
 *	Vikas Aggarwal (vikas@navya_.com)  June 2000
 *
 */

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"


MODULE = snips		PACKAGE = snips		PREFIX = snips_

PROTOTYPES: ENABLE

#define _MAIN_
#include "snips.h"
#undef _MAIN_
#include <stdio.h>

# We call the startup function which forks and sets up signal handlers.
# Need to setup the readconfig function so that the signal handler
# knows what to call to re-read the config file on HUP.
# NOTE NOTE: Different arguments as compared to the C routine.
int
snips_startup(readconf)
	void  *readconf;
  CODE:
  {
	debug = 2;
	prognm = "perltestmonitor";
	set_readconfig_function(readconf);  /* needed by reload */
	RETVAL = snips_startup(NULL, NULL);
  }
  OUTPUT:
	RETVAL


# Cleans up pid file and exits
void
snips_done()

  CODE:
  {
	snips_done();

  }

# Given old file handle, return new filehandle. Check for the value of
# the global variable  snips::do_reload to decide if should call
# snips_reload()
#
FILE *
snips_reload(oldfh)
	FILE *oldfh;

  CODE:
  {
	int newfd ;
	FILE *newfh;
	newfd = snips_reload(fileno(oldfh));
	if (newfd >= 0)
	{
	  newfh = fdopen(newfd, "r+");
	  /* sv_setsv(ST(0), (SV *)newfh); /* does not work */
	  RETVAL = newfh;
	}
	else
	  /* ST(0) = &sv_undef;	/* or XSRETURN_UNDEF */
	  RETVAL = NULL;
  }
  OUTPUT:
	RETVAL

char *
snips_get_datafile()

  CODE:
  {
	RETVAL = (char *)get_datafile();
  }
  OUTPUT:
	RETVAL

char *
snips_get_configfile()

  CODE:
  {
	RETVAL = (char *)get_configfile();
  }
  OUTPUT:
	RETVAL


# Read and write dataversion.
int
read_dataversion(fh)
	FILE *fh;

  CODE:
  {
	RETVAL = read_dataversion(fileno(fh));
  }
  OUTPUT:
	RETVAL

# Write dataversion
int
write_dataversion(fh)
	FILE *fh;

  CODE:
  {
	RETVAL = write_dataversion(fileno(fh));
  }
  OUTPUT:
	RETVAL

##
##  EVENT functions
##

# Create a new event. REMEMBER TO FREE??
EVENT *
new_packedevent()
  PREINIT:
	static EVENT v;
  CODE:
  {
	bzero(&v, sizeof (EVENT));
	RETVAL = &v;
  }
  OUTPUT:
	RETVAL

# Create a new event. REMEMBER TO FREE??
EVENT *
new_packedevent_FIX()
  PREINIT:
	EVENT *pv;
  CODE:
  {
	pv = (void *)malloc(sizeof(EVENT));
	bzero(pv, sizeof (EVENT));
	RETVAL = (EVENT *)newSVpv((char *)pv, sizeof(EVENT));
	free(pv);
  }
  OUTPUT:
	RETVAL

##
int
init_packedevent(pv)
	EVENT *pv;

  CODE:
  {
	RETVAL = init_event(pv);
  }
  OUTPUT:
	RETVAL


##
int
update_packedevent(pv, status, value, maxsev)
	EVENT *pv;
	int   status;
	unsigned long value;
	int   maxsev;

  CODE:
  {
	RETVAL = update_event(pv, status, value, maxsev);
  }
  OUTPUT:
	RETVAL


# Read one event from the open filehandle. Will need to unpack this
# structure in perl.
EVENT *
read_packedevent(fh)
	FILE *fh;
  PREINIT:
	static EVENT v;
  CODE:
  {
	if (read_event(fileno(fh), &v) > 0)
	{
	  RETVAL = &v;
	}
	else
	  /* ST(0) = &sv_undef; /* does not work */
	  RETVAL = NULL;
  }
  OUTPUT:
	RETVAL

# Write event to the open filehandle
int
write_packedevent(fh, pv)
	FILE *fh;
	EVENT *pv;
  CODE:
  {
	RETVAL = write_event(fileno(fh), pv);
  }
  OUTPUT:
	RETVAL

# Rewind's output file by one event, and then writes supplied event.
int
rewrite_packedevent(fh, pv)
	FILE *fh;
	EVENT *pv;
  CODE:
  {
	RETVAL = rewrite_event(fileno(fh), pv);
  }
  OUTPUT:
	RETVAL

# Convert a packed event into a hash.
# FIX FIX, CHECK FOR MEMORY LEAK in 'SV' (new without freeing prev?)
HV *
unpack_event(pv)
	EVENT *pv;
  CODE:
  {
	int i = 0;
	static char **keyarray;
	char **strarray;
	static SV *sv_value;
	static HV *hashevent ;

	/* if (SvREFCNT(sv_value))
	 * SvREFCNT_dec(sv_value);	/* free memory.. FIX */
	if (!hashevent)
		hashevent = newHV();
	else
		hv_clear(hashevent);	/* free old data */

	if (!keyarray)
		keyarray = (char **)event2strarray(NULL); /* once only */
	strarray = (char **)event2strarray(pv);
	for (i=0; keyarray[i] && *(keyarray[i]) ; ++i)
	{
		if (strarray[i] == NULL)
			strarray[i] = "";
		sv_value = newSVpv(strarray[i], 0);
		hv_store(hashevent, keyarray[i], strlen(keyarray[i]),
			  sv_value, /*hash value*/ 0);

	/*	fprintf(stderr, "(debug) unpack_event() %s, %s\n",
				 keyarray[i], strarray[i]); /* */
	}
	RETVAL = hashevent;
  }
  OUTPUT:
	RETVAL


# pack_event() not needed?

# open_datafile()  /* how to send flags and mode to C program? */
# close_datafile()

/* $Header$ */
/*
 * SNIPS perl interface.
 *
 * AUTHOR:
 *	Vikas Aggarwal (vikas@navya_.com)  June 2000
 *
 */
#ifdef __cpluscplus
extern "C" {
#endif
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#ifdef __cpluscplus
}
#endif

#define _MAIN_
#include "snips.h"
#undef _MAIN_
#include <stdio.h>
#ifndef PL_na
# define PL_na na
#endif


MODULE = SNIPS		PACKAGE = SNIPS		PREFIX = snips_

PROTOTYPES: ENABLE


# Call the startup function which forks and sets up signal handlers.
# Need to setup the readconfig function so that the signal handler
# knows what to call to re-read the config file on HUP.
# NOTE NOTE: Different arguments as compared to the C routine.
int
_startup(readconf)
	void  *readconf;
  CODE:
  {
	prognm = "perltestmonitor";	/* FIX FIX */
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
	  XSRETURN_UNDEF;
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

# Create a new event. Use newSV() so that perl can handle freeing memory.
EVENT *
new_event()
  PREINIT:
	static EVENT v;
	SV *sv;
  CODE:
  {
	bzero(&v, sizeof (EVENT));
	sv = newSVpv((char *)&v, sizeof(EVENT));	
	RETVAL = (EVENT *)SvPV(sv, PL_na);
  }
  OUTPUT:
	RETVAL

##
int
init_event(pv)
	EVENT *pv;

  CODE:
  {
	RETVAL = init_event(pv);
  }
  OUTPUT:
	RETVAL


##
int
update_event(pv, status, value, maxsev)
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


##
int
eventlog(pv)
	EVENT *pv;
  CODE:
  {
	RETVAL = eventlog(pv);
  }
  OUTPUT:
	RETVAL

#
#      calc_status()
# Useful to extract the status and maximum severity in monitors which
# have 3 thresholds. Given the three thresholds and the value, it returns
# the 'status, thres, maxseverity' as a list.
#
# 	($status, $thres, $maxsev) = calc_status(...)
#
void
calc_status(val, warnt, errt, critt)
	unsigned long  val;
	unsigned long  warnt;
	unsigned long  errt;
	unsigned long  critt;
  PREINIT:
	int status, maxseverity, incr;
	unsigned long  thres;

  PPCODE:
  {
	if (warnt <= critt)
		incr = 1;
	status = calc_status(val, warnt, errt, critt, incr,
				&thres, &maxseverity);
	EXTEND(sp, 3);
	PUSHs(sv_2mortal(newSViv(status)));	/* integer */
	PUSHs(sv_2mortal(newSVnv(thres)));	/* use double for long */
	PUSHs(sv_2mortal(newSViv(maxseverity)));
  }


# Read one event from the open filehandle. Will need to unpack this
# structure
EVENT *
read_event(fh)
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
	  XSRETURN_UNDEF;
  }
  OUTPUT:
	RETVAL

# Write event to the open filehandle
int
write_event(fh, pv)
	FILE *fh;
	EVENT *pv;
  CODE:
  {
	RETVAL = write_event(fileno(fh), pv);
  }
  OUTPUT:
	RETVAL

# Rewind's output file by one event, and then writes given event.
int
rewrite_event(fh, pv)
	FILE *fh;
	EVENT *pv;
  CODE:
  {
	RETVAL = rewrite_event(fileno(fh), pv);
  }
  OUTPUT:
	RETVAL

# Make a duplicate of the first event. Should call 'new_event()' before
# calling this routine.
void
copy_event(old, new)
	EVENT *old;
	EVENT *new;
  CODE:
  {
	bcopy((void *)old, (void *)new, sizeof(EVENT));
  }

## Return the list of fields in the EVENT structure in the same order
#  as the event2strarray() function returns.
# This returns a reference to an array, so you have to dereference it
# using @$xxx
AV *
_get_eventfields()
  PREINIT:
	int i;
	char **keyarray;
	static AV *av;
  CODE:
  {
	if (! av)
	{
		keyarray = (char **)event2strarray(NULL);
		av = newAV();
		for (i=0; keyarray[i] && *(keyarray[i]); ++i)
		  av_push(av, newSVpv(keyarray[i], 0));
	}
	RETVAL = av;
  }
  OUTPUT:
	RETVAL

# Convert a packed event into a hash. Return's a hash reference, so the
# calling routine has to dereference it.
#	$hashref = _unpack_event($event);
#	%event = %$hashref;	# dereference
#
HV *
_unpack_event(pv)
	EVENT *pv;
  PREINIT:
	int i = 0;
	static char **keyarray;
	char **strarray;
	static SV *sv_value;
	static HV *hashevent;
  CODE:
  {
	if (!hashevent)
		hashevent = newHV();
	else
		hv_clear(hashevent);	/* free's old data */

	if (!keyarray)
		keyarray = (char **)event2strarray(NULL); /* once only */
	strarray = (char **)event2strarray(pv);
	for (i=0; keyarray[i] && *(keyarray[i]) ; ++i)
	{
		if (strarray[i] == NULL)
			strarray[i] = "";
		sv_value = newSVpv(strarray[i], 0);	/* malloc memory */
		hv_store(hashevent, keyarray[i], strlen(keyarray[i]),
			  sv_value, /*hash value*/ 0);

	/*	fprintf(stderr, "(debug) unpack_event() %s, %s\n",
				 keyarray[i], strarray[i]); /* */
	}
	RETVAL = hashevent;
  }
  OUTPUT:
	RETVAL

## Cannot seem to invoke this if argument is set to HV *. So typecasting
#  it.
# Typical Usage:
#	$event1 = snips::new_event();
#	%event1 = snips::unpack_event($event1);
#	$event2 = snips::_pack_event(\%event1);
EVENT *
_pack_event(eventhash)
	SV *eventhash;
  PREINIT:
	int i = 0;
	static char **keyarray;
	char *strarray[64];	/* assuming max 64 fields in EVENT */
	SV **sv;
	HV *hv;
  CODE:
  {
	hv = (HV *)NULL;
	if (SvROK(eventhash))
	  hv = (HV *)SvRV(eventhash); 	/* extract reference */
	if (hv == NULL)
		XSRETURN_UNDEF ;

	if (!keyarray)
		keyarray = (char **)event2strarray(NULL);
	bzero(strarray, sizeof(strarray));
	for (i = 0; keyarray[i] && *(keyarray[i]); ++i)
	{
		sv = hv_fetch(hv, keyarray[i], strlen(keyarray[i]), FALSE);
		if (sv)
		{
			strarray[i] = (char *)SvPV(*sv, PL_na);
		/*	printf("_pack_event() - fetched %s for key %s\n",
				strarray[i], keyarray[i]); /* */
		}
	}

	RETVAL = (EVENT *)strarray2event(strarray);
  }
  OUTPUT:
	RETVAL


# open_datafile()  /* how to send flags and mode to C program? */
# close_datafile()

/* $Header$ */
/*
 * SNIPS perl interface.
 *
 * AUTHOR:
 *	Vikas Aggarwal (vikas@navya_.com)  June 2000
 *
 */
#ifdef __cplusplus
extern "C" {
#endif
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#ifdef __cplusplus
}
#endif

#define _MAIN_
#include "snips.h"
#undef _MAIN_

#ifndef PL_na
# define PL_na na
#endif

MODULE = SNIPS		PACKAGE = SNIPS		PREFIX = snips_

PROTOTYPES: ENABLE


# Call the snips_startup function which sets up signal handlers.
# NOTE NOTE: Different arguments as compared to the C routine.
int
_startup(myname,...)
	char *myname;
  PREINIT:
	int i;
	char *s;
  CODE:
  {
	prognm = (char *)Strdup(myname);	/* global var in snips.h */
	if (items > 1) {  /* 0 is myname, 1 is extnm */
	  s = (char *)SvPV(ST(1), PL_na);
	  if (extnm)  free(extnm);
	  extnm = (char *)Strdup(s);
	}
	printf ("BTW, prognm= %s, extnm= %s, do_reload= %d\n",
			prognm, extnm, do_reload);
	RETVAL = snips_startup();
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

void
open_eventlog()
  CODE:
  {
	openeventlog();
  }

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

##
int
check_configfile_age()
  CODE:
  {
	RETVAL = check_configfile_age();
  }
  OUTPUT:
	RETVAL

		
# Copies all 'common' events data from old file to new file. Called
# by the reload function.
int
copy_events_datafile(ofile, nfile)
	char *ofile;
	char *nfile;
  PREINIT:
	int ofd, nfd;
  CODE:
  {
	ofd = open_datafile(ofile, O_RDONLY);
	nfd = open_datafile(nfile, O_RDWR|O_CREAT|O_TRUNC);
	if (ofd < 0 || nfd < 0) {
		croak("FATAL copy_events_datafile()- cannot open() %s\n",
			sys_errlist[errno]);
	}
	RETVAL = copy_events_datafile(ofd, nfd);
	close_datafile(ofd);  close_datafile(nfd);
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

## FIX FIX FIX. how to access these global variables from snips.h
int
snips_get_reload_flag()

  CODE:
  {
	RETVAL = do_reload;
  }
  OUTPUT:
	RETVAL

## FIX FIX FIX. how to access these global variables from snips.h
void
snips_set_reload_flag(i)
	int i;
  CODE:
  {
	do_reload = i;
  }

## Override the default config and datafile names
void
snips_set_configfile(f)
	char *f;
  CODE:
  {
	(void *)set_configfile(f);
  }

void
snips_set_datafile(f)
	char *f;
  CODE:
  {
	(void *)set_datafile(f);
  }

## Read and write dataversion.
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
# Allow direct updating of various event fields. Else perl will have
# to unpack and then repack.
void
alter_event(pv, sender, sitename, siteaddr, varname, varunits)
	EVENT *pv;
	char  *sender;
	char  *sitename;
	char  *siteaddr;
	char  *varname;
	char  *varunits;
  CODE:
  {
	if (sender) strncpy(pv->sender, sender, sizeof(pv->sender));

	if (sitename) strncpy(pv->site.name, sitename, sizeof(pv->site.name));
	if (siteaddr) strncpy(pv->site.addr, siteaddr, sizeof(pv->site.addr));

	if (varname) strncpy(pv->var.name, varname, sizeof(pv->var.name));
	if (varunits) strncpy(pv->var.units, varunits, sizeof(pv->var.units));
  }


## given a status, value and maxseverity, updates the structure
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
	int status, maxseverity;
	unsigned long  thres;

  PPCODE:
  {
	status = calc_status(val, warnt, errt, critt, -1,
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

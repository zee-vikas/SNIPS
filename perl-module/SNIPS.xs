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

/* Older perl.h files have a debug which conflicts with snips.h */
#include "patchlevel.h"
#ifdef PATCHLEVEL
# if (PATCHLEVEL < 5)
#  define OLDPERL
# endif
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
	if ( items > 1 ) {  /* 0 is myname, 1 is extnm */
	  if (extnm) free(extnm);
	  if ( SvOK(ST(1))) {
		s = (char *)SvPV(ST(1), PL_na);
		extnm = (char *)Strdup(s);
	  }
	  else
		extnm = NULL;
	}
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
  CODE:
  {
	RETVAL = copy_events_datafile(ofile, nfile);
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
read_dataversion(fd)
	int fd;
  CODE:
  {
	RETVAL = read_dataversion(fd);
  }
  OUTPUT:
	RETVAL

# Write dataversion
int
write_dataversion(fd)
	int fd;
  CODE:
  {
	RETVAL = write_dataversion(fd);
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
	init_event(&v);
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
	SV  *sender;
	SV  *sitename;
	SV  *siteaddr;
	SV  *varname;
	SV  *varunits;
  CODE:
  {
	if (SvOK(sender))
	  strncpy(pv->sender, SvPV(sender, PL_na), sizeof(pv->sender));

	if (SvOK(sitename))
	  strncpy(pv->site.name, SvPV(sitename, PL_na), sizeof(pv->site.name));
	if (SvOK(siteaddr))
	  strncpy(pv->site.addr, SvPV(siteaddr, PL_na), sizeof(pv->site.addr));

	if (SvOK(varname))
	  strncpy(pv->var.name, SvPV(varname, PL_na), sizeof(pv->var.name));
	if (SvOK(varunits))
	  strncpy(pv->var.units, SvPV(varunits, PL_na), sizeof(pv->var.units));
  }


## given a status, value and maxseverity, updates the structure
int
update_event(pv, status, value, thres, maxsev)
	EVENT *pv;
	int   status;
	unsigned long value;
	SV* thres;
	int   maxsev;
  CODE:
  {
	if (!SvOK(thres))
	  RETVAL = update_event(pv, status, value, pv->var.threshold, maxsev);
	else
	  RETVAL = update_event(pv, status, value, (unsigned long)SvNV(thres), maxsev);
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
	PUSHs(sv_2mortal(newSVnv(thres)));
	PUSHs(sv_2mortal(newSViv(maxseverity)));
  }


## Given a severity string, return an integer.
int
str2severity(str)
	char *str;
  PREINIT:
	int sev;
  CODE:
	if (str)
	  switch (*str)
	  {
		case 'C': case 'c':
			RETVAL = E_CRITICAL;
			break;
		case 'E': case 'e':
			RETVAL = E_ERROR;
			break;
		case 'W': case 'w':
			RETVAL = E_WARNING;
			break;
		case 'I': case 'i':
			RETVAL = E_INFO;
			break;
		default:
			RETVAL = E_CRITICAL;
			break;
	  }
	else
	  RETVAL = E_CRITICAL;
  OUTPUT:
	RETVAL


# Read one event from the open filehandle. Will need to unpack this
# structure
EVENT *
read_event(fd)
	int fd;
  PREINIT:
	static EVENT v;
  CODE:
  {
	if (read_event(fd, &v) > 0)
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
write_event(fd, pv)
	int fd;
	EVENT *pv;
  CODE:
  {
	RETVAL = write_event(fd, pv);
  }
  OUTPUT:
	RETVAL

# Rewind's output file by one event, and then writes given event.
int
rewrite_event(fd, pv)
	int  fd;
	EVENT *pv;
  CODE:
  {
	RETVAL = rewrite_event(fd, pv);
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


## open_datafile but accepts fopen() style flags (r, w, r+, w+)
int
fopen_datafile(dfile, cflags)
	char *dfile;
	char *cflags;
  CODE:
  {
	int flags = 0;
	if (!strcmp(cflags, "r"))
		flags = O_RDONLY;
	else if (!strcmp(cflags, "w"))
		flags = (O_WRONLY | O_TRUNC | O_CREAT);
	else if (!strcmp(cflags, "r+"))
		flags = O_RDWR;
	else if (!strcmp(cflags, "w+"))
		flags = (O_RDWR | O_TRUNC | O_CREAT);
	else if (!strcmp(cflags, "a"))
		flags = (O_WRONLY | O_CREAT | O_APPEND);
	else if (!strcmp(cflags, "a+"))
		flags = (O_RDWR | O_CREAT | O_APPEND);
	else
		flags = O_RDONLY;

	RETVAL = open_datafile(dfile, flags);
  }
  OUTPUT:
	RETVAL

	
void
close_datafile(fd)
	int fd;
  CODE:
  {
	close_datafile(fd);
  }

# ######   ######   ######   ######   ######   ######

MODULE = SNIPS		PACKAGE = SNIPS::globals 	PREFIX = snips_

## This SETS the global C variables (called by the TIE functions)
void *
_STORE(var, value)
	char *var;
	SV *value;
  PREINIT:
	char *s;
  CODE:
  {
	fprintf(stderr, "debug: trying to SET global '%s'\n", var); /* */

	if      (!strcmp(var, "debug"))      debug = SvIV(value);
	else if (!strcmp(var, "do_reload"))  do_reload = SvIV(value);
	else if (!strcmp(var, "doreload"))  do_reload = SvIV(value);
	else if (!strcmp(var, "autoreload")) autoreload = SvIV(value);
	else if (!strcmp(var, "dorrd"))      dorrd = SvIV(value);
	else if (!strcmp(var, "configfile")) {
		s = SvPV(value, PL_na);
		set_configfile(s);
	}
	else if (!strcmp(var, "datafile"))   {
		s = SvPV(value, PL_na);
		set_datafile(s);
	}
	else {
	  if (debug)
	    fprintf(stderr, "Error: trying to set unknown global '%s'\n", var);
	  XSRETURN_UNDEF ;
	}

	/* RETVAL = value;		/* return what we were sent? */
  }


# Fetch the C value of the variable name passed to this function
SV *
_FETCH(var)
	char *var;
  PREINIT:
	char *s;
  CODE:
  {
	fprintf(stderr, "debug: trying to GET global '%s'\n", var); /* */

	if 	(!strcmp(var, "debug"))	   RETVAL = newSViv(debug);
	else if (!strcmp(var, "do_reload"))  RETVAL = newSViv(do_reload);
	else if (!strcmp(var, "doreload"))  RETVAL = newSViv(do_reload);
	else if (!strcmp(var, "autoreload")) RETVAL = newSViv(autoreload);
	else if (!strcmp(var, "dorrd"))	   RETVAL = newSViv(dorrd);
	else if (!strcmp(var, "configfile")) {
		s = get_configfile();
		RETVAL = newSVpv(s, 0);
	}
	else if (!strcmp(var, "datafile")) {
		s = get_datafile();
		RETVAL = newSVpv(s, 0);
	}
	else {
	  if (debug)
	    fprintf(stderr,
			"Error: trying to fetch unknown global '%s'\n", var);

	  XSRETURN_UNDEF ;
	}
  }
  OUTPUT:
	RETVAL



# ######   ######   ######   ######   ######   ######

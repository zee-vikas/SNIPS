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
#ifndef PL_na
# define PL_na na
#endif


MODULE = SNIPS		PACKAGE = SNIPS		PREFIX = snips_

PROTOTYPES: ENABLE


## Call generic snips_main()  C routine
int
_main(...)
  PREINIT:
	int  i;
	char **argv;
  CODE:
  {
	argv = (char **)malloc((items) * sizeof(char *));
	for (i = 0; i < items; ++i)
	{
		char *handle = argv[i] = (char *)SvPV(ST(i), PL_na);
		argv[i] = (char *)Strdup(handle);
	}
	optind = 0 ; opterr = 0;
	RETVAL = snips_main(items, argv);
	/*** should never return ***/

	for (i = 0; i < items; ++i)		/* just in case... */
		free(argv[i]);
	free(argv);
  }
  OUTPUT:
	RETVAL


# Call the snips_startup function which sets up signal handlers.
# NOTE NOTE: Different arguments as compared to the C routine.
int
_startup()
  CODE:
  {
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

# Following two routines are tricky. The perl routine will call
# set_readconfig_function(), which sets the parameter in the C
# library to be call_readconfig().  call_readconfig() then tries
# to invoke the PERL read_conf subroutine.
#
#   DOES NOT WORK      DOES NOT WORK
int
call_readconfig(datafd, cfile)
	int datafd;
	char *cfile;
  PREINIT:
	FILE *fh;
  CODE:
  {
	int count;	/* number of values returned by perl function */

	fh = fdopen(datafd, "r+");	/* for perl */

	/* dSP ;	/* automatically declared in XSUB */

	ENTER ;
	SAVETMPS ;

	PUSHMARK(SP) ;
	/* XPUSHs(sv_2mortal(newSViv(datafd)));	/* file desc */
	XPUSHs(sv_2mortal(newSVpv((char *)fh, sizeof(*fh))));
	XPUSHs(sv_2mortal(newSVpv(cfile, 0)));
	PUTBACK ;

	count = perl_call_pv("read_conf", G_SCALAR);	/* or G_ARRAY */
	SPAGAIN;	/* refresh stack */

	if (count == 1)
		RETVAL = POPi;	/* pop the return integer value */

	PUTBACK;
	FREETMPS ;
	LEAVE ;
  }


#   DOES NOT WORK      DOES NOT WORK  (see above)
void
set_readconfig_function()

  CODE:
  {
	int call_readconfig();		/* */
	set_readconfig_function(call_readconfig);
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
	newfd = snips_reload(fileno(oldfh), NULL);
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

int
copy_datafile_events(ofh, nfh)
	FILE *ofh;
	FILE *nfh;
  CODE:
  {
	RETVAL = copy_datafile_events(fileno(ofh), fileno(nfh));
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

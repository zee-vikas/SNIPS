#ifndef lint
static char *RCSid = "$Header$" ;
#endif

#include "os.h"

#ifdef SNIPS

/*
 * SNIPS specific routines for the 'etherload' program.
 *
 *	Vikas Aggarwal, vikas@navya_.com
 */

/* Copyright Netplex Technologies Inc. */

/*
 * $Log$
 * Revision 1.2  2008/04/25 23:31:50  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.1  2001/08/01 01:08:06  vikas
 * Was not setting the name of the snips config file correctly.
 *
 * Revision 1.0  2001/07/08 21:47:32  vikas
 * For SNIPS v1.0
 *
 *
 */

#include <stdlib.h>
#include <string.h>

#define _MAIN_
#include "snips.h"
#include "snips_funcs.h"
#include "snips_main.h"
#include "event_utils.h"
#undef _MAIN_
#include "etherload.h"
#include "externs.h"		/* defines  extern bw, pps */
#include "util.h"

static char	*sender;

/* function prototypes */
void set_functions();
int readconfig();

/*
 * Note that E_CRITICAL = 1 & E_INFO = 4. We only need 3 spaces in the
 * thres[x][], but easier to reference using these ENUM types.
 */
enum { BW, PPS, EOVARS } ;		/* list of variables checked */
static char *varnames[] = {		/* names of the variables */
  "bw", "pps", "" };

struct _snips_if			/* store thresholds for each var */
{
  char	*name ;				/* device name in the config file */
  int	thres[E_INFO][EOVARS];		/* the three thresholds */
} cfsnips_if[MAXINTERFACES], *psnips_if[MAXINTERFACES] ;


/*
 * Called upon startup to do device specific stuff
 */
void snips_begin(cf_file)
  char *cf_file;
{
  if ((sender = (char *)strrchr (prognm , '/')) == NULL)
    sender = prognm ;                         /* no path in program name */
  else
    sender++ ;                                /* skip leading '/' */

  set_functions();
  if (cf_file && *cf_file)
    set_configfile(cf_file);

  snips_startup();

  if (readconfig() == -1)
    finish(1);

}	/* end snips_begin() */


/*+ 
 * Read in the configuration file.
 *
 * SLEEPSECS ppp (or POLLINTERVAL secs)
 * SCANSECS sss
 * DEVICE device
 * BW	thres1 thres2 thres3
 * PPS	thres1 thres2 thres3
 *
 */

int readconfig()
{
  register int i;
  int ndevs = -1;		/* reset every time function is called */
  int bwdone =1, ppsdone =1;
  char *configfile;
  char line[MAXLINE];
  FILE *cfile = NULL;
  extern char *skip_spaces(), *Strdup();
  extern char **devlist;		/* in etherload.h */
    

  configfile = (char *)get_configfile();

  if (debug)
    fprintf (stderr, "Opening config file '%s'\n", configfile);

  if ((cfile = fopen(configfile, "r")) == NULL)
  {
    fprintf(stderr, "%s error (init_devices) ", prognm) ;
    perror (configfile);
    return (-1);
  }

  /* free list in case rereading config file */
  for (i=0; cfsnips_if[i].name != NULL; ++i)
    if (cfsnips_if[i].name)
      free(cfsnips_if[i].name);

  while (fgets(line, sizeof (line), cfile) != NULL)
  {
    char *s;
    static int linenum = 0;

    s = (char *)strchr(line, '\n');
    *s = '\0';			/* rid of newline */
    s = line;
    ++linenum;
    if (debug > 3)
      fprintf (stderr, "Config %.2d: %s\n", linenum, line);

    s = skip_spaces(s);

    if (*s == '#' || *s == '\0')
      continue;

    if (strncasecmp(s, "pollinterval", strlen("pollinterval")) == 0  ||
	strncasecmp(s, "sleep", strlen("sleep")) == 0)
    {
      s = (char *)strtok(s, " \t");
      sleepsecs = atoi (skip_spaces((char *)strtok(NULL, " \t\n")));
      if (debug > 1)
	fprintf(stderr, "readconfig(): sleepsecs/pollinterval= %d\n",
		sleepsecs);
      continue;
    }

    if (strncasecmp(s, "scan", strlen("scan")) == 0)
    {
      s = (char *)strtok(s, " \t");
      scansecs = atoi (skip_spaces((char *)strtok(NULL, " \t\n")));
      if (debug > 1)
	fprintf(stderr, "readconfig(): scansecs= %d\n", scansecs);
      continue;
    }

    if (strncasecmp(s, "rrdtool", strlen("rrdtool")) == 0)
    {
      s = (char *)strtok(s, " \t");
      s = skip_spaces((char *)strtok(NULL, " \t\n"));
      if (strncasecmp(s, "on", strlen("on")) == 0)
      {
#ifdef RRDTOOL
	++dorrd;
	if (debug > 1)
	  fprintf(stderr, "readconfig(): RRD enabled\n");
#else
	fprintf(stderr, "%s: RRDtool not supported\n", prognm);
#endif
      }
      continue;
    }

    if (strncasecmp(s, "device", strlen("device")) == 0)
    {
      if (bwdone != 1 && ppsdone != 1)
	fprintf (stderr,
		 "%s [%d]: error- incomplete DEVICE config, ignoring\n",
		 prognm, linenum);
      else
	++ndevs ;			/* increment only if prev was okay */
      s = (char *)strtok(s, " \t");
      s = skip_spaces((char *)strtok(NULL, " \t\n"));
      cfsnips_if[ndevs].name = Strdup(s);

      bwdone =0; ppsdone =0;
      if (debug > 1)
	fprintf(stderr, "readconfig(): device = %s\n", s);
      continue;
    }

    if (strncasecmp(s, "pps", strlen("pps")) == 0)
    {
      if (ppsdone)
      {
	fprintf(stderr,"%s [%d]: error- got PPS line without DEVICE\n",
		prognm, linenum);
	continue ;
      }
      if (debug > 1)
	fprintf(stderr, "readconfig(): PPS thres= ");

      s = (char *)strtok(s, " \t");
      for (i=E_WARNING ; i >= E_CRITICAL; --i)
      {
	if ((s = (char *)strtok(NULL, " \t\n")) != NULL)
	  cfsnips_if[ndevs].thres[i][PPS] = atoi(skip_spaces(s));
	else
	  cfsnips_if[ndevs].thres[i][PPS] = 0;

	if (debug > 1)
	  fprintf (stderr, 
		   "%d(%s) ", cfsnips_if[ndevs].thres[i][PPS],
		   severity_txt[i]);
      }
      ppsdone =1;
      if (debug > 1)
	fprintf (stderr, "\n");
      continue;
    }	

    if (strncasecmp(s, "bw", strlen("bw")) == 0)
    {
      if (bwdone)
      {
	fprintf(stderr, "%s [%d]: error- got BW line without DEVICE\n",
		prognm, linenum);
	continue;
      }
      if (debug > 1)
	fprintf(stderr, "readconfig(): BW thres= ");

      s = (char *)strtok(s, " \t");
      for (i=E_WARNING ; i >= E_CRITICAL; --i)
      {
	if ((s = (char *)strtok(NULL, " \t\n")) != NULL)
	  cfsnips_if[ndevs].thres[i][BW] = atoi(skip_spaces(s));
	else
	  cfsnips_if[ndevs].thres[i][BW] = 0;

	if (debug > 1)
	  fprintf (stderr, "%d(%s) ", cfsnips_if[ndevs].thres[i][BW],
		   severity_txt[i]);
      }
      bwdone =1;
      if (debug > 1)
	fprintf (stderr, "\n");
      continue;
    }	

    fprintf (stderr, "%s [%d]: Ignoring config '%s'\n", prognm, linenum,
	     line);

  }	/* end: while()  */

  fclose(cfile);
  if (++ndevs < MAXINTERFACES)
    cfsnips_if[ndevs].name = NULL;	/* last one in list */

  /*
   * Create list of devices read in from the config file. Used by the
   * main module to see which interfaces it should look for.
   */
  devlist = (char **)malloc((ndevs + 1) * sizeof (char *));
  for (i = 0 ; i <= ndevs ; ++i)
    *(devlist + i) = cfsnips_if[i].name ;


  return (0);
}		/* readconfig() */

/*
 * Called just before beginning the tests.... Write out the initial
 * EVENT structures to the output datafile. The only reason this is
 * not included in readconfig()  is because this weeds out all
 * unavailable interfaces from the output data file.
 */

int snips_prep()
{
  register int i, j;
  int nvar, nif, fdout;
  extern int	ninterfaces;		/* number of valid interfaces found */
  char *datafile;
  EVENT v;                            	/* Defined in SNIPS.H */

  datafile = get_datafile();
  if ( (fdout = open_datafile(datafile, O_RDWR|O_CREAT|O_TRUNC)) < 0)
  {
    fprintf(stderr, "(%s) ERROR in open datafile ", prognm);
    perror (datafile);
    finish(-1);
  }

  /*
   * Get rid of the unavailable interfaces from the 'snips_if' struct
   * so that the 'if_stats' interfaces match the 'snips_if' struct.
   * DONT 'free' the char strings- they are pointed to by a number
   * of other vars like devlist, etc.
   */

  for (i=0; i < ninterfaces; ++i)
    for (j=0; cfsnips_if[j].name != NULL; ++j)
      if (strcmp(cfsnips_if[j].name, if_stats[i].name) == 0)
      {
	psnips_if[i] = &(cfsnips_if[j]);

	if (debug)
	{
	  int s, t;

	  fprintf (stderr, "(debug)%s thresholds:\n",
		   psnips_if[i]->name);
	  for (s = 0; s < EOVARS; ++s)
	  {
	    fprintf (stderr, "%s: ", varnames[s]);
	    for (t = E_WARNING ; t >= E_CRITICAL; --t)
	      fprintf (stderr, " %d", psnips_if[i]->thres[t][s]);
	    fprintf (stderr, "\n");
	  }
	}
	      
	break;
      }

  init_event(&v);
  strncpy (v.sender, sender, sizeof(v.sender) - 1);
  gethostname(v.device.addr, sizeof(v.device.addr) - 1); /* null from bzero */
  strncpy(v.device.name, v.device.addr, sizeof(v.device.name) - 1);

  for (nif = 0; nif < ninterfaces; ++nif)
    for (nvar = 0; nvar < EOVARS; ++nvar)
    {
      /* put 'interface - type'  in v.device.subdev  field */
      sprintf(v.device.subdev, "%s-%s",if_stats[nif].name, if_stats[nif].type);
      switch (nvar)
      {
      case BW:
	strncpy (v.var.units, "%age", sizeof (v.var.units) - 1);
	strncpy(v.var.name, "Bandwidth", sizeof(v.var.name) - 1);
	v.var.threshold = 0 ;	/* any value for now */
	break;

      case PPS:
	strncpy (v.var.units, "pps", sizeof (v.var.units) - 1);
	strncpy(v.var.name, "PktsPerSec", sizeof(v.var.name) - 1);
	v.var.threshold = 0 ;	/* any value for now */
	break;

      default:
	fprintf(stderr, "%s: fatal, went past EOVARS in snips_prep\n",
		prognm);
	finish(-1);
      }

      if (write_event(fdout, &v) != sizeof(v))
	finish(-1) ;
    }

  close(fdout);
  return(1);                          /* All OK  */

}	/* end:  snips_prep */


/*
 * Write data to the snips data file.
 */
void
snips_write_etherload_stats()
{
  int nif, nvar;
  int fdout;
  char *datafile;

  (void) gettimeofday(&endtime, (struct timezone *) 0);
  totaltime = (u_long)endtime.tv_sec - (u_long)starttime.tv_sec ;

  if (totaltime <= 0)
    return;

  datafile = get_datafile();
  if ( (fdout = open_datafile(datafile, O_RDWR)) < 0 )
  {
    fprintf(stderr, "(%s) ERROR in open datafile ", prognm);
    perror (datafile);
    finish(-1);
  }

  for (nif = 0; nif < ninterfaces; nif++)
  {
    dostats (&if_stats[nif]);
    if (debug)
      fprintf(stderr, "BW= %d PPS=%d\n", bw, pps);
    for (nvar = 0; nvar < EOVARS; nvar++)
    {
      int status ;
      int maxseverity;
      u_long value, thres;	/* u_long because of VAR in snips struct */
      EVENT v;		/* Do PPS AND BandWidth */

      switch (nvar)
      {
      case BW:
	value = bw; break;
      case PPS:
	value = pps; break;
      default:
	fprintf(stderr, "%s: fatal error, went past EOVARS\n", prognm);
	finish (-1);
      }

      status = calc_status(value,
			   psnips_if[nif]->thres[E_WARNING][nvar],
			   psnips_if[nif]->thres[E_ERROR][nvar],
			   psnips_if[nif]->thres[E_CRITICAL][nvar],
			   /*incr*/1, &thres, &maxseverity);

      read_event(fdout, &v);
      update_event(&v, /* up or down */status, value, thres, maxseverity) ;
      /* update with current threshold */
      if (maxseverity == E_INFO)
	v.var.threshold = psnips_if[nif]->thres[E_WARNING][nvar];
      else
	v.var.threshold = psnips_if[nif]->thres[maxseverity][nvar];
      rewrite_event(fdout, &v);
    }	/* end: for(nvar) */
  }	/* end:  for(nif) */
  close(fdout);
}	/* end:  snips_write_etherload_stats()  */

/*
 * Set the customizable functions for the SNIPS library.
 */
void set_functions()
{
  set_readconfig_function(readconfig);
}

#endif /* SNIPS */

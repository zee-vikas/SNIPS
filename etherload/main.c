#ifndef lint
static char *RCSid = "$Id$";
#endif

/*
 * To watch the load and packets per sec on an ethernet interface. This
 * program can work standalone or in conjunction with the SNIPS monitoring
 * package. It does NOT look inside the packets (i.e. it is not a sniffer,
 * though there are hooks to process each packet - currently doing nothing).
 *
 * Author:
 * 	Vikas Aggarwal, vikas@navya_.com
 */



/* Adapted from nfswatch.c v4.1 Original header follows:
 *
 * nfswatch.c,v 4.3 93/10/04 11:01:12 mogul Exp $";
 *
 * nfswatch - NFS server packet monitoring program.
 *
 * David A. Curry				Jeffrey C. Mogul
 * Purdue University				Digital Equipment Corporation
 * Engineering Computer Network			Western Research Laboratory
 * 1285 Electrical Engineering Building		250 University Avenue
 * West Lafayette, IN 47907-1285		Palo Alto, CA 94301
 * davy@ecn.purdue.edu				mogul@decwrl.dec.com
 *
 * ********* End original header **********
 *
 */

/*
 * $Log$
 * Revision 1.1  2001/08/01 01:08:06  vikas
 * Was not setting the name of the snips config file correctly.
 *
 * Revision 1.0  2001/07/08 21:47:32  vikas
 * For SNIPS v1.0
 *
 *
 */

/*  */

#include "os.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <net/if.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef SNIPS
# include "snips.h"
#endif

#include "etherload.h"

int		debug = 0;
int		truncation = 200;		/* pkt trunc len - magic */
int		scansecs;			/* scan wire for this secs */
int		sleepsecs = -1;			/* sleep before next scan */
int		ninterfaces;			/* number of interfaces	*/
int	       	breakscan;			/* flag to stop scanning */
int		extended;
int		probedevices;			/* search for all devices */
char		*prognm;			/* program name		*/
u_long		totaltime ;
struct timeval	starttime;			/* time we started test	*/
struct timeval	endtime;			/* time test stopped */
char		**devlist;			/* list of devices to watch */

/*
 * These are the stats calculated by 'dostats'
 */
u_long	totalpkts;
u_long	avg_pkt_sz;
u_long	est_total_bytes;	/* Notice kbytes to prevent overflow */
u_long	kbps;			/* Utilization in kbits per sec */
u_long	bw;			/* bandwidth in percentage */
u_long	pps;			/* packets per second */
u_long	dropspct;		/* percent pkt drops */


extern void finish();



#ifdef ultrix
void
fpe_warn()
{
  fprintf(stderr, "nfswatch: mystery bug encountered.\n");
  finish(-1);
}
#endif /* ultrix */

main(argc, argv)
  int argc;
  char **argv;
{
  register int i;
  char *cfile = NULL;
  extern char *os_devices[] ;	/* standard device names for each OS */
  extern char *optarg;
  extern int  optind;

  if ((prognm = (char *)strrchr (argv[0], '/')) == NULL)
    prognm = argv[0] ;                        /* no path in program name */
  else
    prognm++ ;                                /* skip leading '/' */

  while ((i = getopt(argc, argv, "c:deps:i:")) != EOF)
    switch (i)
    {
#ifdef SNIPS
    case 'c':
      cfile = Strdup(optarg);
      break;
#endif
    case 'd':
      debug++;
      break;
    case 'e':
      extended++;
      break;
    case 'i':			/* scan interval */
      scansecs = atoi(optarg);
      break;
    case 'p':			/* probe for devices */
      probedevices++ ;
      break;
    case 's':
      sleepsecs = atoi(optarg);	/* time between each scan */
      break;
    case '?':
    default:
      fprintf(stderr, "%s: Unknown flag: %c\n", prognm, optarg);
      fprintf(stderr, "USAGE: %s  [-d (debug)] [-e (extended)] ", prognm);
      fprintf(stderr, "[-s <sleep secs>] [-i <scan interval>] ");
#ifdef SNIPS
      fprintf(stderr, "[config-file]\n");
#else	  
      fprintf(stderr, "[interface ...]\n");
#endif
      exit (1);
    }


#ifdef SNIPS
  fprintf (stderr, "%s: This is a SNIPS version\n", prognm);
  if (cfile == NULL || *cfile == '\0')
    if (argc > optind)
      cfile = argv[optind];		/* config file as the args */
  snips_begin(cfile);
#else
  if (argc > optind)
    devlist = &argv[optind];		/* specified interfaces */

#endif	/* SNIPS */


  /***************************************************************************/
  if (scansecs == 0)
    scansecs = SCANTIME ;

  if (sleepsecs == -1)	/* un-initialized */
    sleepsecs = SLEEPTIME ;

    /*
     * Trap signals so we can clean up.
     */
  (void) signal(SIGINT, finish);
  (void) signal(SIGQUIT, finish);
  (void) signal(SIGTERM, finish);
    
#ifdef sgi
  /*
     * Kludge to prevent coredumps when the optimizer's on?
     */
  (void) signal(SIGFPE, SIG_IGN);
#endif
    
#ifdef ultrix
  (void) signal(SIGFPE, fpe_warn);
#endif


  /* 'Minor' initalizations */
  for (i = 0; i < MAXINTERFACES; ++i )
    bzero ( (char *)&if_stats[i], sizeof (struct _if_stats) );

  /*
   * Initialize the devices
   */

  if (devlist == NULL)
  {
    fprintf(stderr, "%s: searching for all devices\n", prognm);
    if (os_devices && *os_devices && !probedevices)
      devlist = os_devices;	/* os_dev is set in the os specific modules */
    else
      devlist = (char **)get_devlist();
  }

  if (debug)
  {
    fprintf(stderr, "debug: Checking for devices: ");
    i =0;
    while (devlist[i])
      fprintf(stderr, "%s ", devlist[i++]);
    fprintf(stderr, "\n");
  }

  for (i=0; devlist[i] != NULL; i++)
  {
    if_stats[ninterfaces].fd = setup_device(devlist[i]);
	
    if (if_stats[ninterfaces].fd >= 0)
    {
      if_stats[ninterfaces].name = devlist[i];
      if_stats[ninterfaces].typei =
	get_devtype(devlist[i], if_stats[ninterfaces].fd);
      if_stats[ninterfaces].type = 
	(char *)ndt_namebw(if_stats[ninterfaces].typei,
			   &(if_stats[ninterfaces].bw)) ;

      if (if_stats[ninterfaces].bw == 0)	/* loopback or null device */
      {
	close (if_stats[ninterfaces].fd);
	if_stats[ninterfaces].fd = -1;
	if_stats[ninterfaces].name = NULL;
      }
      else
	ninterfaces++;

    }	/* if (fd > 0) */
    else
      if (debug > 1)		/* print out perror for diagnostics */
      {
	fprintf (stderr, "(debug) open() failed for device %s: ", devlist[i]);
	perror("");
      }

  }	/* for() */
    
  if (ninterfaces <= 0)		/* no devices could be opened */
  {
    fprintf(stderr,
	    "%s error: No devices could be opened, exiting...\n", prognm);
    exit (1);
  }


  /*
   * Now lose super-user permission, since we don't need it for anything else.
   */
#ifdef SVR4
  (void) setuid(getuid());
  (void) seteuid(getuid());
#else
  (void) setreuid(getuid(), getuid());
#endif

  /*
     * Valid devices are those with open fd and a name.
     */
  if (debug)
  {
    printf ("Devices being monitored are:\n");
    for (i =0; i < ninterfaces; ++i)
      if (if_stats[i].name && *(if_stats[i].name) != '\0')
	printf(" %d. %s %s %u\n", i+1, 
	       if_stats[i].name, if_stats[i].type, if_stats[i].bw) ;
  }
    
  printf("Scan-interval= %d, sleeptime= %d\n\n", scansecs, sleepsecs);
  (void) fflush(stdout);
    
  /***************************************************************************/

#ifdef SNIPS
  snips_prep();
#endif
    
  for (;;)		/* forever */
  {
    etherload(scansecs);
#ifdef SNIPS
    snips_write_etherload_stats();
#else
    printstats();
#endif
    if (debug > 2)
      fprintf(stderr, "debug: sleeping for %d secs, zzzz..\n", sleepsecs);
    sleep (sleepsecs);
  }

}	/* end:  main()  */



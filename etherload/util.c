#ifndef lint
static char *RCSid = "$Header$" ;
#endif

#include "os.h"

/* Adapted from nfswatch v4.1 Original comments follow: */
/*
 * util.c - miscellaneous utility routines.
 *
 * util.c,v 4.6 93/11/02 10:59:06 mogul Exp $;
 *
 * David A. Curry				Jeffrey C. Mogul
 * Purdue University				Digital Equipment Corporation
 * Engineering Computer Network			Western Research Laboratory
 * 1285 Electrical Engineering Building		250 University Avenue
 * West Lafayette, IN 47907-1285		Palo Alto, CA 94301
 * davy@ecn.purdue.edu				mogul@decwrl.dec.com
 *
 */

/*
 * $Log$
 * Revision 1.0  2001/07/08 21:47:32  vikas
 * For SNIPS v1.0
 *
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <curses.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "etherload.h"
#include "externs.h"

/*
 * finish - clean up and exit.
 */
void
finish(code)
  int code;
{
  int i;

  /*
   * Close the network devices.
   */
  for (i = 0; i < ninterfaces; i++)
    if (if_stats[i].fd >= 0)
      (void) close(if_stats[i].fd);

#ifdef SNIPS
  snips_done();			/* exits  also */
#else
  if (code < 0)
    (void) exit(-code);
  (void) exit(0);
#endif
}	/* finish() */


/*
 * Print statistics.
 */
void
printstats()
{
  register int i;
  static int printhdr = 1;

  (void) gettimeofday(&endtime, (struct timezone *) 0);
  totaltime = (u_long)endtime.tv_sec - (u_long)starttime.tv_sec ;

  if (totaltime <= 0)
    return;

  for (i = 0; i < ninterfaces && if_stats[i].fd >= 0; i++)
  {
    (void) dostats (&if_stats[i]);
    if (printhdr)
    {
      if (extended)
	printf ("%24.24s  %6.6s %8.8s %8.8s %5.5s %8.8s %5.5s %8.8s %5.5s %4.4s %3.3s\n",
		"Date     ", "Device", "ReadPkts", "ReadBytes",
		"Secs", "AvgPktSz", "Drops-%", "TotalByt", " PPS ", "kbps", "BW%");
      else	/* short header */
	printf("%24.24s  %6.6s %8.8s %6.6s %5.5s %3.3s\n", "Date       ", 
	       "Device", "TotalPkts", "Drop-%", " PPS ", "BW%");
      printhdr = 0;
    }
	
    if (extended)
      (void) printf("%24.24s %6s %8u %8u %5us %8u %5u %8u %5u %4u %3u\n",
		    ctime((time_t *)&(endtime.tv_sec)), if_stats[i].name,
		    if_stats[i].readpkts, if_stats[i].readbytes,
		    totaltime, avg_pkt_sz, dropspct, est_total_bytes,
		    pps, kbps, bw);

    else
      printf ("%24.24s %6s %8u %6u %5u %3u\n",
	      ctime((time_t *)&(endtime.tv_sec)), 
	      if_stats[i].name, totalpkts, dropspct , pps, bw);
  }	/* end for() */
    
  fflush (stdout);

}	/* end printstats() */


/*
 * Calculate the statistics on the various interfaces.
 * Since most of the calcuations are not done at the decimal level, the
 * order of calculations is important. i.e. 
 *
 *		(1 / 100) * 200 = 0
 * but
 *		(1 * 200) / 100 = 2
 *
 */

dostats(p)
  struct _if_stats *p;		/* pointer to _if_stats structure */
{
  if (totaltime == 0)
    return (0);

  totalpkts	= p->readpkts + p->droppkts;
  avg_pkt_sz	= p->readpkts ? (p->readbytes / p->readpkts) : 0 ;
  est_total_bytes = totalpkts * avg_pkt_sz;

  pps  = totalpkts / totaltime;
  kbps = (est_total_bytes * 8) / (totaltime * 1000) ;	/* kbits/sec */

  bw = (kbps * 100) / p->bw;			/* convert to %age */

  if (bw > 100)
  {
    fprintf(stderr, "%s: %24.24s Bad bandwidth %d >100\n", prognm,
	    ctime((time_t *)&(endtime.tv_sec)), bw);
    return(0);
  }

  /* percent pkt drops, check for zero totalpkts */
  dropspct = totalpkts ? (p->droppkts * 100/totalpkts) : 0;

  return (0);
}	/* end: dostats()  */

/*
 * error - print an error message preceded by the program name.
 */
void
error(str)
  register char *str;
{
  char buf[BUFSIZ];

  (void) sprintf(buf, "%s: %s", prognm, str);
  (void) perror(buf);
}



/*
 * ndt_info - return string giving name, bw for a network device type.
 * Fills in the integer bandwidth, and returns a string to the description.
 */
char *
ndt_namebw(ndt, bw)
  int ndt;	/* network device type */
  long *bw;
{
  switch(ndt) 
  {
  case NDT_NULL:
    *bw = NULLBW ;
    return("no link-layer encapsulation");
  case NDT_EN10MB:
    *bw = EN10BW;
    return("Ethernet");
  case NDT_EN3MB:
    *bw = EN3BW;
    return("Experimental Ethernet");
  case NDT_AX25:
    *bw = AX25BW;
    return("Amateur Radio AX.25");
  case NDT_PRONET:
    *bw = PRONETBW;
    return("ProNET");
  case NDT_CHAOS:
    *bw = CHAOSBW;
    return("Chaosnet");
  case NDT_IEEE802:
    *bw = IEEE802BW;
    return("IEEE802");
  case NDT_ARCNET:
    *bw = ARCNETBW;
    return("ARCNET");
  case NDT_SLIP:
    *bw = SLIPBW;
    return("SLIP");
  case NDT_PPP:
    *bw = PPPBW;
    return("PPP");
  case NDT_FDDI:
    *bw = FDDIBW;
    return("FDDI");
  case NDT_EN100MB:
    *bw = EN100BW;
    return("Fast Ether");
  default:
    *bw = 0;
    return("[unknown LAN type]");
  }
}


#ifndef lint
 static char rcsid[] = "$Header$";
#endif

#include "os.h"

/*
 * To watch the `load' and `packets-per-sec' on an ethernet interface.
 *
 * Author:
 * 	Vikas Aggarwal, vikas@navya_.com
 */



/* Adapted from nfswatch.c v4.1 Original comments follow */

/* nfswatch.c,v 4.3 93/10/04 11:01:12 mogul Exp $"; */
/*
 * nfswatch - NFS server packet monitoring program.
 *
 * David A. Curry				Jeffrey C. Mogul
 * Purdue University				Digital Equipment Corporation
 * davy@ecn.purdue.edu				mogul@decwrl.dec.com
 *
 */

/*
 * $Log$
 * Revision 1.1  2008/04/25 23:31:50  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/08 21:47:32  vikas
 * For SNIPS v1.0
 *
 *
 */

/*  */

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
#ifdef AIX
# include <sys/select.h>
#endif

#include "etherload.h"
#include "externs.h"

/* function prototypes */
int pkt_process(char *pkt, int pktlen, int itype);

/*
 * etherload - main packet reading loop.
 */
int etherload(secs)
  int secs;				/* scan seconds */
{
  register int i, cc;
  void set_breakscan() ;		/* On getting a sigalarm */
  fd_set saved_readfds, readfds;
  int ndrops ;				/* number of packets dropped */
  int pktlen ;				/* snap length of pkt */
  int wirelen ;				/* full data-link level len of pkt */
  char *pkt ;				/* must typecast depending on arch */
  u_long tdrops[MAXINTERFACES];		/* temporary values */

  /*
   * Zero out the counters
   */
  for (i =0; i < ninterfaces ; ++i)
  {
    if_stats[i].readpkts = 0;
    if_stats[i].droppkts = 0;
    if_stats[i].readbytes = 0;
    tdrops[i] = (u_long)-1;		/* init to largest possible value */
  }

  /*
   * Set the fd-set once and for all, and keep in a unchanged var.
   * Better than resetting it each time. Only for valid devices...
   */
  FD_ZERO(&saved_readfds);
  for (i=0; i < ninterfaces; i++)
    if (if_stats[i].fd >= 0)
      FD_SET(if_stats[i].fd, &saved_readfds);
	
  /*
   * Set up the alarm handler.
   */
  breakscan = 0;		/* reset */
  (void) signal(SIGALRM, set_breakscan);
    
  /*
   * Set up the alarm clock.
   */
  if (alarm (secs) < 0) {
    error ("alarm");
    finish(-1);
  }
    
  /*
   * get the start time.
   */
  (void) gettimeofday(&starttime, (struct timezone *) 0);

  /*
   * Flush the read queue of any packets that accumulated
   * during setup time.
   */
  if (debug > 2)
    fprintf (stderr, "%s: Flushing interfaces...\n", prognm);
  for (i=0; i < ninterfaces; i++)
    flush_device(if_stats[i].fd);

  if (debug > 1)
    fprintf (stderr, "%s: ready to read packets...\n", prognm);
    
  while (!breakscan)	/* forever, until alarm or something */
  {
    readfds = saved_readfds;    /* faster than calling FD_SET each time */
	
    cc = select(NFDBITS, &readfds, (fd_set *) 0, (fd_set *) 0, 0);

    if ((cc < 0) && (errno != EINTR)) {
      error("select");
      finish(-1);
    }
    if (cc == 0) {
      continue;
    }
    
    /*
     * For each interface, see which one has packets to read
     */
    for (i=0; i < ninterfaces; i++)
    {		/* read from the interface */
      int   c;
      /*
       * Nothing to read.
       */
      if (!FD_ISSET(if_stats[i].fd, &readfds))
	continue;

      /*
       * Now get packets from the interface. Call
       * getpkt to extract the packets from the 'chunks'
       */
      while ( (c=getpkt(if_stats[i].fd, if_stats[i].typei,
			&pkt, &pktlen, &wirelen, &ndrops)) != NOPKT)
      {
#ifdef DEBUG
	if (debug > 3)
	  fprintf (stderr, "debug3: pktlen/snaplen %d/%d\n",
		   wirelen, pktlen);
#endif
	++(if_stats[i].readpkts);      	/* increment read packets */
	if_stats[i].readbytes += (u_long) wirelen ;
	if (tdrops[i] == (u_long)-1)
	  tdrops[i] = (u_long) ndrops;	/* ignore first time value */

	if_stats[i].droppkts  += (u_long) (ndrops - tdrops[i]) ;
	tdrops[i] = (u_long) ndrops ;	/* we want only delta drops */
#ifdef DEBUG
	if (debug > 3)
	  fprintf(stderr, "debug3: int_drops= %u, if_drops= %u\n",
		  if_stats[i].droppkts, ndrops);
#endif
	/*
	 * Process the packet.
	 */

	(void) pkt_process(pkt, pktlen, if_stats[i].typei);

	if (c == ENDOFCHUNK)
	  break;		/* out of the while() loop */

      }	/* end:  while(getpkt)  */

      if (debug > 1)
	fprintf (stderr, "debug: Done chunk from interface %d\n", i);

    }	/* end:  for( ;i<ninterfaces ;)  */

  }	/* end:  forever() */

  return (1);

}	/* end: etherload() */

/*
 * Set flag so that etherload can creak out of the scanning loop on
 * getting a SIGALRM.
 */
void
set_breakscan()
{
  breakscan = 1;
}

/*+ pkt_process()
 * Process the packet- extract whatever info is needed.
 *
 * The NIT device rips off the FDDI packet header and the 802.2 LLC
 * header, and replaces them with an ethernet packet header. So
 * process fddi and ethernet packets as ether packets.
 */
int pkt_process(pkt, pktlen, itype)
  char *pkt ;
int pktlen;
int itype;		/* interface type: NDT_FDDI, NDT_EN10MB, etc. */
{

#if defined(notdef) && defined(USE_NIT)	/* on NIT interfaces */

  /*    if (itype == NDT_FDDI) {
   *	(void) pkt_process_fddi(pkt, pktlen);
   *  }
   *   else {
   *	(void) pkt_process_ether(pkt, pktlen);
   *   }
   */
#else	/* treat FDDI pkts like ethernet */

  /*
   * So... we just do this instead for both ethernet and FDDI.
   */
  /*    pkt_process_ether(pkt, pktlen);	*/

#endif
  return (1);
}



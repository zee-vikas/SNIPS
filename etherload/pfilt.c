#ifndef lint
static char *RCSid = "$Header$" ;
#endif

#include "os.h"

#ifdef USE_PFILT	/* Ultrix & OSF1 */

/* Routines for grabbing packets from the Ultrix pfilt interface for
 * use with 'etherload'.
 *
 *	Vikas Aggarwal, vikas@navya_.com, Feb 1994
 */

/* Adapted from nfswatch 4.1. Original comments follow: 
 *
 * pfilt.c - routines for messing with the packet filter
 *
 * davy/system/nfswatch/RCS/pfilt.c,v 4.0 1993/03/01 19:59:00 davy Exp $";
 *
 * Jeffrey Mogul
 * DECWRL
 *
 * ********* End original header ********
 */

/*
 * $Log$
 * Revision 1.0  2001/07/08 21:47:32  vikas
 * For SNIPS v1.0
 *
 *
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/file.h>
#include <net/if.h>
#include <signal.h>
#include <stdio.h>

#include <net/pfilt.h>

#include "etherload.h"
#include "externs.h"

#define PFILT_CHUNKSIZE	8192		/* chunk size for grabbing pkts	*/

static struct ifreq ifr;			/* holds interface name	*/

char	*os_devices[] = {
  "pf0", "pf1", "pf2", "pf3", "pf4", "pf5",
  "pf6", "pf7", "pf8", "pf9",
  0
};

/*
 * setup_device - set up the packet filter (PFILT)
 */
int
setup_device(device)
  char *device;		/* device name */
{
  int fd;
  struct timeval timeout;
  short enmode;
  short backlog = -1;	/* request the most */
  struct enfilter Filter;

  if (!device  || *device == NULL) {
    error("no device name");
    finish (-1);
  }
	  
  /*
   * Open the packetfilter.  If it fails, the device is probably
   * busy. Dont exit yet, since we might find other devices.
   * Note that the pfopen() call handles the exact location of the
   * device file (hence we dont have to prepend a prefix for Ultrix
   * or OSF/1 device names).
   */
  if ((fd = pfopen(device, 0)) < 0) {
    return(-1);
  }

  /* can find out the actual device name using the following... */
  /*	if (device == NULL) {
   *		if (ioctl(fd, EIOCIFNAME, &ifr) >= 0) {
   *			device = ifr.ifr_name;
   *		}
   *		else {
   *			device = "pf0";
   *		}
   *	}
   */
  /*
   * We want the ethernet in promiscuous mode
   * The ENTSTAMP is needed to get headers on the pkts.
   */
  enmode = ENBATCH | ENTSTAMP | ENNONEXCL | ENPROMISC;
  if (ioctl(fd, EIOCMBIS, &enmode) < 0) {
    error("ioctl: EIOCMBIS");
    finish(-1);
  }

#ifdef ENCOPYALL
  /*
   * Attempt to set "copyall" mode (see our own packets).
   * Okay if this fails.
   */
  enmode = ENCOPYALL;
  (void) ioctl(fd, EIOCMBIS, &enmode);
#endif

  /*
   * Set the read timeout.
   */
  timeout.tv_sec = SNAPTIME;
  timeout.tv_usec = 0;
  if (ioctl(fd, EIOCSRTIMEOUT, &timeout) < 0) {
    error("ioctl: EIOCSRTIMEOUT");
    finish(-1);
  }

  /* set the backlog */
  if (ioctl(fd, EIOCSETW, &backlog) < 0) {
    error("ioctl: EIOCSETW");
    finish(-1);
  }

  /* set the truncation */
  if (ioctl(fd, EIOCTRUNCATE, &truncation) < 0) {
    error("ioctl: EIOCTRUNCATE");
    finish(-1);
  }

  /* accept all packets */
  Filter.enf_Priority = 37;	/* anything > 2 */
  Filter.enf_FilterLen = 0;	/* means "always true" */
  if (ioctl(fd, EIOCSETF, &Filter) < 0) {
    error("ioctl: EIOCSETF");
    finish(-1);
  }

  return(fd);
}

/*
 * pfilt_devtype - return device type code for packet filter device
 */
int
get_devtype(device, fd)
  char *device;		/* unused */
  int fd;
{
  struct endevp devparams;

  if (ioctl(fd, EIOCDEVP, &devparams) < 0) {
    error("ioctl: EIOCDEVP");
    finish(-1);
  }

  switch (devparams.end_dev_type)
  {
  case ENDT_10MB:
    return(NDT_EN10MB);
		
#ifdef	ENDT_FDDI		/* HACK: to compile prior to Ultrix 4.2 */
  case ENDT_FDDI:
    return(NDT_FDDI);
#endif
#ifdef  ENDT_SLIP
  case ENDT_SLIP:
    return(NDT_SLIP);
#endif
#ifdef  ENDT_PPP
  case ENDT_PPP:
    return(NDT_PPP);
#endif
#ifdef  ENDT_LOOPBACK
  case ENDT_LOOPBACK:
    return(NDT_NULL);		/* return NULL for loopback interface */
#endif
#ifdef  ENDT_NULL
  case ENDT_NULL:
    return(NDT_NULL);
#endif
#ifdef  ENDT_TRN
  case ENDT_TRN:
    return(NDT_PRONET);		/* token ring */
#endif		
  default:
    fprintf(stderr, "Packet filter data-link type %d unknown\n",
	    devparams.end_dev_type);
    fprintf(stderr, "Assuming Ethernet\n");
    return(NDT_EN10MB);
  }
}

/*
 * flush_pfilt - flush data from the packet filter
 */
void
flush_device(fd)
  int fd;
{
  if (ioctl(fd, EIOCFLUSH) < 0) {
    error("ioctl: EIOCFLUSH");
    finish(-1);
  }
}


/*
 * getpkt - extract a 'packet' from the 'chunk'. Return pointer to
 *	    start of packet, length of pkt and number of pkts dropped.
 *	    Device specific.
 */

int
getpkt(fd, ifc_type, ppkt, plen, pwirelen, pdrops)
  int   fd;
  int   ifc_type;			/* interface type */
  char **ppkt ;
  int  *plen ;				/* snap packet length */
  int  *pwirelen ;			/* orig length of pkt at data-link */
  int  *pdrops ;
{
  register int i ;
  static int cc;
  static char *buf;			/* buffer for packet */
  static char *bp;			/* temp pointers */
  struct enstamp stamp;
  int datalen;

  /*
   * Allocate a buffer so it's properly aligned for
   * casting to structure types.
   */
  if (buf == NULL)	/* first time around */
  {
    if ((buf = (char *)malloc(PFILT_CHUNKSIZE)) == NULL)
    {
      (void) fprintf(stderr, "%s: out of memory\n", prognm);
      (void) perror("getpkt malloc");
      finish(-1);
    }

    cc = 0;
  }

  /*
   * Now read packets from the packet filter device.
   */

  if (cc <= 0)	/* no more data left from the previous reads */
  {
    if ((cc = read(fd, buf, PFILT_CHUNKSIZE)) < 0)
    {
      lseek(fd, 0L, 0);
		
      /*
       * Might have read MAXINT bytes.  Try again.
       */
      if ((cc = read(fd, buf, PFILT_CHUNKSIZE)) < 0)
      {
	error("pfilt read");
	finish(-1);
      }
    }

    if (cc == 0)			/* Nothing to read */
      return (NOPKT);

    if (debug > 2)
      fprintf(stderr, "debug2: getpkt read chunk of %d bytes\n", cc);

    bp = buf;
  }
	    
  /*
   * Extract a packet from the chunk.
   */

  (void) bcopy(bp, &stamp, sizeof(stamp));	/* Avoid alignment issues */
		
  if (stamp.ens_stamplen != sizeof(stamp))
  {
    if (debug)
      fprintf (stderr, "%s: Bad packet, stamplen mismatch\n", prognm);

    cc = 0;
    return (NOPKT);			/* Treat entire buffer as garbage. */
  }
		
  *pdrops = stamp.ens_dropped;	/* Number of dropped packets */
  datalen = stamp.ens_count;

  *pwirelen = datalen;		/* Actual size of packet */
		
  if (datalen > truncation)		/* Extract size of data in snap pkt */
    datalen = truncation;

  *plen = datalen ;

  /* Timestamp in  &stamp.ens_tstamp  */

  if (ifc_type == NDT_FDDI)
    *ppkt = &(bp[sizeof(stamp) + 3]); 	/* wierd Ultrix FDDI padding */
  else
    *ppkt = &(bp[sizeof(stamp)]);
		
  /*
   * Skip over this packet.
   */
  datalen = ENALIGN(datalen) ;
  datalen += sizeof(stamp);

  cc -= datalen;

  if (cc > 0)
    bp += datalen;
  else
  {
    cc = 0;
    return (ENDOFCHUNK);
  }

  return (OKPKT);

}	/* end:  getpkt() */

#endif /* USE_PFILT */

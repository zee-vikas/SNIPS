#ifndef lint
static char *RCSid = "$Header$" ;
#endif

#include "os.h"

#ifdef USE_NIT

/* Routines for grabbing packets from the NIT interface for use with
 * 'etherload'.
 *
 *	 -Vikas Aggarwal, vikas@navya_.com
 */


/* Adapted from nfswatch v4.1.  Original comments and header follow:
 *
 *
 * nit.c - routines for messing with the network interface tap.
 *
 *  davy/system/nfswatch/RCS/nit.c,v 4.0 1993/03/01 19:59:00 davy Exp $";
 *
 * David A. Curry
 * Purdue University
 * davy@ecn.purdue.edu
 *
 * ******** End original header *********
 */

/*
 * $Log$
 * Revision 1.0  2001/07/08 21:47:32  vikas
 * For SNIPS v1.0
 *
 *
 */

#include <sys/param.h>
#include <sys/stropts.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/file.h>
#include <net/if.h>
#include <signal.h>
#include <stdio.h>

#include <net/nit_if.h>
#include <net/nit_buf.h>

#include "etherload.h"
#include "externs.h"	/* */

#define NIT_DEV	"/dev/nit"		/* network interface tap device	*/
#define NIT_BUF	"nbuf"			/* nit stream buffering module	*/
#define NIT_CHUNKSIZE	8192		/* chunk size for grabbing pkts	*/

#if NOFILE <= 64
# define SUNOS40 1
#endif

char	*os_devices[] = {
  "le0", "le1", "le2", "le3", "le4",
  "ie0", "ie1", "ie2", "ie3", "ie4",
  "fddi0", "fddi1", "fddi2", "fddi3", "fddi4",
  "bf0", "bf1",
  0
};


/*
 * setup_device - set up the network interface tap (NIT)
 */
int
setup_device(device)
  char *device;
{
  int n, s, fd;
  u_int chunksz;
  u_long if_flags;
  struct strioctl si;
  struct timeval timeout;
  struct ifreq ifr;

  if (!device  || *device == NULL) {
    error("no device name");
    finish (-1);
  }

  if_flags = NI_TIMESTAMP | NI_DROPS | NI_PROMISC | NI_LEN;

  /*
   * Open the network interface tap.
   */
  if ((fd = open(NIT_DEV, O_RDONLY)) < 0) {
    error("nit: open");
    finish(-1);
  }

  /*
   * Arrange to get discrete messages.
   */
  if (ioctl(fd, I_SRDOPT, (char *) RMSGD) < 0) {
    error("ioctl: I_SRDOPT");
    finish(-1);
  }

  /*
   * Push and configure the nit buffering module.
   */
  if (ioctl(fd, I_PUSH, NIT_BUF) < 0) {
    error("ioctl: I_PUSH NIT_BUF");
    finish(-1);
  }

  /*
   * Set the read timeout.
   */
  timeout.tv_sec = SNAPTIME;	/* typically 1-2 secs */
  timeout.tv_usec = 0;

  si.ic_cmd = NIOCSTIME;
  si.ic_timout = INFTIM;
  si.ic_len = sizeof(timeout);
  si.ic_dp = (char *) &timeout;

  if (ioctl(fd, I_STR, (char *) &si) < 0) {
    error("ioctl: I_STR NIOCSTIME");
    finish(-1);
  }

  /*
   * Set the chunk size.
   */
  chunksz = NIT_CHUNKSIZE;

  si.ic_cmd = NIOCSCHUNK;
  si.ic_len = sizeof(chunksz);
  si.ic_dp = (char *) &chunksz;

  if (ioctl(fd, I_STR, (char *) &si) < 0) {
    error("ioctl: I_STR NIOCSCHUNK");
    finish(-1);
  }

  /*
   * Configure the network interface tap by binding it
   * to the underlying interface, setting the snapshot
   * length, and setting the flags.
   */
  (void) strncpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));
  ifr.ifr_name[sizeof(ifr.ifr_name)-1] = '\0';

  si.ic_cmd = NIOCBIND;
  si.ic_len = sizeof(ifr);
  si.ic_dp = (char *) &ifr;

  /*
   * If the bind fails, there's no such device. Dont do a
   * 'finish' here, since we might still be probing for devices...
   */
  if (ioctl(fd, I_STR, (char *) &si) < 0) {
    close(fd);
    return(-1);
  }

  /*
   * SNAP is buggy on SunOS 4.0.x
   */
#ifndef SUNOS40
  si.ic_cmd = NIOCSSNAP;
  si.ic_len = sizeof(truncation);
  si.ic_dp = (char *) &truncation;

  if (ioctl(fd, I_STR, (char *) &si) < 0) {
    error("ioctl: I_STR NIOCSSNAP");
    finish(-1);
  }
#endif

  si.ic_cmd = NIOCSFLAGS;
  si.ic_len = sizeof(if_flags);
  si.ic_dp = (char *) &if_flags;

  if (ioctl(fd, I_STR, (char *) &si) < 0) {
    error("ioctl: I_STR NIOCSFLAGS");
    finish(-1);
  }

  return(fd);
}

/*
 * flush_nit - flush data from the nit.
 */
void
flush_device(fd)
  int fd;
{
  if (ioctl(fd, I_FLUSH, (char *) FLUSHR) < 0) {
    error("ioctl: I_FLUSH");
    finish(-1);
  }
}

/*
 * nit_devtype - determine the type of device we're looking at.
 */
int
get_devtype(device, fd)
  char *device;
int  fd;		/* open file descriptor, unused by SunOS */
{
  /*
   * This whole routine is a kludge.  Ultrix does it the
   * right way; see pfilt.c.
	 */

  if (strncmp(device, "le", 2) == 0 || strncmp(device, "ie", 2) == 0)
    return(NDT_EN10MB);
	
  if (strncmp(device, "fddi", 4) == 0)
    return(NDT_FDDI);

  fprintf(stderr, "Unknown device type: %s -- assuming ethernet.\n",
	  device);
  return(NDT_EN10MB);
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
  register int i;
  static int cc;
  static char *buf;			/* buffer for packet */
  static char *bp;			/* temp pointers */
  register char *cp;
  struct nit_bufhdr *hdrp;		/* pkt header */
  struct nit_ifdrops *ndp;		/* dropped pkts */
  struct nit_iftime *tstamp;		/* timestamp */
  struct nit_iflen  *nlp;		/* orig length of pkt */

  /*
   * Allocate a buffer so it's properly aligned for
   * casting to structure types.
   */
  if (buf == NULL)	/* first time around */
  {
    if ((buf = (char *)malloc(NIT_CHUNKSIZE)) == NULL)
    {
      (void) fprintf(stderr, "%s: out of memory\n", prognm);
      (void) perror("getpkt malloc");
      finish(-1);
    }

    cc = 0;
  }

  /*
   * Now read packets from the nit device.
   */
  if (cc <= 0)	/* no more data left to parse from previous read */
  {
    if ((cc = read(fd, buf, NIT_CHUNKSIZE)) <= 0)
      return (NOPKT);
    
    if (debug > 2)
      fprintf(stderr, "debug2: getpkt read chunk of %d bytes\n", cc);

    bp = buf;
  }
    
  /*
   * Extract a packet from the chunk.
   */
  cp = bp;
		
  /*
   * Get the nit header.
   */
  hdrp = (struct nit_bufhdr *) cp;
  cp += sizeof(struct nit_bufhdr);
  *plen = hdrp->nhb_msglen ;

  /*
   * Get the time stamp. Only if used IOCTL on the device.
   */
  tstamp = (struct nit_iftime *) cp;	/* */
  cp += sizeof(struct nit_iftime);	/* */
    
  /*
   * Get the number of dropped packets.
   */
  ndp = (struct nit_ifdrops *) cp;
  cp += sizeof(struct nit_ifdrops);
  *pdrops = ndp->nh_drops;

  nlp = (struct nit_iflen *)cp;	/* extract orig size of pkt */
  cp += sizeof (struct nit_iflen);
  *pwirelen = nlp->nh_pktlen;
     
  *ppkt = cp ;				/* ptr to start of packet */

  /*
   * Set pointers to skip over this packet for next pass.
   */
  cc -= hdrp->nhb_totlen;
  if (cc > 0)
    bp += hdrp->nhb_totlen;			/* Skip over this packet. */
  else
  {
    cc = 0;
    return (ENDOFCHUNK);		/* valid packet, but end of chunk */
  }

  return (OKPKT) ;			/* valid packet */

}	/* end: getpkt() */



#endif /* USE_NIT */

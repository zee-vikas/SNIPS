#ifndef lint
static char *RCSid = "$Header$";
#endif

#include "os.h"

#ifdef USE_DLPI

/*
 * Routines for grabbing packets from the DLPI interface. This module is
 * a part of the 'etherload' package.
 *
 *  Vikas Aggarwal, vikas@navya_.com
 */


/* Adapted from nfswatch v4.1. Original comments follow:
 *
 * dpli.c - routines for messing with the Data Link Provider Interface.
 *
 * davy/nfswatch/RCS/dlpi.c,v 4.1 1993/09/15 20:50:44 davy Exp $";
 *
 * The code in this module is based in large part (especially the dl*
 * routines) on the example code provided with the document "How to Use
 * DLPI", by Neal Nuckolls of Sun Internet Engineering.  Gotta give credit
 * where credit is due.  If it weren't for Neal's excellent document,
 * this module just plain wouldn't exist.
 *
 * David A. Curry
 * Purdue University
 * davy@ecn.purdue.edu
 *
 * ******* End original comments ********
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
#include <sys/stream.h>
#include <sys/dlpi.h>
#ifdef SUNOS5
# include <sys/bufmod.h>
# include <stdlib.h>
#endif
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <net/if.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "etherload.h"
#include "externs.h"

#define DLPI_DEVDIR		"/dev/"		/* path to device dir	*/
#define DLPI_BUFMOD		"bufmod"	/* streams buffering	*/
#define DLPI_MAXWAIT		15		/* max timeout		*/
#define DLPI_MAXDLBUF		8192		/* buffer size		*/
#define DLPI_MAXDLADDR		1024		/* max address size	*/
#define DLPI_CHUNKSIZE		(8192 * sizeof(long))	/* buffer size	*/
#define DLPI_DEFAULTSAP	0x0800			/* IP protocol		*/

#ifdef SUNOS5
char	*os_devices[] = {
  "le0", "le1", "le2", "le3", "le4",
  "ie0", "ie1", "ie2", "ie3", "ie4",
  "hme0", "hme1", "hme2", "hme3", "hme4",
  "fddi0", "fddi1", "fddi2", "fddi3", "fddi4",
  0
};
#else
char	*os_devices[] = {
  "emd0", "emd1", "emd2", "emd3", "emd4",
  0
};
#endif	/* SUNOS5 */

static void	dlbindreq();
static void	dlinforeq();
static void	dlattachreq();
static void	dlpromisconreq();
static void	sigalrm();
static int	dlokack();
static int	dlinfoack();
static int	dlbindack();
static int	expecting();
static int	strgetmsg();

/*
 * setup_dlpi_dev - set up the data link provider interface.
 */
int
setup_device(device)
  char *device;
{
  char *p;
  u_int chunksz;
  struct strioctl si;
  char devname[BUFSIZ];
  int n, s, fd, devppa;
  struct timeval timeout;
  long buf[DLPI_MAXDLBUF];


  if (!device  || *device == NULL) {
    error("no device name");
    finish (-1);
  }

  /*
     * Split the device name into type and unit number.
     */
  if ((p = strpbrk(device, "0123456789")) == NULL)
    return(-1);

  strcpy(devname, DLPI_DEVDIR);
  strncat(devname, device, p - device);
  devppa = atoi(p);

  /*
     * Open the device.
     */
  if ((fd = open(devname, O_RDWR)) < 0) {
    if (errno == ENOENT || errno == ENXIO)
      return(-1);

    error(devname);
    finish(-1);
  }

  /*
     * Attach to the device.  If this fails, the device
     * does not exist.
     */
  dlattachreq(fd, devppa);
    
  if (dlokack(fd, buf) < 0) {
    close(fd);
    return(-1);
  }

  /*
     * We want the ethernet in promiscuous mode if we're looking
     * at nodes other than ourselves.
     */
#ifdef DL_PROMISC_PHYS
  dlpromisconreq(fd, DL_PROMISC_PHYS);
	
  if (dlokack(fd, buf) < 0) {
    fprintf(stderr, "%s: DL_PROMISC_PHYS failed.\n", prognm);
    finish(-1);
  }
#else
  fprintf(stderr,
	  "%s: DLPI 1.3 does not support promiscuous mode operation\n",
	  prognm);
  fprintf(stderr, "%s: cannot monitor the ethernet.\n",
	  prognm);
  finish(-1);
#endif

  /*
     * Bind to the specific unit.
     */
  dlbindreq(fd, DLPI_DEFAULTSAP, 0, DL_CLDLS, 0, 0);

  if (dlbindack(fd, buf) < 0) {
    fprintf(stderr, "%s: dlbindack failed.\n", prognm);
    finish(-1);
  }

#ifdef SUNOS5
  /*
     * We really want all types of packets.  However, the SVR4 DLPI does
     * not let you have the packet frame header, so we won't be able to
     * distinguish protocol types.  But SunOS5 gives you the DLIOCRAW
     * ioctl to get the frame headers, so we can do this on SunOS5.
     */
  dlpromisconreq(fd, DL_PROMISC_SAP);

  if (dlokack(fd, buf) < 0) {
    fprintf(stderr, "%s: DL_PROMISC_SAP failed.\n", prognm);
    finish(-1);
  }

  /*
     * We want raw packets with the packet frame header.  But we can
     * only get this in SunOS5 with the DLIOCRAW ioctl; it's not in
     * standard SVR4.
     */
  si.ic_cmd = DLIOCRAW;
  si.ic_timout = -1;
  si.ic_len = 0;
  si.ic_dp = 0;

  if (ioctl(fd, I_STR, &si) < 0) {
    error("ioctl: I_STR DLIOCRAW");
    finish(-1);
  }
#endif /* SUNOS5 */

  /*
     * Arrange to get discrete messages.
     */
  if (ioctl(fd, I_SRDOPT, (char *) RMSGD) < 0) {
    error("ioctl: I_SRDOPT RMSGD");
    finish(-1);
  }

#ifdef SUNOS5
  /*
     * Push and configure the streams buffering module.  This is once
     * again SunOS-specific.
     */
  if (ioctl(fd, I_PUSH, DLPI_BUFMOD) < 0) {
    error("ioctl: I_PUSH BUFMOD");
    finish(-1);
  }

  /*
     * Set the read timeout.
     */
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;

  si.ic_cmd = SBIOCSTIME;
  si.ic_timout = INFTIM;
  si.ic_len = sizeof(timeout);
  si.ic_dp = (char *) &timeout;

  if (ioctl(fd, I_STR, (char *) &si) < 0) {
    error("ioctl: I_STR SBIOCSTIME");
    finish(-1);
  }

  /*
     * Set the chunk size.
     */
  chunksz = DLPI_CHUNKSIZE;

  si.ic_cmd = SBIOCSCHUNK;
  si.ic_len = sizeof(chunksz);
  si.ic_dp = (char *) &chunksz;

  if (ioctl(fd, I_STR, (char *) &si) < 0) {
    error("ioctl: I_STR SBIOCSCHUNK");
    finish(-1);
  }

  /*
     * Set snapshot mode.
     */
  si.ic_cmd = SBIOCSSNAP;
  si.ic_len = sizeof(truncation);
  si.ic_dp = (char *) &truncation;

  if (ioctl(fd, I_STR, (char *) &si) < 0) {
    error("ioctl: I_STR SBIOCSSNAP");
    finish(-1);
  }
#endif /* SUNOS5 */

  return(fd);
}

/*
 * flush_dlpi - flush data from the dlpi.
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
 * dlpi_devtype - determine the type of device we're looking at.
 */
int
get_devtype(device, fd)
  char *device;		/* unused by dlpi interface */
int fd;
{
  long buf[DLPI_MAXDLBUF];
  union DL_primitives *dlp;

  dlp = (union DL_primitives *) buf;

  dlinforeq(fd);

  if (dlinfoack(fd, buf) < 0)
    return(NDT_EN10MB);

  switch (dlp->info_ack.dl_mac_type) {
  case DL_CSMACD:
  case DL_ETHER:
    if (strncmp(device, "hme", 3) == 0)
      return(NDT_EN100MB);
    else
      return(NDT_EN10MB);
#ifdef DL_FDDI
  case DL_FDDI:
    return(NDT_FDDI);
#endif
  default:
    fprintf(stderr, "%s: DLPI MACtype %d unknown, ", prognm,
	    dlp->info_ack.dl_mac_type);
    fprintf(stderr, "assuming ethernet.\n");
    return(NDT_EN10MB);
  }
}

/*
 * dlinforeq - request information about the data link provider.
 */
static void
dlinforeq(fd)
  int fd;
{
  dl_info_req_t info_req;
  struct strbuf ctl;
  int flags;

  info_req.dl_primitive = DL_INFO_REQ;

  ctl.maxlen = 0;
  ctl.len = sizeof (info_req);
  ctl.buf = (char *) &info_req;

  flags = RS_HIPRI;

  if (putmsg(fd, &ctl, (struct strbuf *) NULL, flags) < 0) {
    error("putmsg");
    finish(-1);
  }
}

/*
 * dlattachreq - send a request to attach.
 */
static void
dlattachreq(fd, ppa)
  u_long ppa;
  int fd;
{
  dl_attach_req_t	attach_req;
  struct strbuf ctl;
  int flags;

  attach_req.dl_primitive = DL_ATTACH_REQ;
  attach_req.dl_ppa = ppa;

  ctl.maxlen = 0;
  ctl.len = sizeof (attach_req);
  ctl.buf = (char *) &attach_req;

  flags = 0;

  if (putmsg(fd, &ctl, (struct strbuf*) NULL, flags) < 0) {
    error("putmsg");
    finish(-1);
  }
}

#ifdef DL_PROMISCON_REQ
/*
 * dlpromisconreq - send a request to turn promiscuous mode on.
 */
static void
dlpromisconreq(fd, level)
  u_long level;
  int fd;
{
  dl_promiscon_req_t promiscon_req;
  struct strbuf ctl;
  int flags;

  promiscon_req.dl_primitive = DL_PROMISCON_REQ;
  promiscon_req.dl_level = level;

  ctl.maxlen = 0;
  ctl.len = sizeof (promiscon_req);
  ctl.buf = (char *) &promiscon_req;

  flags = 0;

  if (putmsg(fd, &ctl, (struct strbuf*) NULL, flags) < 0) {
    error("putmsg");
    finish(-1);
  }
}
#endif /* DL_PROMISCON_REQ */

/*
 * dlbindreq - send a request to bind.
 */
static void
dlbindreq(fd, sap, max_conind, service_mode, conn_mgmt, xidtest)
  u_long sap, max_conind, service_mode, conn_mgmt, xidtest;
  int fd;
{
  dl_bind_req_t bind_req;
  struct strbuf ctl;
  int flags;

  bind_req.dl_primitive = DL_BIND_REQ;
  bind_req.dl_sap = sap;
  bind_req.dl_max_conind = max_conind;
  bind_req.dl_service_mode = service_mode;
  bind_req.dl_conn_mgmt = conn_mgmt;
#ifdef DL_PROMISC_PHYS
  /*
   * DLPI 2.0 only?
   */
  bind_req.dl_xidtest_flg = xidtest;
#endif

  ctl.maxlen = 0;
  ctl.len = sizeof (bind_req);
  ctl.buf = (char *) &bind_req;

  flags = 0;

  if (putmsg(fd, &ctl, (struct strbuf*) NULL, flags) < 0) {
    error("putmsg");
    finish(-1);
  }
}

/*
 * dlokack - general acknowledgement reception.
 */
static int
dlokack(fd, bufp)
  char *bufp;
  int fd;
{
  union DL_primitives *dlp;
  struct strbuf ctl;
  int flags;

  ctl.maxlen = DLPI_MAXDLBUF;
  ctl.len = 0;
  ctl.buf = bufp;

  if (strgetmsg(fd, &ctl, (struct strbuf*)NULL, &flags, "dlokack") < 0)
    return(-1);

  dlp = (union DL_primitives *) ctl.buf;

  if (expecting(DL_OK_ACK, dlp) < 0)
    return(-1);

  if (ctl.len < sizeof (dl_ok_ack_t))
    return(-1);

  if (flags != RS_HIPRI)
    return(-1);

  if (ctl.len < sizeof (dl_ok_ack_t))
    return(-1);

  return(0);
}

/*
 * dlinfoack - receive an ack to a dlinforeq.
 */
static int
dlinfoack(fd, bufp)
  char *bufp;
  int fd;
{
  union DL_primitives *dlp;
  struct strbuf ctl;
  int flags;

  ctl.maxlen = DLPI_MAXDLBUF;
  ctl.len = 0;
  ctl.buf = bufp;

  if (strgetmsg(fd, &ctl, (struct strbuf *)NULL, &flags, "dlinfoack") < 0)
    return(-1);

  dlp = (union DL_primitives *) ctl.buf;

  if (expecting(DL_INFO_ACK, dlp) < 0)
    return(-1);

  if (ctl.len < sizeof (dl_info_ack_t))
    return(-1);

  if (flags != RS_HIPRI)
    return(-1);

  if (ctl.len < sizeof (dl_info_ack_t))
    return(-1);

  return(0);
}

/*
 * dlbindack - receive an ack to a dlbindreq.
 */
static int
dlbindack(fd, bufp)
  char *bufp;
  int fd;
{
  union DL_primitives *dlp;
  struct strbuf ctl;
  int flags;

  ctl.maxlen = DLPI_MAXDLBUF;
  ctl.len = 0;
  ctl.buf = bufp;

  if (strgetmsg(fd, &ctl, (struct strbuf*)NULL, &flags, "dlbindack") < 0)
    return(-1);

  dlp = (union DL_primitives *) ctl.buf;

  if (expecting(DL_BIND_ACK, dlp) < 0)
    return(-1);

  if (flags != RS_HIPRI)
    return(-1);

  if (ctl.len < sizeof (dl_bind_ack_t))
    return(-1);

  return(0);
}

/*
 * expecting - see if we got what we wanted.
 */
static int
expecting(prim, dlp)
  union DL_primitives *dlp;
  int prim;
{
  if (dlp->dl_primitive != (u_long)prim)
    return(-1);

  return(0);
}

/*
 * strgetmsg - get a message from a stream, with timeout.
 */
static int
strgetmsg(fd, ctlp, datap, flagsp, caller)
  struct strbuf *ctlp, *datap;
  char *caller;
  int *flagsp;
  int fd;
{
  int rc;
  void sigalrm();

  /*
   * Start timer.
   */
  (void) sigset(SIGALRM, sigalrm);

  if (alarm(DLPI_MAXWAIT) < 0) {
    error("alarm");
    finish(-1);
  }

  /*
   * Set flags argument and issue getmsg().
   */
  *flagsp = 0;
  if ((rc = getmsg(fd, ctlp, datap, flagsp)) < 0) {
    error("getmsg");
    finish(-1);
  }

  /*
   * Stop timer.
   */
  if (alarm(0) < 0) {
    error("alarm");
    finish(-1);
  }

  /*
   * Check for MOREDATA and/or MORECTL.
   */
  if ((rc & (MORECTL | MOREDATA)) == (MORECTL | MOREDATA))
    return(-1);
  if (rc & MORECTL)
    return(-1);
  if (rc & MOREDATA)
    return(-1);

  /*
   * Check for at least sizeof (long) control data portion.
   */
  if (ctlp->len < sizeof (long))
    return(-1);

  return(0);
}

/*
 * sigalrm - handle alarms.
 */
static void
sigalrm()
{
  (void) fprintf(stderr, "dlpi: timeout\n");
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
  static int cc;
  static char *buf;			/* buffer for packet */
  static char *bp;			/* temp pointers */
  static struct strbuf strdata;		/* needs to be static */
  struct sb_hdr *hdrp;
  int flags;

  /*
   * Allocate a buffer so it's properly aligned for
   * casting to structure types.
   */
  if (buf == NULL)	/* first time around */
  {
    if ((buf = (char *)malloc(DLPI_CHUNKSIZE)) == NULL)
    {
      (void) fprintf(stderr, "%s: out of memory\n", prognm);
      (void) perror("getpkt malloc");
      finish(-1);
    }

    cc = 0;

    strdata.len = 0;
    strdata.buf = buf;
    strdata.maxlen = DLPI_CHUNKSIZE;
  }

  /*
   * Now read packets from the dlpi device.
   */

  if (cc <= 0)		/* no more data left to parse from prev read */
  {
    flags = 0;
    strdata.len = 0;
    if (getmsg(fd, NULL, &strdata, &flags) != 0)
      return (0);

    cc = strdata.len;		/* bytes of data in the chunk */
    if (cc == 0)			/* Nothing to read */
      return (NOPKT);

    if (debug > 2)
      fprintf(stderr, "debug2: getpkt read chunk of %d bytes\n", cc);

    bp = buf;
  }
    

#ifdef SUNOS5
  /*
   * Loop through the chunk, extracting packets.
   */
		
  /*
   * Get the nit header.
   * sb_origlen is the original length of the message after the 
   * M_PROTO => M_DATA conversion, but before truncating or adding
   * the header.
   *
   * sb_msglen is the length of the message after truncation, but
   * before adding the header.
   *
   * sb_totlen is the length of the message after truncation, and
   * including both the header itself and the trailing padding bytes.
   *
   * sb_drops is the cumulative number of messages dropped by the module
   * due to stream read-side flow control or resource exhaustion.
   *
   * sb_timestamp is the packet arrival time expressed as a 'struct timeval'.
   */
    
  hdrp = (struct sb_hdr *) bp;
  *ppkt = bp + sizeof(struct sb_hdr);

  *plen = hdrp->sbh_msglen ;
  *pwirelen = hdrp->sbh_origlen;
  *pdrops = hdrp->sbh_drops;

  /* timestamp is in  &hdrp->sbh_timestamp  */

  /*
   * Skip over this packet.
   */
  cc -= hdrp->sbh_totlen;

  if (cc > 0)
    bp += hdrp->sbh_totlen;
  else
  {
    cc =0;
    return (ENDOFCHUNK);
  }

  return (OKPKT);			/* valid packet */

#else  /* !SUNOS5 */

  /*
   * It's just a packet, no buffering or chunks of data.
   */
  if (strdata.len)
  {
    /*
     * Since we're not on SunOS5, that means we
     * don't have DLIOCRAW, so we don't have the
     * packet frame header.  So, we need to
     * bypass that level of filtering.
     */
    *ppkt = bp;
    *plen = strdata.len;
    *pwirelen = strdata.len;
    *pdrops = 0;
    return (ENDOFCHUNK);
  }

#endif  /* SUNOS5 */

}	/* end: getpkt() */

#endif /* USE_DLPI */


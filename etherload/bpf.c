#ifndef lint
static char *RCSid = "$Header$" ;
#endif

#include "os.h"

#ifdef USE_BPF

/*
 * Routines for grabbing packets from the Berkeley Packet Filter (bpf)
 * for use with etherload.
 *
 * BPF specific stuff adapted from tcpdump-v2.0/cap-bpf.c
 *
 * Author:
 *	Vikas Aggarwal, vikas@navya_.com
 *
 */

/*
 * $Log$
 * Revision 1.0  2001/07/08 21:47:32  vikas
 * For SNIPS v1.0
 *
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/file.h>
#include <net/if.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>

/* #include <netinet/in.h>	/* */
/* #include <netinet/if_ether.h>	/* */
#include <net/bpf.h>

#include "etherload.h"
#include "externs.h"	/* */

/* we find an unused pseudo device name by searching /dev/bpf0 /dev/bpf1 ... */
char  **os_devices = NULL ;

/*
 * setup_device - set up the BPF device
 */

#ifndef BPF_RET
# define BPF_RET	RetOp		/* older versions */
# define BPF_K		0		/* not needed */
#endif

int
setup_device(device)
  char *device;
{
  int fd, n;
  char bpfdev[sizeof "/dev/bpf000"];
  struct timeval timeout;
  struct ifreq ifr;
  struct bpf_version bv;
  static struct bpf_insn bpf_prog[] = {	/* filter program */
    BPF_STMT(BPF_RET+BPF_K, 0),		/* save space for snaplen */
  };
  static struct bpf_program bpfilt = {	/* filter structure */
    sizeof bpf_prog / sizeof(bpf_prog[0]),
    bpf_prog
  };

  if (!device  || *device == NULL) {
    error("no device name");
    finish (-1);
  }
	  
  /* Go thru all the pseudo-devices and find one that is not in use */
  n = 0;
  do {

    (void)sprintf(bpfdev, "/dev/bpf%d", n++);
    fd = open(bpfdev, O_RDONLY);
  } while (fd < 0 && errno == EBUSY);

  if (fd < 0) {
    error("No unused bpf device found");
    /* error(bpfdev);	/* */
    finish(-1);
  }

  if (ioctl(fd, BIOCVERSION, (caddr_t)&bv) < 0) {
    error ("BIOCVERSION");
    finish(-1);
  }

  if (bv.bv_major != BPF_MAJOR_VERSION || bv.bv_minor < BPF_MINOR_VERSION)
  {
    error ("kernel bpf filter out of date");
    finish(-1);
  }

  /* Associate the hardware interface with the file-descriptor. */
  (void)strncpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));
  if (ioctl(fd, BIOCSETIF, (caddr_t)&ifr) < 0) {
    error("BIOCSETIF");
    finish(-1);
  }
  /* set timeout */
  timeout.tv_sec = SNAPTIME;
  timeout.tv_usec = 0;
  if (ioctl(fd, BIOCSRTIMEOUT, (caddr_t)&timeout) < 0) {
    error("BIOCSRTIMEOUT");
    finish(-1);
  }
  /* set promiscuous mode */
  if (ioctl(fd, BIOCPROMISC, (void *)0) < 0) {
    error("BIOCPROMISC");
    finish (-1);
  }

  /* Read buffer length is pre-defined (set to MCLBYTES) */

    /* truncation length set in the filter */
  bpf_prog[0].k = truncation;

  /* set filter on the device */
  if (ioctl(fd, BIOCSETF, (caddr_t)&bpfilt) < 0) {
    error("BIOSETF");
    finish (-1);
  }

  return(fd);
}


/*
 * flush_bpf - flush data from the BPF interface
 */
void
flush_device(fd)
  int fd;
{
  if (ioctl(fd, BIOCFLUSH, (void *)0 ) < 0) {
    error("ioctl: BIOCFLUSH");
    finish(-1);
  }
}


/*
 * bpf_devtype - determine the type of device we're looking at.
 * We can afford to just return the ioctl() value since the NDT_xx
 * definitions map into the bpf defintiions.
 */
int
get_devtype(device, fd)
  char *device;			/* unused */
int  fd;
{
#ifndef BIOCGDLT
  struct bpf_devp devp;
    
  if (ioctl(fd, BIOCDEVP, (caddr_t)&devp) < 0) {
    error("ioctl: BIOCDEVP");
    finish(-1);
  }

  return (devp.bdev_type);
#else
  u_int	dev_type;
  ioctl(fd, BIOCGDLT, (caddr_t)&dev_type );
  return(dev_type);
#endif
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
  static u_int bufsize;
  struct bpf_hdr *bhp;			/* header pointer */
  struct bpf_stat bstats;		/* drop stats */

  /*
   * Allocate a buffer so it's properly aligned for
   * casting to structure types.
   */
  if (buf == NULL)	/* first time around */
  {
    if (ioctl(fd, BIOCGBLEN, (caddr_t)&bufsize) < 0)
    {
      perror("getting buffer length");
      finish (-1);
    }
    if ((buf = (char *)malloc(bufsize)) == NULL)
    {
      (void) fprintf(stderr, "%s: out of memory\n", prognm);
      (void) perror("getpkt malloc");
      finish(-1);
    }

    cc = 0;
  }

  /*
   * Now read packets from the bpf device.
   */
  if (cc <= 0)		/* no more data left to parse from previous read */
  {
    if ((cc = read(fd, buf, bufsize)) <= 0)
    {
      lseek(fd, 0L, 0);
 
      /*
       * Might have read MAXINT bytes on SunOS.  Try again.
       */
      if ((cc = read(fd, buf, bufsize)) < 0)
      {
	error("bpf read");
	finish(-1);
      }
    }

    if (cc == 0)			/* nothing to read */
      return (NOPKT);
    
    if (debug > 2)
      fprintf(stderr, "debug2: getpkt read chunk of %d bytes\n", cc);

    bp = buf;
  }
    
  /*
   * Extract a packet from the chunk.
   */

  bhp = (struct bpf_hdr *) bp;
  *pwirelen 	= (int)bhp->bh_datalen;	/* wire length */
  *plen 	= (int)bhp->bh_caplen;	/* capture length */
  *ppkt = bp + bhp->bh_hdrlen;		/* ptr to start of packet */

  /*
   * Get the number of dropped packets.
   */
  ioctl(fd, BIOCGSTATS, &bstats);
  *pdrops = (int)bstats.bs_drop;


  /*
   * Set pointers to skip over this packet for next pass.
   */
  cc -= BPF_WORDALIGN(bhp->bh_caplen + bhp->bh_hdrlen);
  if (cc > 0)		/* Skip over this packet. */
    bp += BPF_WORDALIGN(bhp->bh_caplen + bhp->bh_hdrlen);
  else
  {
    cc = 0;
    return (ENDOFCHUNK);		/* valid packet, but end of chunk */
  }

  return (OKPKT) ;			/* valid packet */

}	/* end: getpkt() */



#endif /* USE_BPF */

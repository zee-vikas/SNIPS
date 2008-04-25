#ifndef lint
static char *RCSid = "$Header$";
#endif

#include "os.h"

#ifdef USE_LINUX

/*
 * Routines to extract ethernet packets from Linux network interfaces.
 *
 * Linux supports a low level SOCK_PACKET scheme. Didn't seem to be detecting
 * packets that were not destined for itself.
 *
 * BUGS:
 *	On older versions of Linux kernels, the bind() does not give
 * the proper error ENODEV if the device does not exist. In such cases,
 * change the list of os_devices[] to just eth0 (i.e. existing devices)
 *
 * AUTHOR:
 *	Vikas Aggarwal, vikas@navya_.com
 *
 */

/* Sources of information for Linux network device programming:
 *      /usr/include/linux/{netdevice.h, if_ether.h}
 *      /usr/src/linux/drivers/nnet
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

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#ifdef __GLIBC__			/* linux 2.0 gcc lib has changed */
# include <sys/socket.h>
#else
# include <asm/types.h>
# include <sys/socket.h>		/* some Linux's dont like this */
#endif
#include <sys/ioctl.h>
#include <sys/time.h>
#ifdef __GLIBC__
# include <netinet/in.h>
# include <net/if.h>
# include <net/ethernet.h>
#else
# include <linux/in.h>
# include <linux/netdevice.h>         /* interface specific definitions */
#endif

#include "etherload.h"
#include "externs.h"

#define  RCV_BUFFER_SZ  (8 * 1024)              /* for grabbing packets */

/*
 * Linux 1.x does not return a valid errno if the device does not exist...
 */
#ifdef LINUX1
char *os_devices[] = { "eth0", "eth1", "eth2", "eth3", "eth4", 0 };
#else
char *os_devices[] = {
  "eth0", "eth1", "eth2", "eth3", "eth4", 	/* device list */
  "sl0", "sl1", "sl2", "ppp0", "ppp1", "ppp2",
  0
};
#endif

int setup_device(device)
  char *device;
{
  int s, i;
  struct timeval timeout;
  struct sockaddr sa;
    
  if (!device  || *device == '\0') {
    error("no device name");
    finish (-1);
  }
  if (debug)
    fprintf(stderr, "setup_device() %s\n", device);
  /*
   * Make the raw socket (see include/in.h)
   * alan@cymru.net  says to use:
   *            socket(AF_INET, SOCK_PACKET, htons(ETH_P_ALL)
   */
  if ((s = socket(AF_INET, SOCK_PACKET, htons(ETH_P_ALL)) ) < 0) {
    error("setup_device: socket");
    finish(-1);
  }

  /*
   * Set it up for the chosen interface.
   */
  bzero((char *)&sa, sizeof(sa));
  sa.sa_family = AF_INET;	/* Note the family is NOT AF_UNIX */
  strncpy(sa.sa_data, device, sizeof(sa.sa_data));
  
  /*
   * If bind fails:
   * Linux v1.2.13  always returned  "Invalid Argument" EINVAL for all devices
   * (even valid ones)
   * Linux v2.x returns "No such device"  ENODEV for invalid devices
   *
   * However, the ioctl() call below fails in either case if the device
   * does not exist (proper behaviour). Hence, do a check in ioctl() also.
   */
  if (bind(s, &sa, sizeof(sa)) < 0)
    if (errno && errno != EINVAL)
    {
      /* perror("bind");		/* need not print out the error */
      close(s);
      return(-1);
    }

  /*
   * enable promiscous mode so we see all packets. Can also do
   *		ifconfig eth0 promisc
   * at the shell
   */

  if (1)
  {
    struct ifreq ifr;

    strcpy(ifr.ifr_name, device);
    if (ioctl(s, SIOCGIFFLAGS, &ifr) < 0)	/* get the flags */
    {
      if (errno == ENODEV)	/* the device does not exist */
      {
	close(s);
	return(-1);
      }

      error("ioctl GETFLAGS");
      finish(-1);
    }
    strcpy(ifr.ifr_name, device);	/* copy over the name again... */
    ifr.ifr_flags |= IFF_PROMISC;	/* set promiscous flag */
    if (ioctl(s, SIOCSIFFLAGS, &ifr) < 0)
    {
      error("ioctl SET PROMISC");
      finish(-1);
    }
  }

  /* set timeout */
  timeout.tv_sec = SNAPTIME;
  timeout.tv_usec = 0;

    /* Not essential to set the socketopt for timeout, but would be nice */
#ifdef SO_RCVTIMEO
  if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
  {
    if (debug)
      error("setsockopt: SO_RCVTIMEO");
    /* not fatal, continue */
  }
#endif	/* SO_RCVTIMEO */
  /*
   * Beef up the socket buffer to minimise packet losses
   */
  i = RCV_BUFFER_SZ;
  if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)&i, sizeof(i)) < 0) {
    error("setsockopt: set rcvbuf");
    /*      finish(-1); */
  }

  return(s);
}	/* setup_device() */

/*
 * Flush any existing packets in the buffers. Dont think it can be done under
 * Linux.
 */
void
flush_device(fd)
  int fd;
{
  /*  if (ioctl(fd, EIOCFLUSH) < 0) {
      error("ioctl: EIOCFLUSH");
      finish(-1);
      }
      */

}  /* end:   flush_device() */


/*
 * linux_devtype - determine the type of device we're looking at.
 */
int
get_devtype(device, fd)
  char *device;
  int  fd;		/* open file desc, unused */
{
  /*
   * This whole routine is a kludge.  Ultrix does it the
   * right way; see pfilt.c.
	 */

  if (strncmp(device, "eth", 3) == 0)
    return(NDT_EN10MB);

  if (strncmp(device, "sl", 2) == 0)
    return(NDT_SLIP);

  if (strncmp(device, "ppp", 3) == 0)
    return(NDT_PPP);

  fprintf(stderr, "Unknown device type: %s -- assuming ethernet.\n",
	  device);
  return(NDT_EN10MB);
}



/*
 * getpkt - extract a 'packet' from the 'chunk'. Return pointer to
 *	    start of packet, length of pkt and number of pkts dropped.
 *	    Device specific.
 *
 * The 'snoop' interface doesn't seem to read data in chunks, so
 * I guess we are stuck with reading packet-by-packet. Don't really
 * think this code is optimal for this pkt-by-pkt stuff.
 *
 *		-vikas
 */

int
getpkt(fd, ifc_type, ppkt, plen, pwirelen, pdrops)
  int fd;
  int ifc_type;			/* interface type */
  char **ppkt ;
  int *plen ;
  int *pwirelen;
  int *pdrops ;
{
  static int	fromlen;
  static char	*buf;
  struct	sockaddr from;

  /*
   * Allocate a buffer so it's properly aligned for
   * casting to structure types.
   */
  if (buf == NULL)    /* first time around */
  {
    if ((buf = (char *)malloc(RCV_BUFFER_SZ)) == NULL)
    {
      (void) fprintf(stderr, "%s: out of memory\n", prognm);
      (void) perror("getpkt malloc");
      finish(-1);
    }
    fromlen = sizeof(from);		/* needed in recvfrom() */
  }

  /* read one packet from the socket */
  if ((*plen = recvfrom(fd, buf, RCV_BUFFER_SZ, 0, &from, &fromlen)) <= 0)
    return(NOPKT);

  *pwirelen = *plen;
  *ppkt = buf;

  /*
   * To extract the dropped packets, we will have to open /proc/net/dev
   * and then search for the device name. Can be pretty slow, so skip
   * dropped packets for now.
   *
  struct  enet_statistics  stats;
  FILE *fp = fopen("/proc/net/dev", "r");
  len = strlen("eth0");

  while (fgets(buf, 255, fp))
  {
    p = buf;
    while (isspace(*p) && *p)
      p++;
    if ((strncmp(p, "eth0", len) == 0) && (p[len] == ':')) {
      p = strchr(p, ':');
      p++;
      sscanf(p, "%d %d %d %d %d %d %d %d %d %d %d",
	     &stats->rx_packets, &stats->rx_errors, &stats->rx_dropped,
	     &stats->rx_fifo_errors, &stats->rx_frame_errors,
	     &stats->tx_packets, &stats->tx_errors, &stats->tx_dropped,
	     &stats->tx_fifo_errors, &stats->collisions,
	     &stats->tx_carrier_errors);
      fclose(fp);
      return(stats->rx_packets);
    }
  }
   */

  return (ENDOFCHUNK);		/* not OKPKT */

}	/* end getpkt() */

#endif /* USE_LINUX */


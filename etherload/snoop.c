#ifndef lint
static char *RCSid = "$Header$" ;
#endif

#include "os.h"

#ifdef USE_SNOOP	/* Irix */

/*
 * Routines for grabbing packets from the 'snoop' interface for use
 * with etherload. Most details can be extracted from the excellent
 * snoop(7) man page.
 *
 *	Vikas Aggarwal, vikas@navya_.com
 */

/* Adapted from nfswatch 4.1. Original comments follow:
 *
 * snoop.c - routines for snooping on network traffic via the SGI
 *	     provided snoop(7p) network monitoring protocol
 *	     -- works under IRIX 3.2.* and IRIX 3.3.[12]
 *
 * davy/system/nfswatch/RCS/snoop.c,v 4.0 1993/03/01 19:59:00 davy Exp $";
 *
 *
 * *************** End original comments *************
 */

/*
 * $Log$
 * Revision 1.0  2001/07/08 21:47:32  vikas
 * For SNIPS v1.0
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

#include <net/soioctl.h>
#include <net/raw.h>
#include <netinet/if_ether.h>

#include "etherload.h"
#include "externs.h"

#define SNOOP_BUFFER_SIZE	(55 * 1024)	/* for grabbing packets	*/

#define ETHERHDRPAD		RAW_HDRPAD(sizeof(struct ether_header))

char	*os_devices[] = {
  "et0", "et1", "et2", "et3", "et4",
  "ec0", "ec1", "ec2", "ec3", "ec4",
  "fxp0", "fxp1", "fxp2", "fxp3", "fxp4",
  "enp0", "enp1", "enp2", "enp3", "enp4",
  "ipg0", "ipg1", "ipg2", "ipg3", "ipg4",
  0
};

struct etherpacket {
  struct snoopheader	snoop;
  char	pad[ETHERHDRPAD];
  struct ether_header	ether;
  char	data[ETHERMTU];
};


/*
 * setup_device - set up the SNOOP device
 */
int
setup_device(device)
  char *device;
{
  int n, s;
  int i, res;
  u_int chunksz;
  u_long if_flags;
  struct snoopfilter sf;
  struct timeval timeout;
  struct sockaddr_raw sr, sn;

  if (!device  || *device == NULL) {
    error("no device name");
    finish (-1);
  }

  /*
	 * Make the raw socket.
	 */
  if ((s = socket(PF_RAW, SOCK_RAW, RAWPROTO_SNOOP)) < 0) {
    error("snoop: socket");
    finish(-1);
  }

  /*
	 * Set it up for the chosen interface.
	 */
  bzero((char *)&sr, sizeof(sr));
  sr.sr_family = AF_RAW;	
  sr.sr_port = 0;
  strncpy(sr.sr_ifname, device, sizeof(sr.sr_ifname));

	/*
	 * If the bind fails, there's no such device.
	 */
  if (bind(s, &sr, sizeof(sr)) < 0) {
    close(s);
    return(-1);
  }

  /* set timeout */
  timeout.tv_sec = SNAPTIME;
  timeout.tv_usec = 0;
  /*	if (ioctl(fd, SIOCSRTIMEOUT, (caddr_t)&timeout) < 0)
	{
	error("SIOCSRTIMEOUT");
	finish(-1);
	}
	*/
  /*
   * Set up NULL filter - this is not necessary ...
	 */
  bzero((char *)&sf, sizeof(sf));

  if (ioctl(s, SIOCADDSNOOP, &sf) < 0) {
    error("snoop: add filter");
    finish(-1);
  }

  /*
	 * Beef up the socket buffer to minimise packet losses
	 */
  i = SNOOP_BUFFER_SIZE;
  if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)&i, sizeof(i)) < 0) {
    error("snoop: set rcvbuf");
    finish(-1);
  }

  return(s);
}

/*
 * flush_snoop - flush data from the snoop.
 */
void
flush_device(fd)
  int fd;
{
  int on = 1;
  int off = 0;

  /*
   * Off then on should do a flush methinks
   */
  ioctl(fd, SIOCSNOOPING, &off);

  if (ioctl(fd, SIOCSNOOPING, &on) < 0) {
    error("snoop: snoop on");
    finish(-1);
  }
}

/*
 * snoop_devtype - determine the type of device we're looking at.
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

  if (strncmp(device, "et", 2) == 0 || strncmp(device, "ec", 2) == 0 ||
      strncmp(device, "fxp", 3) == 0 || strncmp(device, "enp", 3) == 0)
    return(NDT_EN10MB);
	
  if (strncmp(device, "ipg", 3) == 0)
    return(NDT_FDDI);

  fprintf(stderr, "Unknown device type: %s -- assuming ethernet.\n", device);
  return(NDT_EN10MB);
}



/*
 * getpkt - extract a 'packet' from the 'chunk'. Return pointer to
 *	    start of packet, length of pkt and number of pkts dropped.
 *	    Device specific.
 *
 * The snoop protocol captures link-layer packets which match a bitfield
 * filter and transports them to that filter's socket.  Snoop prepends a
 * header containing packet state, sequence, and a timestamp.  The drain
 * protocol receives input packets with network-layer type codes selected
 * from among encapsulations not implemented by the kernel.
 * Snoop transmits packets with a link-layer header fetched from the
 * beginning of the user's write buffer. 
 *		- From IRIX include file
 *
 * The 'snoop' interface doesn't seem to read data in chunks, so
 * I guess we are stuck with reading packet-by-packet.
 * Hope they did their job right and its optimal to do it pkt-by-pkt...
 *
 *		-vikas
 */

getpkt(fd, ifc_type, ppkt, plen, pwirelen, pdrops)
  int fd;
  int ifc_type;			/* interface type */
  char **ppkt ;
  int *plen ;
  int *pwirelen;
  int *pdrops ;
{
  static int		cc;
  struct etherpacket	ep;
  struct rawstats	rs;

  /*
   * Now read packets from the snoop device.
   */
  if ((cc = read(fd, &ep, sizeof(ep))) <= 0)
    return (NOPKT);
	    
  /*
   * number of dropped packets = drops at interface + drops at socket buffer
   */
  if (ioctl(fd, SIOCRAWSTATS, &rs) == 0)
    pdrops = rs.rs_snoop.ss_ifdrops + rs.rs_snoop.ss_sbdrops;

  /* snoop packets contain the link-layer hdr followed by data */
  *plen = ep.snoop.snoop_packetlen ;	/* this is the wirelen actually */
  /*    *pwirelen = ep.ether.ether_packetlen;	/* extract from the wire */
  *pwirelen = ep.snoop.snoop_packetlen;	/* ep.ether. dont work */
  *ppkt = ep.data;

  return (ENDOFCHUNK);		/* not OKPKT */

}	/* end getpkt() */

#endif /* USE_SNOOP */

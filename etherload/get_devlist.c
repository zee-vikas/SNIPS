/*
 * Extracted from tcpdump.c and nfswatch.c code.
 *
 * 	Vikas Aggarwal, vikas@navya_.com, Feb 1994
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

#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifdef SUNOS5
#include <sys/sockio.h>
#endif

#include <sys/time.h>
#include <net/if.h>
#include <netinet/in.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "util.h"

/* Not all systems have IFF_LOOPBACK */
#ifdef IFF_LOOPBACK
#define ISLOOPBACK(p) ((p)->ifr_flags & IFF_LOOPBACK)
#else
#define ISLOOPBACK(p) (strcmp((p)->ifr_name, "lo0") == 0)
#endif

static char *dupstr();
/*
 * Return the name of a network interface attached to the system, or NULL
 * if none can be found.  The interface must be configured up; the
 * lowest unit number is preferred; loopback is ignored.
 */
char **
get_devlist()
{
  int fd, i;
  extern int debug;
  static char  *sdevlist[16];
  struct ifreq ibuf[16], *ifrp, *ifend, *ifnext;
  struct ifconf ifc;
    
  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    error("socket");
    finish (-1);
  }

  ifc.ifc_len = sizeof ibuf;
  ifc.ifc_buf = (caddr_t)ibuf;

  if (ioctl(fd, SIOCGIFCONF, (char *)&ifc) < 0 ||
      ifc.ifc_len < sizeof(struct ifreq)) {
    error("SIOCGIGCONF");
    finish(-1);
  }

  ifrp = ibuf;
  ifend = (struct ifreq *)((char *)ibuf + ifc.ifc_len);

  for (i = 0; ifrp < ifend; ifrp = ifnext)
  {
    int n ;
    struct ifreq ifr ;	/* temp to copy over the structure */

#if BSD - 0 >= 199006
    n = ifrp->ifr_addr.sa_len + sizeof(ifrp->ifr_name);
    if (n < sizeof(*ifrp))
      ifnext = ifrp + 1;
    else
      ifnext = (struct ifreq *)((char *)ifrp + n);
    if (ifrp->ifr_addr.sa_family != AF_INET)
      continue;
#else
    ifnext = ifrp + 1;
#endif
    /*
     * Need a template to preserve address info that is
     * used below to locate the next entry.  (Otherwise,
     * SIOCGIFFLAGS stomps over it because the requests
     * are returned in a union.)
     */
    strncpy(ifr.ifr_name, ifrp->ifr_name, sizeof(ifr.ifr_name));
    if (debug)
      fprintf(stderr, "get_devlist: probing device %s\n", ifr.ifr_name);
    if (ioctl(fd, SIOCGIFFLAGS, (char *)&ifr) < 0) {
      error("SIOCGIFFLAGS");
      finish(-1);
    } /* endif */

    /* Must be up and not the loopback */
    if ((ifr.ifr_flags & IFF_UP) == 0 || ISLOOPBACK(&ifr))
      continue;
    sdevlist[i++] = (char *)dupstr(ifr.ifr_name);
  }	/* end: for () */

  sdevlist[i] == NULL;	/* terminating NULL */
  (void)close(fd);
  return (sdevlist);
}

/*+ 
 * FUNCTION:
 *      Duplicate a string. Can handle a NULL ptr.
 */
 
static char *dupstr(s)
  register char *s ;
{
  char *t ;
 
  if (s == NULL)
  {
    t = (char *)malloc(1);
    *t = '\0';
  }
  else
  {
    t = (char *)malloc(strlen(s) + 1);
    if (t != NULL)
      (char *)strcpy(t, s);
    else
    {
      perror("dupstr() malloc");
      finish(-1);
    }
  }
  return (t);
}

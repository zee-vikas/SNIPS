/* $Header$ */
/*
 * Network utility functions.
 *
 * 	Vikas Aggarwal  (vikas@navya_.com)
 */

/*
 * $Log$
 * Revision 1.0  2001/07/11 03:30:32  vikas
 * Initial revision
 *
 */

#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/*
 * Instead of inet_addr() which can only handle IP addresses, this handles
 * hostnames as well...
 * Return -1 on error, 1 if okay.
 */
get_inet_address(saddr, host)
  struct sockaddr_in *saddr;		/* must be a malloced structure */
  char *host;
{
  register struct hostent *hp;
  struct sockaddr_in sin, *s;

  if (host == NULL || *host == '\0')
    return (-1);

  if ( (s = saddr) == NULL)
    s = &sin;
  bzero((char *)s, sizeof (struct sockaddr_in));

  s->sin_family = AF_INET;

  s->sin_addr.s_addr = (u_long) inet_addr(host);
  if (s->sin_addr.s_addr == -1 || s->sin_addr.s_addr == 0) {
    if ((hp = gethostbyname(host)) == NULL) {
      fprintf(stderr, "%s is unknown host\n", host);
      return (-1);
    }
#ifdef h_addr		/* in netdb.h */
    bcopy((char *)hp->h_addr, (char *)&(s->sin_addr), hp->h_length);
#else
    bcopy((char *)hp->h_addr_list[0], (char *)&(s->sin_addr), hp->h_length);
#endif
  }
  return 1;	/* OK */
}

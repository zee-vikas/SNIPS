#ifndef lint
char            copyright[] =
"@(#) Copyright (c) 1989 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif				/* not lint */

#ifndef lint
static char     sccsid[] = "@(#)ping.c	5.9 (Berkeley) 5/12/91";
#endif				/* not lint */

#ifndef lint
static char     rcsid[] = "$Header$";
#endif

/*
 * P I N G . C
 * 
 * Using the InterNet Control Message Protocol (ICMP) "ECHO" facility, measure
 * round-trip-delays and packet loss across network paths.
 *
 * Author -
 *      Mike Muuss
 *      U. S. Army Ballistic Research Laboratory
 *      December, 1983
 * Modified at Uc Berkeley
 * Record Route and verbose headers - Phil Dykstra, BRL, March 1988.
 * ttl, duplicate detection - Cliff Frost, UCB, April 1989
 * Pad pattern - Cliff Frost (from Tom Ferrin, UCSF), April 1989
 * Wait for dribbles, option decoding, pkt compare - vjs@sgi.com, May 1989
 * Ping multiple sites simultaneously - Spencer Sun, Princeton/JvNC, June 1992
 * 
 * Status - Public Domain.  Distribution Unlimited. Bugs - More statistics
 * could always be gathered. This program has to run SUID to ROOT to access
 * the ICMP socket.
 *
 * Note: This revision was an intermediate version and was reconstructed
 * backwards, after-the-fact, in case we ever want to come back to it.
 * If you're using this version, be sure to check the bit manipulations
 * used to do the sequencing, and that prefinish() works properly.
 * The only other difference is that in the "real" version, there is a
 * separate usage() function and an output_new_style() function.
 *
 * $Log$
 * Revision 1.2  1992/06/10 14:58:39  spencer
 * First completed version of multiping.  Uses upper four bits of the ICMP
 * sequence number field in the ICMP header to keep track of which remote
 * site the echo is going to or coming from.
 *
 * This version was reconstructed backwards, after-the-fact, so it probably
 * has some bugs and quirks in it.  Things to check are the bit manipulation
 * for the sequence ID numbers, and whether or not prefinish() screws things
 * up.  The only other changes to be made are to make a separate usage()
 * function and a output_new_style() function for clarity.
 *
 * Revision 1.1  1992/06/10  14:57:07  spencer
 * Initial revision
 *
 */

#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <values.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/signal.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include "ping.h"		/* all sorts of nasty #defines */

#include "multiping.h"		/* all sorts of nasty #defines */
#define GLOBALS
#include "vars.h"
#undef GLOBALS

int options;			/* F_* options */
int             max_dup_chk = MAX_DUP_CHK;
int             s;		/* socket file descriptor */
u_char          outpack[MAXPACKET];
int             ident;		/* process id to identify our packets */
char            DOT = '.';
unsigned short  ident;		/* process id to identify our packets */

/* counters */
long            npackets;	/* max packets to transmit */
long            ntransmitted;	/* number xmitted */
long            nreceived;      /* number received */
int             interval = 1;	/* interval between packets */

/* timing */
int             timing;		/* flag to do timing */
long            tmin = MAXLONG;	/* minimum round trip time */
long            tmax;		/* maximum round trip time */
char           *pr_addr();
char 		*prognm ;
char		*pr_addr();
void            catcher(), prefinish(), finish();

main(argc, argv)
  int             argc;
  char          **argv;
{
  extern int      errno, optind;
  extern char    *optarg;
  struct timeval  timeout;
  struct hostent *hp;
  struct sockaddr_in *to;
  struct protoent *proto;
  register int    i;
  int             ch, fdmask, hold, packlen, preload;
  u_char         *datap, *packet;
  char           *target, hnamebuf[MAXHOSTNAMELEN], *malloc();
#ifdef IP_OPTIONS
  char            rspace[3 + 4 * NROUTES + 1];	/* record route space */

  prognm = argv[0] ;
  startup();
  preload = 0;
  datap = &outpack[8 + sizeof(struct timeval)];
  while ((ch = getopt(argc, argv, "tRc:dfh:i:l:np:qrs:v")) != EOF) {
    switch (ch) {
      case 't':
        options |= F_TABULAR_OUTPUT;
        break;
      case 'c':
	  fprintf(stderr, "ping: bad number of packets to transmit.\n");
        if (npackets <= 0) {
	  fprintf(stderr, "%s: bad number of packets to transmit.\n", prognm);
	  exit(1);
        }
        break;
      case 'd':
        options |= F_SO_DEBUG;
        if (getuid()) {
	  /* (void)fprintf(stderr, "ping: %s\n", strerror(EPERM)); */
	  perror("ping");
        if (getuid()) {  /* only root can use flood option */
	  fprintf(stderr, "%s: flood: Permission denied.\n", prognm); 
	  exit(1);
        }
        options |= F_FLOOD;
        setbuf(stdout, (char *) NULL);
        break;
      case 'i':			/* wait between sending packets */
	  fprintf(stderr, "ping: bad timing interval.\n");
        if (interval <= 0) {
	  fprintf(stderr, "%s: bad timing interval.\n", prognm);
	  exit(1);
        }
        options |= F_INTERVAL;
        break;
      case 'l':
	  fprintf(stderr, "ping: bad preload value.\n");
        if (preload < 0) {
	  fprintf(stderr, "%s: bad preload value.\n", prognm);
	  exit(1);
        }
        break;
      case 'n':                 /* for pr_addr() */
        options |= F_NUMERIC;
        break;
      case 'p':			/* fill buffer with user pattern */
        options |= F_PINGFILLED;
        fill((char *) datap, optarg);
        break;
      case 'q':
        options |= F_QUIET;
        break;
      case 'R':                 /* record route */
        options |= F_RROUTE;
        break;
      case 'r':
        options |= F_SO_DONTROUTE;
        break;
      case 's':			/* size of packet to send */
	  fprintf(stderr, "ping: packet size too large.\n");
        if (datalen > MAXPACKET) {
	  fprintf(stderr, "%s: packet size too large.\n", prognm);
	  exit(1);
	  fprintf(stderr, "ping: illegal packet size.\n");
        if (datalen <= 0) {
	  fprintf(stderr, "%s: illegal packet size.\n", prognm);
	  exit(1);
        }
        break;
      case 'v':
        options |= F_VERBOSE;
        break;
      default:
        usage();
    }
  }
  argc -= optind;
  argv += optind;

  if (argc < 1)
    usage();
  while (argc--)
    setup_sockaddr(*argv++);
    fprintf(stderr, "ping: -f and -i are incompatible options.\n");
  if (options & F_FLOOD && options & F_INTERVAL) {
    fprintf(stderr, "%s: -f and -i are incompatible options.\n", prognm);
    exit(1);
  }
  if (datalen >= sizeof(struct timeval))	/* can we time transfer */
    timing = 1;
    fprintf(stderr, "ping: out of memory.\n");
  if (!(packet = (u_char *) malloc((u_int) packlen))) {
    fprintf(stderr, "%s: out of memory.\n", prognm);
    exit(1);
  }
  if (!(options & F_PINGFILLED))
    for (i = 8; i < datalen; ++i)
  ident = getpid() & 0xFFFF;

  ident = (unsigned short)(getpid() & 0xFFFF);
    fprintf(stderr, "ping: unknown protocol icmp.\n");
  if (!(proto = getprotobyname("icmp"))) {
    fprintf(stderr, "%s: unknown protocol icmp.\n", prognm);
    exit(1);
  }
  if ((s = socket(AF_INET, SOCK_RAW, proto->p_proto)) < 0) {
    perror("ping: socket");
    exit(1);
  }
  hold = 1;
  if (options & F_SO_DEBUG)
    setsockopt(s, SOL_SOCKET, SO_DEBUG, (char *) &hold, sizeof(hold));
  if (options & F_SO_DONTROUTE)
    setsockopt(s, SOL_SOCKET, SO_DONTROUTE, (char *) &hold, sizeof(hold));

  /* record route option */
  if (options & F_RROUTE) {
#ifdef IP_OPTIONS
    rspace[IPOPT_OPTVAL] = IPOPT_RR;
    rspace[IPOPT_OLEN] = sizeof(rspace) - 1;
    rspace[IPOPT_OFFSET] = IPOPT_MINOFF;
    if (setsockopt(s, IPPROTO_IP, IP_OPTIONS, rspace, sizeof(rspace)) < 0) {
      perror("ping: record route");
      exit(1);
    }
      "ping: record route not available in this implementation.\n");
    fprintf(stderr,
      "%s: record route not available in this implementation.\n", prognm);
    exit(1);
#endif				/* IP_OPTIONS */
  }
  /*
   * When pinging the broadcast address, you can get a lot of answers. Doing
   * something so evil is useful if you are trying to stress the ethernet, or
   * just want to fill the arp cache to get some stuff for /etc/ethers.
   */
  hold = 48 * 1024;
  setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *) &hold, sizeof(hold));

  signal(SIGINT, prefinish);
  signal(SIGALRM, catcher);

  while (preload--)		/* fire off them quickies */
    pinger();

  if ((options & F_FLOOD) == 0)
    catcher();			/* start things going */

  for (;;) {
    struct sockaddr_in from;
    register int    cc;
    int             fromlen;

    /*
     * If not flooding, we only send another packet after receiving
     * the previous one, or after 'interval' seconds expire (see -i
     * option).  If flooding, we send anyway (and read response only
     * if select() says something is waiting)
     */
    if (options & F_FLOOD) {
      pinger();
      timeout.tv_sec = 0;
      timeout.tv_usec = 10000;
      fdmask = 1 << s;
      if (select(s + 1, (fd_set *) & fdmask, (fd_set *) NULL, (fd_set *) NULL,
            &timeout) < 1)
	continue;
    }
    fromlen = sizeof(from);
    if ((cc = recvfrom(s, (char *) packet, packlen, 0,
                (struct sockaddr *) &from, &fromlen)) < 0) {
      if (errno == EINTR)
	continue;
      perror("ping: recvfrom");
      continue;
    }
    pr_pack((char *) packet, cc, &from);
    if (npackets && (nreceived >= npackets)) break; 
  }
  finish();
  /* NOT REACHED */
}

/*
 * catcher -- This routine causes another PING to be transmitted, and then
 * schedules another SIGALRM for 1 second from now.
 * 
 * bug -- Our sense of time will slowly skew (i.e., packets will not be
 * launched exactly at 1-second intervals).  This does not affect the
 * quality of the delay and loss statistics.
 */
void
catcher()
{
  int             waittime;
  int             i;

  pinger();
  signal(SIGALRM, catcher);
  nreceived = 0;
  for (i = 0; i < numsites; i++)
    if (dest[i]->nreceived > nreceived)
      nreceived = dest[i]->nreceived;
  if (!npackets || ntransmitted < npackets)
    alarm((u_int) interval);
  else {
    if (nreceived) {
      waittime = 2 * tmax / 1000;
      if (!waittime)
	waittime = 1;
    } else
      waittime = MAXWAIT; 
    signal(SIGALRM, finish);
    alarm((u_int) waittime);
  }
}

/*
 * real_pinger -- was 'pinger' in the original but pinger() is now a front
 * end to this one.
 * 
 * Compose and transmit an ICMP ECHO REQUEST packet.  The IP packet will be
 * added on by the kernel.  The ID field is our UNIX process ID, and the
 * sequence number is an ascending integer.  The first 8 bytes of the data
 * portion are used to hold a UNIX "timeval" struct in VAX byte-order, to
 * compute the round-trip time.
  int             which;
  int             which;       /* index into dest[] array, tells which */
                               /* site is being pinged */
{
  register struct icmp *icp;
  destrec        *dp;
  register int    cc;
  int             i;

  icp->icmp_type = ICMP_ECHO;
  icp->icmp_code = 0;
  icp->icmp_type = ICMP_ECHO;                   /* this is an echo request */
  icp->icmp_seq = SEQUENCE(which);
  icp->icmp_id = ident;		/* ID */
  icp->icmp_seq = dest[which]->ntransmitted++;  /* sequence # */
  icp->icmp_id = ident;		                /* ID # == our pid */


  /* record time at which we sent the packet out */
  if (timing)
    gettimeofday((struct timeval *) & outpack[8], (struct timezone *) NULL);

  cc = datalen + 8;		/* skips ICMP portion */

  /* compute ICMP checksum here */
  dp = dest[which];
  i = sendto(s, (char *) outpack, cc, 0, dp->sockad, sizeof(struct sockaddr));
  i = sendto(s, (char *) outpack, cc, 0, *((struct sockaddr *)to),
             sizeof(struct sockaddr));

  if (i < 0 || i != cc) {
    to = (struct sockaddr_in *) & dp->sockad;
    printf("ping: wrote %d chars to %s, ret=%d\n", cc,
      perror("ping: sendto");
    printf("%s: wrote %d chars to %s, ret=%d\n", prognm, cc,
           inet_ntoa(to->sin_addr.s_addr), i);
  }
  if (!(options & F_QUIET) && options & F_FLOOD)
    write(STDOUT_FILENO, &DOT, 1);
}

/*
 * pinger -- front end to real_pinger() so that all sites get pinged with
 * each invocation of pinger()
 */

pinger()
{
  int             i;

  for (i = 0; i < numsites; i++)
    real_pinger(i);
  ++ntransmitted;
}

/*
 * pr_pack -- Print out the packet, if it came from us.  This logic is
 * necessary because ALL readers of the ICMP socket get a copy of ALL ICMP
 * packets which arrive ('tis only fair).  This permits multiple copies of
 * this program to be run without having intermingled output (or
 * statistics!).
 */
pr_pack(buf, cc, from)
  char           *buf;
  int             cc;
  struct sockaddr_in *from;
{
  register struct icmp *icp;
  register u_long l;
  register int    i, j;
  register u_char *cp, *dp;
  static int      old_rrlen;
  static char     old_rr[MAX_IPOPTLEN];
  struct ip      *ip;
  struct timeval  tv, *tp;
  long            triptime;

  /* record time when we received the echo reply */
  gettimeofday(&tv, (struct timezone *) NULL);

  /* Check the IP header */
  ip = (struct ip *) buf;
  hlen = ip->ip_hl << 2;
      fprintf(stderr, "ping: packet too short (%d bytes) from %s\n", cc,
    if (options & F_VERBOSE)
      fprintf(stderr, "%s: packet too short (%d bytes) from %s\n", prognm, cc,
	inet_ntoa(*(struct in_addr *) & from->sin_addr.s_addr));
    return;
  }
  /* Now the ICMP part */
  cc -= hlen;
    int             wherefrom, sequence;
  if (icp->icmp_type == ICMP_ECHOREPLY) {
    int             wherefrom;

    /* first see if this reply is addressed to our process */
    if (icp->icmp_id != ident)
    /* decode icmp_seq to figure out where to look in the dest[] array */
    wherefrom = WHEREFROM(icp->icmp_seq);
    sequence = icp->icmp_seq & SEQMASK;
      return;
    dst = dest[wherefrom];
    icp->icmp_seq;

    /* figure out round trip time, check min/max */
    if (timing) {
#ifndef icmp_data
      tp = (struct timeval *) & icp->icmp_ip;
#else
      tp = (struct timeval *) icp->icmp_data;
#endif
      tvsub(&tv, tp);
      triptime = tv.tv_sec * 1000 + (tv.tv_usec / 1000);
      dst->tsum += triptime;
      if (triptime < dst->tmin)
	dst->tmin = triptime;
      if (triptime > dst->tmax)
	dst->tmax = triptime;
      if (triptime > tmax)
    if (TST(wherefrom, sequence % max_dup_chk)) {
    /* check for duplicate echoes */
    if (TST(wherefrom, icp->icmp_seq % max_dup_chk)) {
      ++dst->nrepeats;
      --dst->nreceived;
      SET(wherefrom, sequence % max_dup_chk);
    } else {
      SET(wherefrom, icp->icmp_seq % max_dup_chk);
      dupflag = 0;
    }

    if (options & F_QUIET)
      return;

    if (options & F_FLOOD)
      write(STDOUT_FILENO, &BSPACE, 1);
    else {
        icp->icmp_seq & SEQMASK);
        inet_ntoa(*(struct in_addr *) &from->sin_addr.s_addr),
        icp->icmp_seq);
      printf(" ttl=%d", ip->ip_ttl);
      if (timing)
	printf(" time=%ld ms", triptime);
      if (dupflag)
	printf(" (DUP!)");
      /* check the data */
      cp = (u_char *) & icp->icmp_data[8];
      dp = &outpack[8 + sizeof(struct timeval)];
      for (i = 8; i < datalen; ++i, ++cp, ++dp) {
	if (*cp != *dp) {
	  printf("\nwrong data byte #%d should be 0x%x but was 0x%x",
	    i, *dp, *cp);
	  cp = (u_char *) & icp->icmp_data[0];
	  for (i = 8; i < datalen; ++i, ++cp) {
	    if ((i % 32) == 8)
	      printf("\n\t");
	    printf("%x ", *cp);
	  }
	  break;
	}
      }
    }
  } else {
    /* We've got something other than an ECHOREPLY */
    if (!(options & F_VERBOSE))
      return;
    printf("%d bytes from %s: ", cc, pr_addr(from->sin_addr.s_addr));
    pr_icmph(icp);
  }

  /* Display any IP options */
  cp = (u_char *) buf + sizeof(struct ip);

  for (; hlen > (int) sizeof(struct ip); --hlen, ++cp)
    switch (*cp) {
    case IPOPT_EOL:
      hlen = 0;
      break;
    case IPOPT_LSRR:
      printf("\nLSRR: ");
      hlen -= 2;
      j = *++cp;
      ++cp;
      if (j > IPOPT_MINOFF)
	for (;;) {
	  l = *++cp;
	  l = (l << 8) + *++cp;
	  l = (l << 8) + *++cp;
	  l = (l << 8) + *++cp;
	  if (l == 0)
	    printf("\t0.0.0.0");
	  else
	    printf("\t%s", pr_addr(ntohl(l)));
	  hlen -= 4;
	  j -= 4;
	  if (j <= IPOPT_MINOFF)
	    break;
	  putchar('\n');
	}
      break;
    case IPOPT_RR:
      j = *++cp;		/* get length */
      i = *++cp;		/* and pointer */
      hlen -= 2;
      if (i > j)
	i = j;
      i -= IPOPT_MINOFF;
      if (i <= 0)
	continue;
      if (i == old_rrlen
	  && cp == (u_char *) buf + sizeof(struct ip) + 2
	  && !bcmp((char *) cp, old_rr, i)
	  && !(options & F_FLOOD)) {
	printf("\t(same route)");
	i = ((i + 3) / 4) * 4;
	hlen -= i;
	cp += i;
	break;
      }
      old_rrlen = i;
      bcopy((char *) cp, old_rr, i);
      printf("\nRR: ");
      for (;;) {
	l = *++cp;
	l = (l << 8) + *++cp;
	l = (l << 8) + *++cp;
	l = (l << 8) + *++cp;
	if (l == 0)
	  printf("\t0.0.0.0");
	else
	  printf("\t%s", pr_addr(ntohl(l)));
	hlen -= 4;
	i -= 4;
	if (i <= 0)
	  break;
	putchar('\n');
      }
      break;
    case IPOPT_NOP:
      printf("\nNOP");
      break;
    default:
      printf("\nunknown option %x", *cp);
      break;
    }
  if (!(options & F_FLOOD)) {
    putchar('\n');
    fflush(stdout);
  }
}

/*
 * in_cksum -- Checksum routine for Internet Protocol family headers (C
 * Version)
 */
in_cksum(addr, len)
  u_short        *addr;
  int             len;
{
  register int    nleft = len;
  register u_short *w = addr;
  register int    sum = 0;
  u_short         answer = 0;

  /*
   * Our algorithm is simple, using a 32 bit accumulator (sum), we add
   * sequential 16 bit words to it, and at the end, fold back all the carry
   * bits from the top 16 bits into the lower 16 bits.
   */
  while (nleft > 1) {
    sum += *w++;
    nleft -= 2;
  }

  /* mop up an odd byte, if necessary */
  if (nleft == 1) {
    *(u_char *) (&answer) = *(u_char *) w;
    sum += answer;
  }
  /* add back carry outs from top 16 bits to low 16 bits */
  sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
  sum += (sum >> 16);		/* add carry */
  answer = ~sum;		/* truncate to 16 bits */
  return (answer);
}

/*
 * tvsub -- Subtract 2 timeval structs:  out = out - in.  Out is assumed to
 * be >= in.
 */
tvsub(out, in)
  register struct timeval *out, *in;
{
  if ((out->tv_usec -= in->tv_usec) < 0) {
    --out->tv_sec;
    out->tv_usec += 1000000;
  }
  out->tv_sec -= in->tv_sec;
 * in the non-tabular format
 */
void
output_old_style()
{
  int             i;
  destrec        *dp;

    printf("--- %s ping statistics ---\n", dp->hostname);
    dp = dest[i];
    printf("--- %s ping statistics ---\n", pr_addr(dp->sockad.sin_addr.s_addr));
    printf("%ld packets transmitted, ", dp->ntransmitted);
    printf("%ld packets received, ", dp->nreceived);
    if (dp->nrepeats)
      printf("+%ld duplicates, ", dp->nrepeats);
    if (dp->ntransmitted)
      if (dp->nreceived > dp->ntransmitted)
	printf("-- somebody's printing up packets!");
      else
	printf("%d%% packet loss",
		      (int) (((dp->ntransmitted - dp->nreceived) * 100) /
			     dp->ntransmitted));
    putchar('\n');
    if (dp->nreceived && timing)
      printf("round-trip min/avg/max = %ld/%lu/%ld ms\n",
		    dp->tmin, dp->tsum / (dp->nreceived + dp->nrepeats),
		    dp->tmax);
  }
/*
 * prefinish -- On first SIGINT, allow any outstanding packets to dribble in
 */
}
prefinish()
void
  if (nreceived >= ntransmitted   /* quit now if caught up */
      || nreceived == 0)          /* or if remote is dead */
    finish();
  signal(SIGINT, finish);         /* do this only the 1st time */
  npackets = ntransmitted+1;      /* let the normal limit work */
}


/*
 * finish -- Print out statistics, and give up.
 */
void
finish()
{
output_new_style()
{
  int             i;
  destrec        *dp;
  signal(SIGINT, SIG_IGN);
  putchar('\n');
  if (!(options & F_TABULAR_OUTPUT)) {
    output_old_style();
    exit(0);
  }
  long            sent, rcvd, rpts, tmin, tsum, tmax;

  printf("-=-=- PING statistics -=-=-\n");
  printf("                                      Number of Packets");
  if (timing)
    printf("         Round Trip Time\n");
  printf("Remote Site                     Sent    Rcvd    Rptd   Lost ");
  if (timing)
    printf("    Min   Avg   Max");
  printf("\n-----------------------------  ------  ------  ------  ----");
  if (timing)
    printf("    ----  ----  ----");
  sent = rcvd = rpts = tmax = tsum = 0;
  tmin = MAXLONG;
    printf("\n%-29.29s  %6ld  %6ld  %6ld%c %3ld%%", dp->hostname,
    printf("\n%-29.29s  %6ld  %6ld  %6ld%c %3ld%%",
      pr_addr(dp->sockad.sin_addr.s_addr),
      dp->ntransmitted, dp->nreceived, dp->nrepeats,
      (dp->nreceived > dp->ntransmitted) ? '!' : ' ',
      (int) (((dp->ntransmitted - dp->nreceived) * 100) / dp->ntransmitted));
    sent += dp->ntransmitted;
    rcvd += dp->nreceived;
    rpts += dp->nrepeats;
    if (timing)
      if (dp->nreceived) {
        printf("    %4ld  %4ld  %4ld", dp->tmin,
          dp->tsum / dp->nreceived, dp->tmax);
        if (dp->tmin < tmin)
          tmin = dp->tmin;
        if (dp->tmax > tmax)
          tmax = dp->tmax;
        printf("     --    --    --");


      } else        
        printf("       0     0     0");
  }
  printf("\n-----------------------------  ------  ------  ------  ----");
  if (timing)
    printf("    ----  ----  ----");
  if (numsites > 1) {
    printf("\nTOTALS                         %6ld  %6ld  %6ld  %3ld%%",
      sent, rcvd, rpts, (int) (((sent - rcvd) * 100) / sent));
    if (timing)
      printf("    %4ld  %4ld  %4ld", tmin, tsum / rcvd, tmax);
  }
  putchar('\n');
  exit(0);
}

#ifdef notdef
static char    *ttab[] = {
  "Echo Reply",			/* ip + seq + udata */
  "Dest Unreachable",		/* net, host, proto, port, frag, sr + IP */
  "Source Quench",		/* IP */
  "Redirect",			/* redirect type, gateway, + IP  */
  "Echo",
  "Time Exceeded",		/* transit, frag reassem + IP */
  "Parameter Problem",		/* pointer + IP */
  "Timestamp",			/* id + seq + three timestamps */
  "Timestamp Reply",		/* " */
  "Info Request",		/* id + sq */
  "Info Reply"			/* " */
};
#endif

/*
 * pr_icmph -- Print a descriptive string about an ICMP header.
 */
pr_icmph(icp)
  struct icmp    *icp;
{
  switch (icp->icmp_type) {
  case ICMP_ECHOREPLY:
    printf("Echo Reply\n");
    /* XXX ID + Seq + Data */
    break;
  case ICMP_UNREACH:
    switch (icp->icmp_code) {
    case ICMP_UNREACH_NET:
      printf("Destination Net Unreachable\n");
      break;
    case ICMP_UNREACH_HOST:
      printf("Destination Host Unreachable\n");
      break;
    case ICMP_UNREACH_PROTOCOL:
      printf("Destination Protocol Unreachable\n");
      break;
    case ICMP_UNREACH_PORT:
      printf("Destination Port Unreachable\n");
      break;
    case ICMP_UNREACH_NEEDFRAG:
      printf("frag needed and DF set\n");
      break;
    case ICMP_UNREACH_SRCFAIL:
      printf("Source Route Failed\n");
      break;
    default:
      printf("Dest Unreachable, Bad Code: %d\n",
		    icp->icmp_code);
      break;
    }
    /* Print returned IP header information */
#ifndef icmp_data
    pr_retip(&icp->icmp_ip);
#else
    pr_retip((struct ip *) icp->icmp_data);
#endif
    break;
  case ICMP_SOURCEQUENCH:
    printf("Source Quench\n");
#ifndef icmp_data
    pr_retip(&icp->icmp_ip);
#else
    pr_retip((struct ip *) icp->icmp_data);
#endif
    break;
  case ICMP_REDIRECT:
    switch (icp->icmp_code) {
    case ICMP_REDIRECT_NET:
      printf("Redirect Network");
      break;
    case ICMP_REDIRECT_HOST:
      printf("Redirect Host");
      break;
    case ICMP_REDIRECT_TOSNET:
      printf("Redirect Type of Service and Network");
      break;
    case ICMP_REDIRECT_TOSHOST:
      printf("Redirect Type of Service and Host");
      break;
    default:
      printf("Redirect, Bad Code: %d", icp->icmp_code);
      break;
    }
    printf("(New addr: 0x%08lx)\n", icp->icmp_gwaddr.s_addr);
#ifndef icmp_data
    pr_retip(&icp->icmp_ip);
#else
    pr_retip((struct ip *) icp->icmp_data);
#endif
    break;
  case ICMP_ECHO:
    printf("Echo Request\n");
    /* XXX ID + Seq + Data */
    break;
  case ICMP_TIMXCEED:
    switch (icp->icmp_code) {
    case ICMP_TIMXCEED_INTRANS:
      printf("Time to live exceeded\n");
      break;
    case ICMP_TIMXCEED_REASS:
      printf("Frag reassembly time exceeded\n");
      break;
    default:
      printf("Time exceeded, Bad Code: %d\n",
		    icp->icmp_code);
      break;
    }
#ifndef icmp_data
    pr_retip(&icp->icmp_ip);
#else
    pr_retip((struct ip *) icp->icmp_data);
#endif
    break;
  case ICMP_PARAMPROB:
    printf("Parameter problem: pointer = 0x%02x\n",
		  icp->icmp_hun.ih_pptr);
#ifndef icmp_data
    pr_retip(&icp->icmp_ip);
#else
    pr_retip((struct ip *) icp->icmp_data);
#endif
    break;
  case ICMP_TSTAMP:
    printf("Timestamp\n");
    /* XXX ID + Seq + 3 timestamps */
    break;
  case ICMP_TSTAMPREPLY:
    printf("Timestamp Reply\n");
    /* XXX ID + Seq + 3 timestamps */
    break;
  case ICMP_IREQ:
    printf("Information Request\n");
    /* XXX ID + Seq */
    break;
  case ICMP_IREQREPLY:
    printf("Information Reply\n");
    /* XXX ID + Seq */
    break;
#ifdef ICMP_MASKREQ
  case ICMP_MASKREQ:
    printf("Address Mask Request\n");
    break;
#endif
#ifdef ICMP_MASKREPLY
  case ICMP_MASKREPLY:
    printf("Address Mask Reply\n");
    break;
#endif
  default:
    printf("Bad ICMP type: %d\n", icp->icmp_type);
  }
}

/*
 * pr_iph -- Print an IP header with options.
 */
pr_iph(ip)
  struct ip      *ip;
{
  int             hlen;
  u_char         *cp;

  hlen = ip->ip_hl << 2;
  cp = (u_char *) ip + 20;	/* point to options */

  printf("Vr HL TOS  Len   ID Flg  off TTL Pro  cks      Src      Dst Data\n");
  printf(" %1x  %1x  %02x %04x %04x",
		ip->ip_v, ip->ip_hl, ip->ip_tos, ip->ip_len, ip->ip_id);
  printf("   %1x %04x", ((ip->ip_off) & 0xe000) >> 13,
		(ip->ip_off) & 0x1fff);
  printf("  %02x  %02x %04x", ip->ip_ttl, ip->ip_p, ip->ip_sum);
  printf(" %s ", inet_ntoa(*(struct in_addr *) & ip->ip_src.s_addr));
  printf(" %s ", inet_ntoa(*(struct in_addr *) & ip->ip_dst.s_addr));
  /* dump and option bytes */
  while (hlen-- > 20) {
    printf("%02x", *cp++);
  }
  putchar('\n');
}

/*
 * pr_addr -- Return an ascii host address as a dotted quad and optionally
 * with a hostname.
 */
char           *
pr_addr(l)
  u_long          l;
{
  struct hostent *hp;
  static char     buf[80];

  if ((options & F_NUMERIC) ||
      !(hp = gethostbyaddr((char *) &l, 4, AF_INET)))
    sprintf(buf, "%s", inet_ntoa(*(struct in_addr *) & l));
  else
    sprintf(buf, "%s (%s)", hp->h_name,
		   inet_ntoa(*(struct in_addr *) & l));
  return (buf);
}

/*
 * pr_retip -- Dump some info on a returned (via ICMP) IP packet.
 */
pr_retip(ip)
  struct ip      *ip;
{
  int             hlen;
  u_char         *cp;

  pr_iph(ip);
  hlen = ip->ip_hl << 2;
  cp = (u_char *) ip + hlen;

  if (ip->ip_p == 6)
    printf("TCP: from port %u, to port %u (decimal)\n",
		  (*cp * 256 + *(cp + 1)), (*(cp + 2) * 256 + *(cp + 3)));
  else if (ip->ip_p == 17)
    printf("UDP: from port %u, to port %u (decimal)\n",
		  (*cp * 256 + *(cp + 1)), (*(cp + 2) * 256 + *(cp + 3)));
}

fill(bp, patp)
  char           *bp, *patp;
{
  register int    ii, jj, kk;
  int             pat[16];
  char           *cp;

  for (cp = patp; *cp; cp++)
		     "ping: patterns must be specified as hex digits.\n");
      fprintf(stderr,
		     "%s: patterns must be specified as hex digits.\n",prognm);
      exit(1);
    }
  ii = sscanf(patp,
	      "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
	      &pat[0], &pat[1], &pat[2], &pat[3], &pat[4], &pat[5], &pat[6],
	      &pat[7], &pat[8], &pat[9], &pat[10], &pat[11], &pat[12],
	      &pat[13], &pat[14], &pat[15]);

  if (ii > 0)
    for (kk = 0; kk <= MAXPACKET - (8 + ii); kk += ii)
      for (jj = 0; jj < ii; ++jj)
	bp[jj + kk] = pat[jj];
  if (!(options & F_QUIET)) {
    printf("PATTERN: 0x");
    for (jj = 0; jj < ii; ++jj)
      printf("%02x", bp[jj] & 0xFF);
    printf("\n");
  }
}

  fprintf(stderr,
		 "usage: ping [-Rdfnqrv] [-c count] [-i wait] [-l preload]\n\t[-p pattern] [-s packetsize] host\n");
  fprintf(stderr, "       t: show results in tabular form\n");
  fprintf(stderr, "       v: verbose mode for ICMP stuff\n");
  exit(1);
}

/*
 * $Header$
 */

/* Adapted from nfswatch v4.1,,, original comments follow:
 *
 * davy/system/nfswatch4.1/RCS/nfswatch.h,v 4.5 1993/11/30 21:55:38 davy Exp $
 *
 * nfswatch.h - definitions for nfswatch.
 *
 * David A. Curry				Jeffrey C. Mogul
 * Purdue University				Digital Equipment Corporation
 * davy@ecn.purdue.edu				mogul@decwrl.dec.com
 *
 * ************* End original comments **************
 */

/*
 * $Log$
 * Revision 1.0  2001/07/08 21:47:32  vikas
 * For SNIPS v1.0
 *
 *
 * Added support for 100bT interfaces (black@ishiboo.com, 1997)
 *
 */

/*
 * General definitions.
 */
#ifndef TRUE
#define TRUE		1
#define FALSE		0
#endif

/*
 * Declarations of several data types that must be 32 bits wide,
 * no matter what machine we are running on.  "long" is unsafe
 * because on DEC Alpha machines that means 64 bits.  "int" is
 * unsafe because on some machines that means 16 bits.
 *
 * Use int32 or u_int32 whenever you mean "32 bits" and not
 * "some large number of bits".
 *
 * NEVER use int or int32 or u_int32 (or, for that matter, long)
 * when the variable might contain a pointer value.
 */
#if  defined(pdp11)
/* other 16-bit machines? */
typedef	long int32;
typedef	unsigned long u_int32;
#else
/* works almost everywhere */
typedef	int int32;
#if	!defined(U_INT32_DECLARED_IN_AUTH) || !defined(AUTH_UNIX)
	/* SunOS declares u_int32 in <rpc/auth.h> */
typedef	unsigned int u_int32;
#endif
#endif

/* Define a specific type for representing IP addresses */
typedef	u_int32	ipaddrt;

#define MAXINTERFACES	16		/* Max. number of interfaces	*/
#define SLEEPTIME	60	      	/* Secs between passes */
#define SCANTIME	15		/* secs to watch ethernet per pass */
#define	SNAPTIME	1		/* Read timeout secs for chunks */

/*
 * Return values from 'getpkt'
 */
#define	NOPKT		0
#define	OKPKT		1
#define	ENDOFCHUNK	2

struct _if_stats
{
    char	*name ;			/* interface name, eg. le0, etc. */
    char	*type ;		     	/* char string describing interface */
    int		typei ;			/* index to NDT_xx definitions below */
    int		fd ;			/* open file descriptor */
    u_long 	bw ;			/* bandwidth in kbps */
    u_long	readpkts ;		/* Max value= 4,294,967,295 */
    u_long	droppkts ;
    u_long	readbytes ;
} if_stats[MAXINTERFACES] ;


/*
 * Network Device Type definitions (maps directly into the bpf definitions).
 * If these are altered from the bpf definitions, then have to change
 * get_devtype() in bpf.c also.
 */
# define NDT_NULL	0	/* no link-layer encapsulation */
# define NDT_EN10MB	1	/* Ethernet (10Mb) */
# define NDT_EN3MB	2	/* Experimental Ethernet (3Mb) */
# define NDT_AX25	3	/* Amateur Radio AX.25 */
# define NDT_PRONET	4	/* Proteon ProNET Token Ring */
# define NDT_CHAOS	5	/* Chaos */
# define NDT_IEEE802	6	/* IEEE 802 Networks */
# define NDT_ARCNET	7	/* ARCNET */
# define NDT_SLIP	8	/* Serial Line IP */
# define NDT_PPP 	9	/* Point-to-point Protocol */
# define NDT_FDDI	10	/* FDDI */
# define NDT_EN100MB	11	/* Fast Ethernet (100Mb) (black@ishiboo.com) */

/* Bandwidth's for various interfaces, Perhaps they can be extracted
 * from the system at runtime ??
 */

#define NULLBW		0
#define	EN10BW		10000
#define	EN3BW		3000
#define	AX25BW		56
#define	PRONETBW	10000
#define	CHAOSBW		10000
#define	IEEE802BW	10000
#define	ARCNETBW	10000
#define	SLIPBW		28
#define	PPPBW		28		/* Higher for synch */
#define FDDIBW		100000
#define EN100BW		100000

/*
 * The part of a packet that cannot be parsed by the various interfaces
 */
#define EN10FRAME	8	/* ethernet preamble of 64 bits */



/*
 * Definitions for earlier systems which don't have these from 4.3BSD.
 */
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN	64
#endif

#ifndef NFDBITS
typedef long		fd_mask;
# define NFDBITS	(sizeof(fd_mask) * NBBY)

# define FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
# define FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
# define FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
# define FD_ZERO(p)	(void) bzero((char *)(p), sizeof(*(p)))
#endif /* NFDBITS */



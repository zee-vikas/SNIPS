/*
 * $Header$
 */

/* Adapted from nfswatch-4.1. Original comments follow:
 *
 * /mogul/code/nfswatch/nfswatch4.1beta/RCS/os.h,v 4.4 1993/10/13 01:13:25 mogul Exp $
 *
 * os.h	- operating system definitions.
 *
 * David A. Curry				Jeffrey C. Mogul
 * Purdue University				Digital Equipment Corporation
 * Engineering Computer Network			Western Research Laboratory
 * 1285 Electrical Engineering Building		250 University Avenue
 * West Lafayette, IN 47907-1285		Palo Alto, CA 94301
 * davy@ecn.purdue.edu				mogul@decwrl.dec.com
 *
 * $Log:
 *
 * Revision 4.4  1993/10/13  01:13:25  mogul
 * IRIX40 fix
 *
 * ....
 * Revision 1.1  1993/01/13  20:18:17  davy
 * Initial revision
 *
 * ************** END ORIGINAL COMMENTS **********************
 */

/*
 * $Log$
 * Revision 1.0  2001/07/12 04:45:40  vikas
 * Initial revision
 *
 * Revision 1.0  2001/07/08 21:47:32  vikas
 * For SNIPS v1.0
 *
 *
 */

#ifdef IRIX51
# define IRIX40
# define _BSD_SIGNALS	/* will use BSD style interface */
#endif

#ifdef IRIX40
# ifndef USE_SNOOP
#  define USE_SNOOP	1
# endif
# ifndef _BSD_SIGNALS
#  define signal	sigset	/* dont do if using BSD_SIGNALS */
# endif
# define U_INT32_DECLARED_IN_AUTH	1
#endif

#ifdef SUNOS4
# ifndef USE_NIT
#  define USE_NIT	1
# endif
# define U_INT32_DECLARED_IN_AUTH	1
#endif

#ifdef SUNOS5
# ifndef SVR4
#  define SVR4		1
# endif
# ifndef USE_DLPI
#  define USE_DLPI	1
# endif
# define U_INT32_DECLARED_IN_AUTH	1
#endif

/* Apparently SCO and UnixWare use DLPI */
#ifdef SVR4
# ifndef USE_DLPI
#  define USE_DLPI	1
# endif
# define index		strchr
# define rindex		strrchr
# define signal		sigset
# define bzero(b,n)	memset(b,0,n)
# define bcmp(a,b,n)	memcmp(a,b,n)
# define bcopy(a,b,n)	memcpy(b,a,n)
#endif

#ifdef ULTRIX
# ifndef USE_PFILT
#  define USE_PFILT	1
# endif
#endif

#ifdef OSF1
# ifndef USE_PFILT
#  define USE_PFILT	1
# endif
#endif

#if defined(__bsdi__) || defined(BSDI)
# define USE_BPF
#endif

#if defined(FREEBSD) || defined(__FreeBSD__)
# define USE_BPF
#endif

/* AIX v3 uses DLPI, however has not been tested. Will need to run
 * strload prior to configure to ensure STREAMS support is enabled.
 */
#ifdef AIX		/* only BPF support for AIX 4.x */
# define USE_BPF
#endif


#if defined(LINUX) || defined(LINUX1) || defined(LINUX2)
# define USE_LINUX
# include <endian.h>
#endif


#ifdef STANDALONE
# ifdef SNIPS
#  undef SNIPS
# endif
#endif


/*+	$Header$
 *
 */

/*+
 * Pings devices and return's an array of 'return values' (returns an array
 * so that one can ping a batch of 'batchsize' devices at a time).
 *
 * BUGS:
 *	Uses 'popen()' so if you try to ping too many devices at a time,
 * the device list gets truncated because of the shell command size limits
 * in Unix (typically _SC_ARGS_MAX defined in the kernel).
 *
 *	-Vikas Aggarwal, vikas@navya_.com
 */

/*
 * $Log$
 * Revision 1.0  2001/07/08 22:56:07  vikas
 * For SNIPS
 *
 *
 * Not using 'sed' to parse output of multiping command anymore. Patch
 * sent in by richard@amega.demon.co.uk.
 */

/* Copyright 2001, Netplex Technologies, Inc. */

#include "snips.h"
#include "pingmon.h"

#include <sys/file.h>

static int *rvalues  = NULL;
static char pingcmd[BUFSIZ * 4];	/* the ping command used */
static char line[BUFSIZ];		/* output of command */

char *strstr();

/*
 * Given an array of devices to monitorm, Return's array of integer values
 */
int *pingmon (devices)
  char *devices;
{
  int rc;

  if (strstr(ping, "/") && access(ping, F_OK | X_OK) != 0)
  {
    perror (ping);		/* access() sets errno */
    return (NULL);
  }

  if (rvalues == NULL)		/* first time around */
  {
    if (mode != RPCPING_MODE)
      rvalues = (int *)malloc(batchsize * sizeof (int) * 2);
    else
      rvalues = (int *)malloc(batchsize * sizeof (int));
  }

  switch (mode)
  {
  case IPMULTIPING_MODE:
    rc = multipingmon(devices);
    break;
  case  OSIPING_MODE:
    rc = osipingmon(devices);
    break;
  case RPCPING_MODE:
    rc = rpcpingmon(devices);
    break;
  default:
    rc = ippingmon(devices);
    break;
  }	/* switch() */

  if (rc < 0)
    return (NULL);	/* fatal error */
  else
    return (rvalues);
}		/* pingmon() */

rpcpingmon(pdevices)
  char **pdevices;
{
  int  value = 0;		/* assume down */
  FILE *pcmd;

  sprintf(pingcmd, "%s -t %d %s", ping, RPCTIMEOUT, pdevices[0]);

  if ((pcmd = popen(pingcmd, "r")) == NULL)	/* open up the pipe */
  {
    perror("rpcpingmon popen()");
    return(-1);
  }

  if (fgets(line, sizeof(line), pcmd) != NULL)
  {
    if (strstr(line, "is running"))	/* output from rpcping */
      value = 1;
  }
  if (pclose(pcmd) < 0)		      /* close the pipe */
    perror("rpcpingmon pclose()");
  rvalues[0] = value;
}

multipingmon(pdevices)
  char **pdevices;
{
  int i, j, ndevices;
  FILE *pcmd;

  /*
   * multiping -q (quiet) -c <pkt count> -s <pkt size> -t (tabular) devices
   *
   * For tabular output, data is separated by a line of dashes.
   * The output from multiping looks something like this:
   *
   *   PING 128.121.50.145 (128.121.50.145): 56 data bytes
   *   PING 128.121.50.147 (128.121.50.147): 56 data bytes
   *   PING 128.121.50.140 (128.121.50.140): 56 data bytes
   *   
   *   -=-=- PING statistics -=-=-
   *                                         Number of Packets
   *   Remote Device                     Sent    Rcvd    Rptd   Lost
   *   -----------------------------  ------  ------  ------  ----
   *   128.121.50.145                     10      10       0    0%
   *   128.121.50.147                     10      10       0    0%
   *   128.121.50.140                     10      10       0    0%
   *   -----------------------------  ------  ------  ------  ----
   *   TOTALS                             30      30       0    0%
   *
   * (I've cut off the right part of the screen to make it fit)
   *
   * The device name is printed as %30.30 (30 spaces)
   */

  sprintf(pingcmd, "%s -qtn -c %d -s %d ", ping, npackets, pktsize);
  for (ndevices = 0; pdevices[ndevices] && *(pdevices[ndevices]); ++ndevices)
    strcat(strcat(pingcmd, " "), pdevices[ndevices]);
  /* strcat(pingcmd, " | sed '1,/^-----/d'");	/* avoid using sed */

  if (debug > 1)
    fprintf(stderr, "pingcmd = %s\n", pingcmd);
  if ((pcmd = popen(pingcmd, "r")) == NULL)	/* open up the pipe */
  {
    perror("ippingmon popen()");
    return(-1);
  }

  /*
   * 'multiping' produces output lines in the order in which they
   * appeared in the command line.
   */

  while (fgets(line, sizeof(line), pcmd) != NULL)
    if (strncmp(line, "-----", 5) == 0)	/* skip all lines till separator */
      break;
  if (line[0] != '-')
  {
    fprintf(stderr, "ippingmon error- command %s returned NULL\n", pingcmd);
    return (-1);
  }

  /*
   * The output after 30 characters has pkts sent and recieved
   * Can't use '%*s' to skip over the devicename since we can have
   * the address after the devicename and we end up with two words.
   * The '30' size is defined in the multiping program (hack).
   */
  i = 0;
  for (i=0, j=0; i < ndevices; ++i )
  {
    if (fgets(line, sizeof(line), pcmd) == NULL)
      break;

    if (strncmp(line, "-----", 5) == 0)	/* end of table */
      break;
    /* get pkts recieved and avg round-trip time */
    sscanf(&line[30], "%*d %d %*d %*s %*d %d", &(rvalues[j]), &(rvalues[j+1]));
    j += 2;
  }
  if (pclose(pcmd) < 0)		      /* close the pipe */
    perror("multipingmon pclose()");

}	/* multipingmon() */

/*
 * Using the system /bin/ping program
 */
ippingmon(pdevices)
  char **pdevices;
{
  int sent, recv = 0;
  float minrtt, avgrtt, maxrtt;
  FILE *pcmd;

  /*
   * A typical (standard) 'ping | tail -2' output looks like this:
   *
   * If you have a different style ping command format and output, then
   * alter here.
   *
   *   nutty> /usr/etc/ping <arguments> | tail -2
   *	 5 packets transmitted, 5 packets received, 0% packet loss
   *	 round-trip (ms)  min/avg/max = 4/4/5
   */
#if defined(SUNOS4) || defined(SUNOS5)
  sprintf(pingcmd,"%s -s %s %d %d 2>&1",
	  ping, pdevices[0], pktsize, npackets);
#else
# if defined(HPUX) || defined(ULTRIX)
  sprintf(pingcmd,"%s %s %d %d",
	  ping, pdevices[0], pktsize, npackets);
# else  /* for FreeBSD2, Linux1, Linux2, Irix, OSF1, AIX */
  sprintf(pingcmd,"%s -c %d -s %d %s",
	  ping, npackets, pktsize, pdevices[0]);
# endif	/* HPUX */
#endif	/* SunOS */

  if (debug > 1)
    fprintf(stderr, "pingcmd = %s\n", pingcmd);
  if ((pcmd = popen(pingcmd, "r")) == NULL)	/* open up the pipe */
  {
    perror("ippingmon popen()");
    return(-1);
  }
  
  /*
   * Trying to scan line of the form:
   *	10 packets transmitted, 1 packets received, 90% packet loss
   *	round-trip (ms)  min/avg/max = 2/2/3
   */
  while (fgets(line, sizeof(line), pcmd) != NULL)
  {
    if (strstr(line, "transmitted"))	/* skip all lines till here */
      sscanf (line, "%d %*s %*s %d", &sent, &recv);
    if (strstr(line, "round-trip")) {
      char *s;
      s = strstr(line, "=");
      sscanf (s, "%*s %f/%f/%f", &minrtt, &avgrtt, &maxrtt);
    }
  }	/* while fgets() */

  if (sent != npackets) {		/* Error of some sort */
    if (debug)
      fprintf (stderr,"ERROR %s: (ippingmon) Sent %d != recvd %d\n",
	       pdevices[0], npackets, sent);
    recv = 0 ;
  }

  if (pclose(pcmd) < 0)		      /* close the pipe */
    perror("ippingmon pclose()");

  rvalues[0] = recv;
  rvalues[1] = (int)avgrtt;	/* drop any decimal part */
}	/* ippingmon() */

/*
 * same as ippingmon since osiping's output is the same.
 */
osipingmon(pdevices)
  char **pdevices;
{
  int sent, recv = 0;
  float minrtt, avgrtt, maxrtt;
  FILE *pcmd;

  sprintf(pingcmd,"%s -c %d -s %d %s",
	  ping, npackets, pktsize, pdevices[0]);

  if (debug > 1)
    fprintf(stderr, "pingcmd = %s\n", pingcmd);
  if ((pcmd = popen(pingcmd, "r")) == NULL)	/* open up the pipe */
  {
    perror("osipingmon popen()");
    return(-1);
  }
  
  /*
   * Trying to scan line of the form:
   *	10 packets transmitted, 1 packets received, 90% packet loss
   *	round-trip (ms)  min/avg/max = 2/2/3
   */
  while (fgets(line, sizeof(line), pcmd) != NULL)
  {
    if (strstr(line, "transmitted"))	/* skip all lines till here */
      sscanf (line, "%d %*s %*s %d", &sent, &recv);
    if (strstr(line, "round-trip")) {
      char *s;
      s = strstr(line, "=");
      sscanf (s, "%*s %f/%f/%f", &minrtt, &avgrtt, &maxrtt);
    }
  }	/* while fgets() */

  if (sent != npackets) {		/* Error of some sort */
    if (debug)
      fprintf (stderr,"ERROR %s: (osipingmon) Sent %d != recvd %d\n",
	       pdevices[0], npackets, sent);
    recv = 0 ;
  }

  if (pclose(pcmd) < 0)		      /* close the pipe */
    perror("osipingmon pclose()");

  rvalues[0] = recv;
  rvalues[1] = (int)avgrtt;	/* drop any decimal part */
}	/* ippingmon() */


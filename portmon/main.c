/* #define DEBUG		/* */
/* $Header$  */

/*+ 		portmon
 * FUNCTION:
 * 	Monitor TCP ports and reponses for SNIPS. It can send a string
 * and then parse the responses against a list. Each response can be
 * assigned a severity. Checks simple port connectivity if given a NULL
 * response string.
 *
 * CAVEATS:
 *	1) Uses case insensitive 'strstr' and not a real regular expression.
 *	2) Looks only at the first buffer of the response unless using the
 *	   timeouts to calculate the response time.
 *
 * AUTHOR:
 *  	Vikas Aggarwal, -vikas@navya_.com
 */


/*
 * $Log$
 * Revision 1.2  2008/06/26 17:03:46  tvroon
 * GCC 3.4.1 reported these conflicting types.
 *
 * Revision 1.1  2008/04/25 23:31:52  tvroon
 * Portability fixes by me, PROMPTA/B switch by Robert Lister <robl@linx.net>.
 *
 * Revision 1.0  2001/07/08 22:48:28  vikas
 * For SNIPS
 *
 * Instead of storing the status, now stores the elapsedsecs
 * (submitted by eankingston@home.com)
 *
 */

/* Copyright 2000, Netplex Technologies Inc, */

/*  */
#ifndef lint
static char rcsid[] = "$Id$" ;
#endif

#define _MAIN_
#include "portmon.h"
#include "portmon-local.h"
#include "netmisc.h"
#include "snips_funcs.h"
#include "snips_main.h"
#include "event_utils.h"
#undef _MAIN_

static int	rtimeout = RTIMEOUT;		/* select timeout in seconds */
static int	simulconnects = MAXSIMULCONNECTS;
struct _harray	*hostlist;			/* global ptr to list */
extern char	*skip_spaces(), *Strdup();	/* in libsnips */

/* function prototypes */
void set_functions();
void free_hostlist(struct _harray *phlist);

int main(ac, av)
  int ac;
  char **av;
{
  hostlist = NULL;
  set_functions();
  snips_main(ac, av);
  return(0);
}
  
/*+
 * FUNCTION:
 * 	Read the config file.
 * POLLINTERVAL ppp
 * HOST  <hostname>  <address>  <var>  <port> <failseverity>  <send string>
 * <severity>	response1 till end of this line
 * <severity>   response2 till end of this line
 *
 */
#define NEXTTOK  (char *)skip_spaces(strtok(NULL, " \t"))

int readconfig()
{
  int mxsever, i, fdout,  lineno = 0;
  char *j1;				/* temp string pointers */
  char record[BUFSIZ], *sender;
  char *configfile, *datafile;
  FILE *pconfig ;
  EVENT v;                            	/* Defined in SNIPS.H */
  struct _harray *h;			/* temp ptr */
  struct _response *r;

  if ((sender = (char *)strrchr (prognm , '/')) == NULL)
    sender = prognm ;			/* no path in program name */
  else
    sender++ ;				/* skip leading '/' */

  configfile = get_configfile();    datafile = get_datafile();
  if ( (fdout = open_datafile(datafile, O_RDWR|O_CREAT|O_TRUNC)) < 0)
  {
    fprintf(stderr, "(%s) ERROR in open datafile %s", prognm, datafile);
    perror (datafile);
    return (-1);
  }

  if ((pconfig = fopen(configfile, "r")) == NULL)
  {
    fprintf(stderr, "%s error (init_devices) ", prognm) ;
    perror (configfile);
    return (-1);
  }

  if (hostlist) 		/* re-reading config file */
    free_hostlist(hostlist);
  hostlist = NULL;

  init_event(&v);
  strncpy (v.sender, sender, sizeof(v.sender) - 1);
  strncpy (v.var.units, VARUNITS, sizeof (v.var.units) - 1);

  /*
   * Now parse the config file
   */

  h = hostlist;
  while(fgets(record, BUFSIZ - 3, pconfig) != NULL ) 	/* keeps the \n */
  {
    static int skiphost;
    int port;
    int checkspeed = 0;
    int readquitstr = 0;

    ++lineno;
    record[strlen(record) - 1] = '\0' ;		/* chop off newline */
    if (*record == '#' || *(skip_spaces(record)) == '\0')
      continue ;

    if (strncasecmp(record, "POLLINTERVAL", strlen("POLLINTERVAL")) == 0)
    {
      strtok(record, " \t");
      j1 = (char *)skip_spaces(strtok(NULL, "")) ;
      if (!j1 || *j1 == '\0' || (pollinterval = atoi(j1)) <= 0)
	fprintf(stderr,	"%s [%d]: bad or missing pollinterval value\n",
		prognm, lineno);

      pollinterval = (pollinterval > 0 ? pollinterval : POLLINTERVAL);
      if (debug)
	fprintf (stderr, "(debug) %s: Pollinterval = %d\n", 
		 prognm, pollinterval);

      continue;
    }

    if ( strncasecmp(record, "READTIMEOUT", strlen("READTIMEOUT")) == 0 ||
	 strncasecmp(record, "TIMEOUT", strlen("TIMEOUT")) == 0 )
    {
      strtok(record, " \t");
      j1 = (char *)skip_spaces(strtok(NULL, "")) ;
      if (!j1 || *j1 == '\0' || (rtimeout = atoi(j1)) <= 0)
	fprintf(stderr, "%s [%d]: bad or missing READTIMEOUT value\n",
		prognm, lineno);

      rtimeout = (rtimeout > 0 ? rtimeout : RTIMEOUT);
      if (debug)
	fprintf (stderr, "(debug) %s: ReadTimeout = %d\n", 
		 prognm, rtimeout);

      continue;
    }

    if (strncasecmp(record, "SIMULCONNECT", strlen("SIMULCONNECT")) == 0)
    {
      strtok(record, " \t");
      j1 = (char *)skip_spaces(strtok(NULL, "")) ;
      if (!j1 || *j1 == '\0' || (simulconnects = atoi(j1)) <= 0)
	fprintf(stderr, "%s [%d]: bad or missing SIMULCONNECT value\n",
		prognm, lineno);

      if (simulconnects > MAXSIMULCONNECTS || simulconnects <= 0)
	simulconnects = MAXSIMULCONNECTS;
#ifdef FD_SETSIZE
      if (simulconnects > (FD_SETSIZE - 6))
	simulconnects = FD_SETSIZE - 6;
#endif
      if (debug)
	fprintf (stderr, "(debug) %s: SimultaneousConnects = %d\n", 
		 prognm, simulconnects);

      continue;
    }

    if (strncasecmp(record, "rrdtool", strlen("rrdtool")) == 0)
    {
      strtok(record, " \t");
      j1 = (char *)skip_spaces(strtok(NULL, " \t\n"));
      if (strncasecmp(j1, "on", strlen("on")) == 0)
      {
#ifdef RRDTOOL
	++dorrd;
	if (debug > 1)
	  fprintf(stderr, "readconfig(): RRD enabled\n");
#else
	fprintf(stderr, "%s: RRDtool not supported\n", prognm);
#endif
      }
      continue;
    }

    if (strncasecmp(record, "HOST", 4) != 0)	/* Not HOST line */
    {
      if (skiphost || h == NULL)	/* not within a HOST block */
      {
	fprintf(stderr, "readconfig [%d]: ignoring config line %s\n", 
		lineno, record);
	continue ;
      }

      j1 = strtok(record," \t") ;			/* severity level */
      j1 = (char *)skip_spaces(j1) ;
      switch(tolower(*j1))
      {
      case 'c': case '1': mxsever = E_CRITICAL;	break;
      case 'e': case '2': mxsever = E_ERROR; 	break;
      case 'w': case '3': mxsever = E_WARNING;	break;
      case 'i': case '4': mxsever = E_INFO; 	break;
      case 't': mxsever = E_CRITICAL; ++checkspeed; break;	/* TIME */
      case 'q': ++readquitstr ; break;		/* QUIT string */
      default:  mxsever = E_CRITICAL;		break ;
      }

      j1 = (char *)skip_spaces(strtok(NULL, "")) ;

      if (readquitstr)
      {
	if (j1 == NULL || *j1 == '\0')
	  fprintf(stderr, "%s [%d]: missing QUIT string\n", prognm, lineno);
	else {
	  strcat (j1, "\r\n");
	  h->quitstr = (char *)Strdup(j1);
	}
	continue;
      }		/* if (readquitstr) */

      if (checkspeed && h->timeouts[0] == 0)
      {		 		/* read timeout thresholds */
	if (3 != sscanf(j1, "%u %u %u", &(h->timeouts[0]),
			&(h->timeouts[1]), &(h->timeouts[2])))
	{
	  fprintf(stderr, "%s [%d]: need 3 time thresholds, invalid syntax\n",
		  prognm, lineno);
	  continue;
	}
	strtok(j1, " \t"); strtok(NULL, " \t"); strtok(NULL, " \t");
	j1 = (char *)skip_spaces(strtok(NULL, ""));
      }		/* end if(checkspeed) */

      if (j1 == NULL || *j1 == '\0')
      {
	fprintf(stderr, "%s [%d]: missing response string, skipped\n",
		prognm, lineno);
	fprintf(stderr, "For null responses, dont put any lines\n");
	continue;
      }

      if (h->responselist == NULL)
      {
	r = (struct _response *)malloc(sizeof(struct _response));
	h->responselist = r;
      }
      else
      {
	r->next = (struct _response *)malloc(sizeof(struct _response));
	r = r->next;
      }
      bzero(r, sizeof(struct _response));
      r->response = (char *)Strdup(j1);
      r->severity = mxsever ;

      continue ;	/* next input line */
    }

    /*
     * Here if parsing a HOST line
     */
	
    skiphost = 0;		/* assume valid config */
    strtok(record, " \t");	/* skip HOST keyword */	
	    
    strncpy(v.device.name, NEXTTOK, sizeof(v.device.name) - 1);
    strncpy(v.device.addr, NEXTTOK, sizeof(v.device.addr) - 1);
    if (get_inet_address(NULL, v.device.addr) < 0)        /* bad address */
    {
      fprintf(stderr,
	      "(%s): Error in addr '%s' for device '%s', ignoring\n",
	      prognm, v.device.addr, v.device.name);
      skiphost = 1;
      continue ;
    }
    strncpy(v.var.name, NEXTTOK, sizeof(v.var.name) - 1);
	
    if ((port = atoi(NEXTTOK)) == 0)
    {
      fprintf(stderr,
	      "(%s): Error in port for device '%s', ignoring\n",
	      prognm, v.device.name);
      skiphost = 1;
      continue ;
    }
	
    j1 = NEXTTOK;	/* severity level */
    if (j1 == NULL)
      j1 = "c";
    switch(tolower(*j1))
    {
    case 'c': mxsever = E_CRITICAL; break;
    case 'e': mxsever = E_ERROR; break;
    case 'w': mxsever = E_WARNING; break;
    case 'i': mxsever = E_INFO; break;
    default:  mxsever = E_CRITICAL ; break ;
    }

    /*
     *if (debug)
     *fprintf (stderr, "Name: %s %s, VAR: %s, Port %d SEV: %d\n",
     *       v.device.name,v.device.addr,v.var.name, port, mxsever);
     */
	
    /* the rest of string is SEND */
    if (h) {
      h->next = (struct _harray *)malloc(sizeof(struct _harray));
      h = h->next;
    }
    else {
      h = (struct _harray *)malloc(sizeof(struct _harray));
      if (hostlist == NULL)
	hostlist = h;
    }
    bzero (h, sizeof(struct _harray));

    h->writebuf = NULL ;		/* Initialize.. */
    j1 = (char *)skip_spaces(strtok(NULL, ""));
    if (j1 && *j1)
    {
      /* We NEED a newline at the end of the send strings. http requires 2 */
      raw2newline(j1);
      strcat(j1, "\r\n");
      h->writebuf = (char *)Strdup(j1) ;

      /*if(debug)
       *fprintf(stderr, "(debug)Sendstring: >>>%s<<<\n", h->send);
       */
    }
    h->readbuf = (char *)malloc(READBUFLEN);
    h->port = port;
    h->connseverity = mxsever ;
    h->hname = (char *)Strdup(v.device.name);
    h->ipaddr = (char *)Strdup(v.device.addr);
	
    if (write_event(fdout, &v) != sizeof(v))
      snips_done() ;
  }	/* end: while */

  fclose (pconfig);       		/* Not needed any more */
  close_datafile(fdout);

  if (!pollinterval)
    pollinterval = POLLINTERVAL ;		/* default value */

  if (debug > 1)
    for (h = hostlist, i=0 ; h; h = h->next)
    {
      fprintf(stderr, "#%d. Host=%s %s :%d, MaxSev= %d, Sendstr=%s",
	      i++, h->hname, h->ipaddr, h->port,  h->connseverity,
	      (h->writebuf ? h->writebuf : "\n"));
      for (r = h->responselist; r; r = r->next)
	fprintf(stderr, "\t%s (sev=%d)\n", r->response, r->severity);
    }

  return(1);                          /* All OK  */

}  /* end:  readconfig() */


/*
 * Make one complete pass over all the devices
 */
int poll_devices()
{
  int i, fdout;
  char *datafile;
  struct _harray *h;
  EVENT v;

  datafile = get_datafile();
  if ( (fdout = open_datafile(datafile, O_RDWR)) < 0)
  {
    fprintf(stderr, "(%s) ERROR in open datafile %s", prognm, datafile);
    perror (datafile);
    return (-1);
  }

  h = hostlist;
  while (h != NULL)	/* one entire pass */
  {
    struct _harray *harray[MAXSIMULCONNECTS + 1];

    bzero(harray, sizeof(harray));
    for (i = 0; i < simulconnects && h != NULL; h = h->next)
      harray[i++] = h;

    if (checkports(harray, i, rtimeout) == -1)	/* fills status and severity */
    {
      fprintf(stderr, "fatal error, checkports returned -1, exiting\n");
      return (-1);
    }
    for (i = 0; harray[i] != NULL; ++i)
    {
      read_event(fdout, &v);
      if (harray[i]->status == -1)
	fprintf (stderr, "%s: Error in checkports(), skipping device %s\n",
		 prognm, (harray[i])->hname);
      else
      {
	update_event(&v, harray[i]->status,
		     /* value */ (u_long)(harray[i]->elapsedsecs),
		     v.var.threshold, harray[i]->testseverity) ;
	rewrite_event(fdout, &v);
      }
    }	/* for() */

    if (do_reload)		/* recieved HUP */
      break;
  }	/* while(h != NULL) */

  close_datafile(fdout);

  return (1);
}	/* end:  poll_devices()  */

void help()
{
  snips_help();
}

/*
 *
 */
void free_hostlist(phlist)
  struct _harray *phlist;
{
  struct _harray *curptr, *nextptr;
  struct _response *r, *s;

  for (curptr = phlist; curptr; curptr = nextptr)
  {
    nextptr = curptr->next;	/* store before freeing curptr */
    if (curptr->hname)    free (curptr->hname);
    if (curptr->ipaddr)   free (curptr->ipaddr);
    if (curptr->writebuf) free (curptr->writebuf);
    if (curptr->readbuf)  free (curptr->readbuf);
    if (curptr->quitstr)  free (curptr->quitstr);
    /* wptr and rptr are just pointers, not malloced */

    for (r = curptr->responselist; r; r = s)
    {
      s = r->next;
      if (r->response)   free(r->response);
      free (r);
    }
    free (curptr);
  }

  phlist = NULL;
}

/*
 * Since we are calling the snips_main(), override the lib functions
 */
void set_functions()
{
  int readconfig(), poll_devices();
  void help();

  set_help_function(help);
  set_readconfig_function(readconfig);
  set_polldevices_function(poll_devices);
}

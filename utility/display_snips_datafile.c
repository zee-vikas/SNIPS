/*+ 
** $Header$
**/

/*+ 
** This module displays the SNIPS EVENT structure.
**/

#define _MAIN_
#include "snips.h"
#undef _MAIN_

#include	<stdio.h>
#include 	<sys/file.h>

static int altmode = 0;		/* alternate display mode */

main(ac, av)
  int ac ;
  char **av ;
{
  if (ac < 2) {
    fprintf(stderr, "USAGE: %s [-a] [-d] datafile...\n", av[0]);
    exit (1);
  }
  while (--ac)
  {
    ++av;
    if (strcmp(*av, "-d") == 0) {
      ++debug;
      continue;
    }
    if (strcmp(*av, "-a") == 0) {
      ++altmode;
      continue;
    }
    display_snips_datafile(*av) ;
  }
}


display_snips_datafile(file)
  char *file;
{
  EVENT v;
  int bufsize, fd;
  int ver;
  
  if ( (fd = open(file, O_RDONLY)) < 0)
  {
    fprintf (stderr, "Error in opening file %s\n", file);
    perror((char *) NULL);
    return (1);
  }
  if ( (ver = read_dataversion(fd)) != DATA_VERSION )
  {
    fprintf(stderr, "%s: Data format version incompatible (is %d, not %d)\n",
	    file, ver, DATA_VERSION);
    return (1);
  }
  fprintf (stdout, ":::::  %s  ::::\n\n", file);
  
  while ((bufsize=read (fd, (char *)&v, sizeof(v))) == sizeof(v))
    if (altmode == 0)
      printf("%s", (char *)event_to_logstr(&v));
    else {
      int i;
      static char **headers;
      char **values;

      if (headers == NULL)
	headers = (char **)event2strarray(NULL);
      values = (char **)event2strarray(&v);
      for (i=0; headers[i] && *(headers[i]); ++i)
	printf("%s=%s, ", headers[i], values[i]);
      printf("\n");
    }
  
  if (bufsize != 0)			/* Error */
    fprintf (stderr, "Invalid data in %s\n", file);

}	/* end: display_snips_datafile */


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

main(ac, av)
  int ac ;
  char **av ;
{
  while (--ac)
    display_snips_datafile(*++av) ;
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
    printf("%s", (char *)event_to_logstr(&v));
  
  if (bufsize != 0)			/* Error		*/
    fprintf (stderr, "Invalid data in %s\n", file);

}	/* end: display_snips_datafile */


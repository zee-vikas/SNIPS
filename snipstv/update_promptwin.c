/*
 * $Header$
 */

/*
 * Update the prompt panel (last line).
 */

/*
 * $Log$
 * Revision 1.0  2001/07/09 03:33:52  vikas
 * sniptstv for SNIPS v1.0
 *
 */

/* Copyright 2001, Netplex Technologies Inc. */

#ifndef lint
  static char rcsid[] = "$Id$";
#endif

#include <sys/types.h>
#include <sys/time.h>

#include "snipstv.h"		/* includes the curses header files */

#define PROMPTA "Enter option, 'q' to quit, 'h' for help: "
#define PROMPTB "Enter option, or any other key for next screen: "

update_promptwin(win)
  WINDOW *win;
{
  
#ifdef __NetBSD__
  if (getcury(EventPanel.win) >= (getmaxy(EventPanel.win) - 1))
#else
  if (EventPanel.win->_cury >= (EventPanel.win->_maxy - 1))
#endif
    mvwprintw(win, 1, 0, "%s", PROMPTA);	/* end of screens */
  else
    mvwprintw(win, 1, 0, "%s", PROMPTB);	/* next screen */

  wclrtoeol(win);
}

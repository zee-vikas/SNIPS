/* $Header$ */
/*
 * This should not be required in newer versions of OSF1 where pfopen()
 * is part of the C library.
 */

#ifdef OSF1
# include "/usr/examples/packetfilter/pfopen.c"
#endif

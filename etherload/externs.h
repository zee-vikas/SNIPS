/*
 * $Header$
 *
 * externs.h - external definitons for etherload
 *
 */

extern char		*prognm;

extern int		breakscan;
extern int		sleepsecs;
extern int		scansecs;
extern int		debug;
extern int		errno;
extern int		extended;
extern int		ninterfaces;
extern int		truncation;

extern u_long	totaltime;
extern u_long	totalpkts;
extern u_long	avg_pkt_sz;
extern u_long	est_total_bytes;	/* Notice kbytes to prevent overflow */
extern u_long	kbps;			/* Utilization in kbits per sec */
extern u_long	bw;			/* bandwidth in percentage */
extern u_long	pps;			/* packets per second */
extern u_long	dropspct;		/* percent pkt drops */

extern struct timeval	starttime;
extern struct timeval	endtime;

char			*ndt_name();
char			*prtime();
char			*dlt_namebw();

void			error();
void			finish();
void			pkt_filter_ether();
void			pkt_filter_fddi();
void			usage();
void			wakeup();
void			flush_device();


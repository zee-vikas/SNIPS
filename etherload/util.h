#ifndef UTIL_H
#define UTIL_H

char *ndt_namebw(int ndt, long *bw);
int dostats(struct _if_stats *p);
void error(register char *str);
void finish(int code);

#endif

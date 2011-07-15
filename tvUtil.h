#ifndef TVUTIL_H
#define TVUTIL_H
#include <sys/time.h>

double tvDouble(const struct timeval *ts);
void tvSub(const struct timeval *lhs, const struct timeval *rhs, 
	   struct timeval *dest);
#endif

#include <assert.h>
#include <stdio.h>
#include "gettimeofday_helpers.h"

#include "tvUtil.h"

/*! @todo this is wrong
double
delta_seconds(struct timeval *s, struct timeval *e){
        return (double) ( (e->tv_sec - s->tv_sec) + (e->tv_usec - s->tv_usec)/1000000.0 );
}
*/

double delta_seconds(struct timeval *s, struct timeval *e){
  struct timeval delta;
  tvSub(e, s, &delta);
  return tvDouble(&delta);
}

void
dump_timeval( FILE *f, char *str, struct timeval *t ){
	if( f != NULL ){
		fprintf( f, "%s %10ld.%06ld\n", 
				str, 
				(long int)t->tv_sec, 
				(long int)t->tv_usec );
	}
}


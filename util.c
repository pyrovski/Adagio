#include <assert.h>
#include "util.h"

double
delta_seconds(struct timeval *s, struct timeval *e){
        return (double) ( (e->tv_sec - s->tv_sec) + (e->tv_usec - s->tv_usec)/1000000.0 );
}

FILE *
initialize_logfile(int rank){
        FILE *fp=NULL;
        char format[]="runtime.%02d.dat";
        char fname[64];
        sprintf(fname, format, rank);
        fp = fopen(fname, "w");
        assert(fp);
        return fp;
}

#define UNW_LOCAL_ONLY
#include <libunwind.h>
int
hash_backtrace(int fid) {
	int rc=0;
	unw_cursor_t cursor; unw_context_t uc;
	unw_word_t ip, sp;

	unw_getcontext(&uc);
	unw_init_local(&cursor, &uc);
	while (unw_step(&cursor) > 0) {
		unw_get_reg(&cursor, UNW_REG_IP, &ip);
		unw_get_reg(&cursor, UNW_REG_SP, &sp);
		//printf ("ip = %lx, sp = %lx\n", (long) ip, (long) sp);
		//rc += (int)( (ip >> (sp%3)) * (sp >> (sp%5)) * 13) ;
		rc += (int)( (ip >> 2) + (sp >> 2) ) ;
	}
	return (rc+fid*11) & 0xfff;
}

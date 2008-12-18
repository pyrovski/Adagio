#include <assert.h>
#include "util.h"
#include "md5.h"

double
delta_seconds(struct timeval *s, struct timeval *e){
        return (double) ( (e->tv_sec - s->tv_sec) + (e->tv_usec - s->tv_usec)/1000000.0 );
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
	unw_cursor_t cursor; unw_context_t uc;
	unw_word_t ip, sp;

	md5_state_t pms;
	md5_byte_t digest[16];
	md5_init(   &pms );

	unw_getcontext(&uc);
	unw_init_local(&cursor, &uc);
	while (unw_step(&cursor) > 0) {
		unw_get_reg(&cursor, UNW_REG_IP, &ip);
		unw_get_reg(&cursor, UNW_REG_SP, &sp);
		md5_append( &pms, (md5_byte_t *)(&ip), sizeof(unw_word_t) );
		md5_append( &pms, (md5_byte_t *)(&sp), sizeof(unw_word_t) );
	}
	md5_append( &pms, (md5_byte_t*)(&fid), sizeof(int) );
	md5_finish( &pms, digest );
	return *((int*)digest) & 0x1fff; //8192 entries.
}

#include "stacktrace.h"
#include "md5.h"
#include <stdio.h>
#define UNW_LOCAL_ONLY
#include <libunwind.h>

/*! @todo this might be modified to give consistent hashes 
across program runs and hosts
*/
int hash_backtrace(int fid, int rank) {
	unw_cursor_t cursor; unw_context_t uc;
	unw_word_t ip;
	//, sp;

	md5_state_t pms;
	md5_byte_t digest[16];

	int status;

	md5_init(   &pms );

	status = unw_getcontext(&uc);
	if(status != 0)
	  fprintf(stderr, "unw_getcontext() error\n");

	status = unw_init_local(&cursor, &uc);
	if(status != 0)
	  fprintf(stderr, "unw_getcontext() error\n");

	while ((status = unw_step(&cursor)) > 0) {
	  status = unw_get_reg(&cursor, UNW_REG_IP, &ip);
	  if(status != 0)
	    fprintf(stderr, "unw_getcontext() error\n");

	  /*
	  status = unw_get_reg(&cursor, UNW_REG_SP, &sp);
	  if(status != 0)
	    fprintf(stderr, "unw_getcontext() error\n");
	  */

	  md5_append( &pms, (md5_byte_t *)(&ip), sizeof(unw_word_t) );
	  //md5_append( &pms, (md5_byte_t *)(&sp), sizeof(unw_word_t) );
	  status = 0;
	}
	if(status < 0)
	  fprintf(stderr, "unw_step() error\n");

	md5_append( &pms, (md5_byte_t*)(&fid), sizeof(int) );
	md5_finish( &pms, digest );
	return *((int*)digest) & 0x1fff; //8192 entries.
}

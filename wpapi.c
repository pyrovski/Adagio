/* wpapi.c
 *  Wrappers for the papi calls. 
 *  Prints debugging info and terminates in case of error.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include "wpapi.h"

static void initialize_papi();
//static int wpapi_library_init(int version);
static int wpapi_create_eventset (int *eventset);
static int wpapi_add_events(int eventset, int *eventcodes, int number);
static int wpapi_add_event(int eventset, int eventcode);
//static int wpapi_read(int EventSet, long_long *values);
//static int wpapi_accum(int EventSet, long_long *values);
static int wpapi_start(int EventSet);
static int wpapi_stop(int EventSet, long_long *values);
//static int wpapi_reset(int EventSet);

static int EventSet = PAPI_NULL;

/*! make sure to update the enum in wpapi.h */
int events[] = {PAPI_TOT_INS,
		PAPI_L3_TCM, 
		PAPI_TLB_DM, 
		PAPI_STL_ICY, 
		PAPI_SR_INS, 
		PAPI_LD_INS};

static void initialize_papi(void);

void
start_papi(){
        static int initialized=0;
        if(!initialized){
                initialize_papi();
                initialized=1;
        }

        //Resets counter to zero when called.
        wpapi_start(EventSet);
}

void
stop_papi(long_long *counters){
        wpapi_stop(EventSet, counters);
}


static void
initialize_papi(){
        int rc;

	assert(num_counters == sizeof(events)/sizeof(int));

        rc=PAPI_library_init(PAPI_VER_CURRENT);
        if(rc != PAPI_VER_CURRENT){
                fprintf(stderr,"%s::%d PAPI_library_init error:  ", __FILE__, __LINE__);
                switch(rc){
                        case PAPI_EINVAL:
                                fprintf(stderr,"PAPI_EINVAL (papi.h version doesn't match library)  ");
                                break;
                        case PAPI_ENOMEM:
                                fprintf(stderr,"PAPI_ENOMEM (out of memory)  ");
                                break;
                        case PAPI_ESBSTR:
                                fprintf(stderr,"PAPI_ESBSTR (substrate issues)  ");
                                break;
                        case PAPI_ESYS:
                                perror("\nfroodle");
                                fprintf(stderr,"PAPI_ESYS (system call failed)  ");
				exit(-1);
                                break;
                        default:
                                fprintf(stderr,"PAPI_WTF?  ");
                                break;
                }
                fprintf(stderr,"rc=%d.\n", rc);
        }else{
                //fprintf(stdout,"%s::%d PAPI library initialized.\n",
                //      __FILE__, __LINE__);
        }
        wpapi_create_eventset(&EventSet);
	wpapi_add_events(EventSet, events, num_counters);
}

int 
wpapi_library_init(int version){
	int rc;
	rc=PAPI_library_init(version);
	if(rc != version){
		fprintf(stderr,"%s::%d PAPI_library_init error\n", __FILE__, __LINE__);
		PAPI_perror(rc,NULL,0);
		exit(rc);
	}
	return rc;
}

int 
wpapi_create_eventset (int *eventset){
	int rc;
	rc = PAPI_create_eventset(eventset);
	if(rc != PAPI_OK){
		fprintf(stderr,"%s::%d PAPI_create_eventset error\n", __FILE__, __LINE__);
		PAPI_perror(rc,NULL,0);
		exit(rc);
	}
	return rc;
}

int 
wpapi_add_events(int eventset, int *eventcodes, int number){
	int rc;
	rc = PAPI_add_events(eventset, eventcodes, number);
	if(rc != PAPI_OK){
		fprintf(stderr,"%s::%d PAPI_add_events error\n", __FILE__, __LINE__);
		if(rc>0){
			fprintf(stderr,"%d consecutive elements succeeded before the error.  Whatever that means.  ", rc);
		}else{
			PAPI_perror(rc,NULL,0);
		}
		fprintf(stderr,"rc=%d.\n", rc);
		exit(rc);
	}
	return rc;
}

int
wpapi_add_event( int eventset, int eventcode ){
	int rc;
	rc = PAPI_add_event( eventset, eventcode );
	if(rc != PAPI_OK){
		fprintf(stderr,"%s::%d PAPI_add_events error\n", __FILE__, __LINE__);
		if(rc>0){
			fprintf(stderr,"%d consecutive elements succeeded before the error.  Whatever that means.  ", rc);
		}else{
			PAPI_perror(rc,NULL,0);
		}
		fprintf(stderr,"rc=%d.\n", rc);
		exit(rc);
	}
	return rc;
}
	
int 
wpapi_read(int EventSet, long_long *values){
	int rc;
	rc = PAPI_read(EventSet, values);
	if(rc!= PAPI_OK){
		fprintf(stderr,"%s::%d PAPI_read error\n", __FILE__, __LINE__);
		PAPI_perror(rc,NULL,0);
		fprintf(stderr,"rc=%d.\n", rc);
		exit(rc);
	}
	return rc;
}

int wpapi_accum(int EventSet, long_long *values){
	int rc;
	rc = PAPI_accum(EventSet, values);
	if(rc!= PAPI_OK){
		fprintf(stderr,"%s::%d PAPI_accum error\n", __FILE__, __LINE__);
		PAPI_perror(rc,NULL,0);
		fprintf(stderr,"rc=%d.\n", rc);
		exit(rc);
	}
	return rc;
}

int wpapi_start(int EventSet){
	int rc;
	rc = PAPI_start(EventSet);
	if(rc!=PAPI_OK){
		fprintf(stderr,"%s::%d PAPI_start error\n", __FILE__, __LINE__);
		PAPI_perror(rc,NULL,0);
		fprintf(stderr,"rc=%d.\n", rc);
		exit(rc);
	}
	return rc;
}

int wpapi_stop(int EventSet, long_long *values){
	int rc;
	rc = PAPI_stop(EventSet, values);
	if(rc!=PAPI_OK){
		fprintf(stderr,"%s::%d PAPI_stop error\n", __FILE__, __LINE__);
		PAPI_perror(rc,NULL,0);
		fprintf(stderr,"rc=%d.\n", rc);
		exit(rc);
	}
	return rc;
}

int wpapi_reset(int EventSet){
	int rc;
	rc = PAPI_reset(EventSet);
	if(rc!=PAPI_OK){
		fprintf(stderr,"%s::%d PAPI_reset error\n", __FILE__, __LINE__);
		PAPI_perror(rc,NULL,0);
		fprintf(stderr,"rc=%d.\n", rc);
		exit(rc);
	}
	return rc;
}

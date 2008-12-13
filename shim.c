/* shim.c
 *
 * The two global functions here, shim_pre() and shim_post, are called from the
 * shim_functions file when an MPI call is intercepted.
 */
#include <stdio.h>
#include <stdlib.h>	// getenv
#include <string.h>	// strlen, strstr
#include "shim_enumeration.h"
#include "shim.h"
#include "util.h"	// Brings in <stdio.h>, <sys/time.h>, <time.h>

static int rank, size;

static struct timeval ts_start_communication, ts_start_computation,
			ts_stop_communication, ts_stop_computation;

static int g_algo;	// which algorithm(s) to use.  (Not all combinations
			//   necessarily make sense.

static int g_freq;	// frequency to use with fixedfreq.

static int g_trace;	// tracing level.  

FILE *logfile;

enum{ 
	// To run without library overhead, use the .pristine binary.
	algo_NONE	= 0x000,	// Identical to CLEAN.
	algo_FERMATA 	= 0x001,	// slow communication
	algo_ANDANTE 	= 0x002,	// slow comp before global sync pts.
	algo_ADAGIO  	= 0x004,	// fermata + andante
	algo_ALLEGRO 	= 0x008,	// slow computation everywhere.
	algo_FIXEDFREQ	= 0x010,	// run whole program at single freq.
	algo_JITTER  	= 0x020,	// per ncsu.
	algo_MISER   	= 0x040,	// per vt.
	algo_CLEAN   	= 0x080,	// Identical to fixedfreq=0.
	algo_FAKEJOULES = 0x100,	// Pretend power meter present.
	algo_FAKEFREQ   = 0x200,	// Pretend to change the cpu frequency.

	trace_NONE	= 0x000,
	trace_TS	= 0x001,	// Timestamp.
	trace_FILE	= 0x002,	// Filename where MPI call occurred.
	trace_LINE	= 0x004,	// Line # where MPI call occurred.
	trace_FN	= 0x008,	// MPI Function Name.
	trace_COMP	= 0x010,	// Elapsed comp time since the end of
					//   the previous MPI comm call.
	trace_COMM	= 0x020,	// Elapsed comm time spend in mpi lib.
	trace_RANK	= 0x040,	// MPI rank.
	trace_PCONTROL	= 0x080,	// Most recent pcontrol.
	trace_ALL	= 0xFFF
	
};

////////////////////////////////////////////////////////////////////////////////
// Init
////////////////////////////////////////////////////////////////////////////////
static void
pre_MPI_Init( union shim_parameters *p ){

	char *env_algo, *env_trace, *env_freq;
	p=p;

	// Using "-mca key value" on the command line, the environment variable
	// "OMPI_MCA_key" is set to "value".  mpirun doesn't check the validity
	// of these (yay!), so we end up with an easy way of controlling the 
	// runtime environment without confusing the application 
	env_algo=getenv("OMPI_MCA_gmpi_algo");
	if(env_algo && strlen(env_algo) > 0){
		g_algo &= strstr(env_algo, "none"      ) ? algo_NONE      : 0xFFF;
		g_algo |= strstr(env_algo, "fermata"   ) ? algo_FERMATA   : 0;
		g_algo |= strstr(env_algo, "andante"   ) ? algo_ANDANTE   : 0;
		g_algo |= strstr(env_algo, "adagio"    ) ? algo_ADAGIO    : 0;
		g_algo |= strstr(env_algo, "allegro"   ) ? algo_ALLEGRO   : 0;
		g_algo |= strstr(env_algo, "fixedfreq" ) ? algo_FIXEDFREQ : 0;
		g_algo |= strstr(env_algo, "jitter"    ) ? algo_JITTER    : 0;
		g_algo |= strstr(env_algo, "miser"     ) ? algo_MISER     : 0;
		g_algo |= strstr(env_algo, "clean"     ) ? algo_CLEAN     : 0;
		g_algo |= strstr(env_algo, "fakejoules") ? algo_FAKEJOULES: 0;
		g_algo |= strstr(env_algo, "fakefreq"  ) ? algo_FAKEFREQ  : 0;
	}

	env_freq=getenv("OMPI_MCA_gmpi_freq");
	if(env_freq && strlen(env_freq) > 0){
		g_freq = atoi(env_freq);	//FIXME make this a little more idiotproof.
	}

	env_trace=getenv("OMPI_MCA_gmpi_trace");
	if(env_trace && strlen(env_trace) > 0){
		g_trace &= strstr(env_trace, "none"    ) ? trace_NONE  	: ~trace_NONE;
		g_trace |= strstr(env_trace, "all"     ) ? trace_ALL   	: 0;
		g_trace |= strstr(env_trace, "ts"      ) ? trace_TS    	: 0;
		g_trace |= strstr(env_trace, "file"    ) ? trace_FILE  	: 0;
		g_trace |= strstr(env_trace, "line"    ) ? trace_LINE  	: 0;
		g_trace |= strstr(env_trace, "fn"      ) ? trace_FN    	: 0;
		g_trace |= strstr(env_trace, "comp"    ) ? trace_COMP  	: 0;
		g_trace |= strstr(env_trace, "comm"    ) ? trace_COMM  	: 0;
		g_trace |= strstr(env_trace, "rank"    ) ? trace_RANK  	: 0;
		g_trace |= strstr(env_trace, "pcontrol") ? trace_PCONTROL: 0;
		fprintf(stdout,"g_trace=%s %d\n", env_trace, g_trace);
	}

	// Put a reasonable value in.
	gettimeofday(&ts_start_computation, NULL);  
	gettimeofday(&ts_stop_computation, NULL);  
}

static void
post_MPI_Init( union shim_parameters *p ){
	p=p;
	// Now that PMPI_Init has ostensibly succeeded, grab the rank & size.
	PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
	PMPI_Comm_size(MPI_COMM_WORLD, &size);
	// Fire up the logfile.
	logfile = initialize_logfile( rank );
}
	
////////////////////////////////////////////////////////////////////////////////
// Finalize
////////////////////////////////////////////////////////////////////////////////
static void
pre_MPI_Finalize( union shim_parameters *p ){
	p=p;
}
	
static void
post_MPI_Finalize( union shim_parameters *p ){
	p=p;
	fclose(logfile);
}

////////////////////////////////////////////////////////////////////////////////
// Utilities
////////////////////////////////////////////////////////////////////////////////
char*
f2str( int shim_id ){
	char *str;
	switch(shim_id){
#include "shim_str.h"
		default:  str = "Unknown"; break;
	}
	return str;
}

void
Log( int shim_id, union shim_parameters *p ){

	static int initialized=0;
	MPI_Aint lb; 
	MPI_Aint extent;
	int MsgSz=-1, hash=hash_backtrace(shim_id);
	char *var_format ="%5d %20s %06d %9.6lf %9.6lf %7d\n";
	char *hdr_format="%5s %20s %6s %9s %9s %7s\n";


	// One-time initialization.
	if(!initialized){
		//Write the header line.
		if(rank==0){
			fprintf(logfile, 
				hdr_format,
				" Rank", "Function", "Hash", "Comp", "Comm", "MsgSz");
		}else{
			fprintf(logfile, 
				hdr_format,
				"#Rank", "Function", "Hash", "Comp", "Comm", "MsgSz");
		}
		initialized=1;
	}

	// Determine message size for selected function calls.
	switch(shim_id){
		case GMPI_ISEND: 
			PMPI_Type_get_extent( p->MPI_Isend_p.datatype, &lb, &extent ); 
			MsgSz = p->MPI_Isend_p.count * extent;
			break;
		case GMPI_IRECV: 
			PMPI_Type_get_extent( p->MPI_Irecv_p.datatype, &lb, &extent );
			MsgSz = p->MPI_Irecv_p.count * extent;
			break;
		case GMPI_SEND:
			PMPI_Type_get_extent( p->MPI_Send_p.datatype, &lb, &extent );
			MsgSz = p->MPI_Send_p.count * extent;
			break;
		case GMPI_RECV:
			PMPI_Type_get_extent( p->MPI_Recv_p.datatype, &lb, &extent );
			MsgSz = p->MPI_Recv_p.count * extent;
			break;
		case GMPI_SSEND: 
			PMPI_Type_get_extent( p->MPI_Ssend_p.datatype, &lb, &extent ); 
			MsgSz = p->MPI_Ssend_p.count * extent;
			break;
		case GMPI_REDUCE:
			PMPI_Type_get_extent( p->MPI_Reduce_p.datatype, &lb, &extent ); 
			MsgSz = p->MPI_Reduce_p.count * extent;
			break;
		case GMPI_ALLREDUCE:
			PMPI_Type_get_extent( p->MPI_Allreduce_p.datatype, &lb, &extent ); 
			MsgSz = p->MPI_Allreduce_p.count * extent;
			break;
			
		default:
			MsgSz = -1;	// We don't have complete coverage, obviously.
	}
			
	// Write to the logfile.
	fprintf( logfile,
		var_format,
		rank,
		f2str(p->MPI_Dummy_p.shim_id),	// Function
		//f2str(shim_id),				// Function
		hash,
		delta_seconds(&ts_start_computation,   &ts_stop_computation),
		delta_seconds(&ts_start_communication, &ts_stop_communication),
		MsgSz
	);

#ifdef USE_EAGER_LOGGING
	fflush( logfile );
#endif
}


////////////////////////////////////////////////////////////////////////////////
// Interception redirect.
////////////////////////////////////////////////////////////////////////////////
void 
shim_pre( int shim_id, union shim_parameters *p ){
	// Bookkeeping.
	gettimeofday(&ts_stop_computation, NULL);  
	
	// Function-specific intercept code.
	if(shim_id == GMPI_INIT){ pre_MPI_Init( p ); }
	if(shim_id == GMPI_FINALIZE){ pre_MPI_Finalize( p ); }
	
	// Bookkeeping.
	gettimeofday(&ts_start_communication, NULL);  
}

void 
shim_post( int shim_id, union shim_parameters *p ){
	// Bookkeeping.
	gettimeofday(&ts_stop_communication, NULL);  

	// (Most) Function-specific intercept code.
	if(shim_id == GMPI_INIT){ post_MPI_Init( p ); }
	
	// Logging occurs as we're about to leave the task.
	if(g_trace){ Log( shim_id, p ); };
	
	// Bookkeeping.  MUST COME AFTER LOGGING.
	gettimeofday(&ts_start_computation, NULL);  

	// NOTE:  THIS HAS TO GO LAST.  Otherwise the logfile will be closed
	// prematurely.
	if(shim_id == GMPI_FINALIZE){ pre_MPI_Finalize( p ); }
}


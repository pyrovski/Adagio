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
#include "wpapi.h"	// PAPI wrappers.
#include "shift.h"	// shift, enums.

static int rank, size;

static struct timeval ts_start_communication, ts_start_computation,
			ts_stop_communication, ts_stop_computation;

static int g_algo;	// which algorithm(s) to use.
static int g_freq;	// frequency to use with fixedfreq.
static int g_trace;	// tracing level.  

static int current_hash=0, previous_hash=-1, current_freq=0, next_freq=0;
static int in_computation=1;



FILE *logfile;

// Don't change this without altering the hash function.
static struct entry schedule[8192];
static double current_comp_seconds[NUM_FREQS];
static double current_comp_insn[NUM_FREQS];

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
	papi_start();	// Pretend computation started here.

}

static void
post_MPI_Init( union shim_parameters *p ){
	p=p;
	// Now that PMPI_Init has ostensibly succeeded, grab the rank & size.
	PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
	PMPI_Comm_size(MPI_COMM_WORLD, &size);
	// Fire up the logfile.
	if(g_trace){logfile = initialize_logfile( rank );}
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
	if(g_trace){fclose(logfile);}
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
	int MsgSz=-1;
	char *var_format =
		"%5d %20s %06d "			\
		" %9.6lf %9.6lf %9.6lf %9.6lf %9.6lf"	\
		" %9.6lf %7d\n";
	char *hdr_format =
		"%5s %20s %6s"				\ 
		" %9s %9s %9s %9s %9s"			\ 
		" %9s %7s\n";


	// One-time initialization.
	if(!initialized){
		//Write the header line.
		if(rank==0){
			fprintf(logfile, 
				hdr_format,
				" Rank", "Function", "Hash", 
				"Comp0", "Comp1", "Comp2", "Comp3", "Comp4",
				"Comm", "MsgSz");
		}else{
			fprintf(logfile, 
				hdr_format,
				"#Rank", "Function", "Hash", 
				"Comp0", "Comp1", "Comp2", "Comp3", "Comp4",
				"Comm", "MsgSz");
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
		f2str(p->MPI_Dummy_p.shim_id),	
		current_hash,
		schedule[current_hash].observed_comp_seconds[0],
		schedule[current_hash].observed_comp_seconds[1],
		schedule[current_hash].observed_comp_seconds[2],
		schedule[current_hash].observed_comp_seconds[3],
		schedule[current_hash].observed_comp_seconds[4],
		schedule[current_hash].observed_comm_seconds,
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
	in_computation = 0;
	gettimeofday(&ts_stop_computation, NULL);  

	// Which call is this?
	current_hash=hash_backtrace(shim_id);
	
	// Function-specific intercept code.
	if(shim_id == GMPI_INIT){ pre_MPI_Init( p ); }
	if(shim_id == GMPI_FINALIZE){ pre_MPI_Finalize( p ); }
	
	// Bookkeeping.
	gettimeofday(&ts_start_communication, NULL);  
	current_comp_insn[current_freq]=papi_stop();
}

void 
shim_post( int shim_id, union shim_parameters *p ){
	// Bookkeeping.
	gettimeofday(&ts_stop_communication, NULL);  

	// (Most) Function-specific intercept code.
	if(shim_id == GMPI_INIT){ post_MPI_Init( p ); }
	
	// Write the schedule entry.  MUST COME BEFORE LOGGING.
	memcpy(	// Copy time accrued before we shifted into current freq.
		schedule[current_hash].observed_comp_seconds, 
		current_comp_seconds, 
		sizeof( double ) * NUM_FREQS);
	memset(
		current_comp_seconds,
		0,
		sizeof( double ) * NUM_FREQS);

	memcpy( // Copy insn accrued before we shifted into current freq.
		schedule[current_hash].observed_comp_insn,
		current_comp_insn,
		sizeof( double ) * NUM_FREQS);
	memset(
		current_comp_insn,
		0,
		sizeof( double ) * NUM_FREQS);

	schedule[current_hash].observed_comp_seconds[current_freq] = 
		delta_seconds(&ts_start_computation, &ts_stop_computation);
	schedule[current_hash].observed_comm_seconds = 
		delta_seconds(&ts_start_communication, &ts_stop_communication);
	if( previous_hash >= 0 ){
		schedule[previous_hash].following_entry = current_hash;
	}

	// Logging occurs as we're about to leave the task.
	if(g_trace){ Log( shim_id, p ); };
	
	// Bookkeeping.  MUST COME AFTER LOGGING.
	previous_hash = current_hash;
	gettimeofday(&ts_start_computation, NULL);  
	papi_start();	//Computation.

	// NOTE:  THIS HAS TO GO LAST.  Otherwise the logfile will be closed
	// prematurely.
	if(shim_id == GMPI_FINALIZE){ pre_MPI_Finalize( p ); }
	in_computation = 1;
}

////////////////////////////////////////////////////////////////////////////////
// Signals
////////////////////////////////////////////////////////////////////////////////
static struct itimerval itv;
static void signal_handler(int signal);
static void initialize_handler(void);
static void
signal_handler(int signal){
	// Keep this really simple and stupid.  
	// When we catch a signal, shift(next_freq).
	// If we're in the computation phase, record time spent computing.
        signal = signal;
	if(in_computation){
		gettimeofday(&ts_stop_computation, NULL);
		current_comp_seconds[current_freq] 
			= delta_seconds(&ts_start_computation, &ts_stop_computation);
		current_comp_insn[current_freq]=stop_papi();
	}
	shift(next_freq);
	current_freq = next_freq;
	next_freq = NO_SHIFT;
	if(in_computation){
		gettimeofday(&ts_start_computation, NULL);
	}
	start_papi();
}



/* -*- Mode: C; tab-width: 2; c-basic-offset: 2 -*- */
/* shim.c
 *
 * The two global functions here, shim_pre() and shim_post, are called from the
 * shim_functions file when an MPI call is intercepted.
 */
#include <stdio.h>
#include <stdlib.h>	// getenv
#include <string.h>	// strlen, strstr
#include <math.h>
#include <numa.h>
#include <sys/utsname.h>
#include <stdint.h>
#include <assert.h>
#include "shim_enumeration.h"
#include "shim.h"
#include "gettimeofday_helpers.h"
#include "stacktrace.h"
#include "log.h"
#include "wpapi.h"	// PAPI wrappers.
#include "shift.h"	// shift, enums.  
#include "affinity.h"	// Setting cpu affinity.
#include "shm.h"        // shared memory
#include "meters.h"     // power meters
#include "cpuid.h"

#define max(a,b) (a > b ? a : b)
#define min(a,b) (a < b ? a : b)

// MPI_Init
static void pre_MPI_Init 	( union shim_parameters *p );
static void post_MPI_Init	( union shim_parameters *p );
// MPI_Finalize
static void pre_MPI_Finalize 	( union shim_parameters *p );
static void post_MPI_Finalize	( union shim_parameters *p );

// Scheduling
static void schedule_communication	( int idx );
static void schedule_computation  	( int idx );
static void initialize_handler    	(void);
static void signal_handler        	( int signal);
static void set_alarm			( double s );


static int rank, size;

typedef struct {
	struct timeval start, stop;
	uint64_t aperf_start, aperf_stop, aperf_accum,
		mperf_start, mperf_stop, mperf_accum,
		tsc_start, tsc_stop, tsc_accum;

	double freq, ratio, elapsed_time;
} timing_t;

static timing_t time_comp, time_comm, time_total;

static int g_algo;	// which algorithm(s) to use.
static int g_freq;	// frequency to use with fixedfreq.  
static int g_trace;	// tracing level.  

static int current_hash=0, previous_hash=-1, next_freq=1;

/*! @todo FASTEST_FREQ, turboboost_present, current_freq, and next_freq
	are initialized assuming turboboost is present.
 */
int current_freq=1;

static int in_computation=1;
static int MPI_Initialized_Already=0;

double frequencies[MAX_NUM_FREQUENCIES], ratios[MAX_NUM_FREQUENCIES];

#define GMPI_MIN_COMP_SECONDS (0.1)     // In seconds.
#define GMPI_MIN_COMM_SECONDS (0.1)     // In seconds.
#define GMPI_BLOCKING_BUFFER (0.1)      // In seconds.

FILE *logfile = NULL;

// Don't change this without altering the hash function.
//! @todo change hash function?
static struct entry schedule[8192];

static double current_comp_seconds;
static double current_comp_insn;
static double current_start_time;

enum{ 
	// To run without library overhead, use the .pristine binary.
	algo_NONE	      = 0x000,	// Identical to CLEAN.
	algo_FERMATA 	  = 0x001,	// slow communication
	algo_ANDANTE 	  = 0x002,	// slow comp before global sync pts.
	algo_ADAGIO  	  = 0x004,	// fermata + andante
	algo_ALLEGRO 	  = 0x008,	// slow computation everywhere.
	algo_FIXEDFREQ	= 0x010,	// run whole program at single freq.
	algo_JITTER  	  = 0x020,	// per ncsu.
	algo_MISER   	  = 0x040,	// per vt.
	algo_CLEAN   	  = 0x080,	// Identical to fixedfreq=0.
	mods_FAKEJOULES = 0x100,	// Pretend power meter present.
	mods_FAKEFREQ   = 0x200,	// Pretend to change the cpu frequency.
	mods_BIGCOMM    = 0x400,	// Barrier before MPI_Alltoall.
	mods_TURBOBOOST = 0x800,  // allow turboboost

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

static inline uint64_t rdtsc(void)
{
	// use cpuid instruction to serialize
	asm volatile ("xorl %%eax,%%eax \n cpuid"
								::: "%rax", "%rbx", "%rcx", "%rdx");
	// compiler should eliminate one code path
  if (sizeof(long) == sizeof(uint64_t)) {
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)(hi) << 32) | lo;
  }
  else {
    uint64_t tsc;
    asm volatile("rdtsc" : "=A" (tsc));
    return tsc;
  }
}

static FILE *perf_file = 0;

static inline void read_aperf_mperf(uint64_t *aperf, uint64_t *mperf){
	uint64_t mperf_aperf[2];
	if(!perf_file){
		perf_file = fopen("/proc/mperf_aperf", "r");
		assert(perf_file);
	}
	
	fread(mperf_aperf, sizeof(uint64_t) * 2, 1, perf_file);
	if(mperf)
		*mperf = mperf_aperf[0];
	if(aperf)
		*aperf = mperf_aperf[1];
	/*
	status = fseek(perf_file, 0, SEEK_SET);
	assert(!status);
	*/
	fclose(perf_file);
	perf_file = 0;
	/*
#ifdef _DEBUG
	printf("core %d aperf: 0x%llx mperf: 0x%llx\n", my_core, 
				 mperf_aperf[1], mperf_aperf[0]);
#endif
	*/
}

/*!
	@todo calculate aperf/mperf ratio and corresponding frequency, elapsed time
	@todo deal with counter overflow; see turbostat code
 */
static void calc_rates(timing_t *t){
	assert(t);

	if(t->aperf_stop <= t->aperf_start)
		printf("aperf went backwards?\n");
	if(t->mperf_stop <= t->mperf_start)
		printf("mperf went backwards?\n");
	if(t->tsc_stop <= t->tsc_start)
		printf("tsc went backwards?\n");

	t->aperf_accum += t->aperf_stop - t->aperf_start;
	t->mperf_accum += t->mperf_stop - t->mperf_start;
	t->tsc_accum += t->tsc_stop - t->tsc_start;
	t->elapsed_time += delta_seconds(&t->start, &t->stop);

	t->ratio = (double)t->aperf_accum / t->mperf_accum;
	//t->elapsed_time = tsc_delta / 2667000000;
	t->freq = t->ratio * t->tsc_accum / t->elapsed_time;
	//t->freq = t->ratio * 2667000000;
}

/*! start = 1, stop = 0 
	 @todo don't need gettimeofday if tsc rate is constant.
 */
static inline void mark_time(timing_t *t, int start_stop){
	if(start_stop){
#ifdef _DEBUG
		printf("rank %d timing start 0x%lx\n", rank, t);
#endif
		gettimeofday(&t->start, 0);
		t->tsc_start = rdtsc();
		read_aperf_mperf(&t->aperf_start, &t->mperf_start);
	} else {
		/*! @todo this should be cumulative; also need a reset.
			alternatively, keep two entries for each interrupted task.
			elapsed_time should be initialized to zero and added to on each stop.
		 */
		gettimeofday(&t->stop, 0);
		t->tsc_stop = rdtsc();
		read_aperf_mperf(&t->aperf_stop, &t->mperf_stop);
		calc_rates(t);
#ifdef _DEBUG
		printf("rank %d timing stop 0x%lx delta: %11.7f ratio: %f min ratio: %f f: %f GHz tsc f: %f GHz aperf: 0x%lx mperf: 0x%lx\n", 
					 rank, t, 
					 t->elapsed_time, t->ratio, 
					 ratios[current_freq],
					 t->freq / 1000000000,
					 time_comp.tsc_accum / time_comp.elapsed_time / 1000000000,
					 time_comp.aperf_stop,
					 time_comp.mperf_stop);
#endif
	}
}

static inline void clear_time(timing_t *t){
	memset(t, 0, sizeof(timing_t));
}

////////////////////////////////////////////////////////////////////////////////
// Init
////////////////////////////////////////////////////////////////////////////////
static void
pre_MPI_Init( union shim_parameters *p ){

	struct utsname utsname;	// Holds hostname.
	char  *hostname;
	char *env_algo, *env_trace, *env_freq, *env_badnode, *env_mods;
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
		g_algo |= strstr(env_algo, "adagio"    ) ? algo_FERMATA | algo_ANDANTE    : 0;
		g_algo |= strstr(env_algo, "allegro"   ) ? algo_ALLEGRO   : 0;
		g_algo |= strstr(env_algo, "fixedfreq" ) ? algo_FIXEDFREQ : 0;
		g_algo |= strstr(env_algo, "jitter"    ) ? algo_JITTER    : 0;
		g_algo |= strstr(env_algo, "miser"     ) ? algo_MISER     : 0;
		g_algo |= strstr(env_algo, "clean"     ) ? algo_CLEAN     : 0;
#ifdef _DEBUG
		printf("g_algo=%s %d\n", env_algo, g_algo);
#endif
	} else {
#ifdef _DEBUG
	  printf("g_algo empty\n");
#endif
	}
	env_mods=getenv("OMPI_MCA_gmpi_mods");
	if(env_mods && strlen(env_mods) > 0){
		g_algo |= strstr(env_mods, "fakejoules") ? mods_FAKEJOULES: 0;
		g_algo |= strstr(env_mods, "bigcomm"   ) ? mods_BIGCOMM   : 0;
		g_algo |= strstr(env_mods, "turboboost") ? mods_TURBOBOOST: 0;
	}

	env_freq=getenv("OMPI_MCA_gmpi_freq");
	if(env_freq && strlen(env_freq) > 0){
		g_freq = atoi(env_freq);	//FIXME make this a little more idiotproof.
#ifdef _DEBUG
		printf("g_freq=%s %d\n", env_freq, g_freq);
#endif
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
#ifdef _DEBUG
		g_trace = trace_ALL;
		printf("g_trace=%s %d\n", env_trace, g_trace);
#endif
	}

	env_badnode=getenv("OMPI_MCA_gmpi_badnode");
	uname(&utsname);
	hostname = utsname.nodename;
	if(env_badnode && strlen(env_badnode) > 0){
		if( strstr( env_badnode, hostname ) ){
			g_algo |= mods_FAKEFREQ;
		}
	}
	
	// Put a reasonable value in.
	clear_time(&time_comp);
	clear_time(&time_total);
	mark_time(&time_comp, 1);
	mark_time(&time_comp, 0);
	mark_time(&time_total, 1);
	current_start_time = time_total.start.tv_sec + 
		time_total.start.tv_usec / 1000000.0;
	
	// initialize mcsup
	parse_proc_cpuinfo();

	// get list of available frequencies
	// assume all processors support the same options
	shift_parse_freqs();
	
	if(g_algo & mods_TURBOBOOST)
		if(!turboboost_present){
			printf("error: turboboost requested without turboboost support");
			exit(1);
		}

	// To bound miser, we assume top gear is 1 instead of 0.
	// We also allow fermata to be used.
	if(g_algo & algo_MISER){
		g_algo |= algo_FERMATA;
		FASTEST_FREQ = 1 + turboboost_present;
	} else {
		if(turboboost_present){
			if(g_algo & mods_TURBOBOOST)
				FASTEST_FREQ = 0;
			else
				FASTEST_FREQ = 1;
		} else
			FASTEST_FREQ = 0;
	}
	{
		int i;
		for(i = 0; i < NUM_FREQS; i++)
			ratios[i] = frequencies[i] / frequencies[FASTEST_FREQ];
	}

	// Pretend computation started here.
	start_papi();	
	
	// Set up signal handling.
	initialize_handler();

	MPI_Initialized_Already=1;

}

static void
post_MPI_Init( union shim_parameters *p ){
	p=p;
	// Now that PMPI_Init has ostensibly succeeded, grab the rank & size.
	PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
	PMPI_Comm_size(MPI_COMM_WORLD, &size);
	
	// Set CPU affinity and memory preference.
	//! this needs to be done by MPI or srun
	//set_cpu_affinity( rank );
	numa_set_localalloc();
	
	/*! setup shared memory and interprocess semaphore
	 */
	shm_setup(*((struct MPI_Init_p*)p)->argv, rank);

	// Start us in a known frequency.
	shift_core(my_core, FASTEST_FREQ);
	
	// Fire up the logfile.
	if(g_trace){logfile = initialize_logfile( rank );}
	mark_joules(rank, size);
}
	
////////////////////////////////////////////////////////////////////////////////
// Finalize
////////////////////////////////////////////////////////////////////////////////
static void
pre_MPI_Finalize( union shim_parameters *p ){
	p=p;
	mark_joules(rank, size);
	PMPI_Barrier( MPI_COMM_WORLD );
	mark_time(&time_total, 0);
	// Leave us in a known frequency.  
	// This should always be the fastest available.
	//! @todo shift socket
	shift_core(my_core, FASTEST_FREQ);
	shm_teardown();
}
	
static void
post_MPI_Finalize( union shim_parameters *p ){
	p=p;
	if(g_trace){fclose(logfile);}
}

////////////////////////////////////////////////////////////////////////////////
// Logging
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

//! @todo represent time since program start in each log entry
void
Log( int shim_id, union shim_parameters *p ){

	static int initialized=0;
	MPI_Aint lb; 
	MPI_Aint extent;
	int MsgSz=-1;

	char var_format[] = "%5d %13s %06d %9.6lf %9.6f %9.6lf %9.6f %9.6lf %9.6lf %8.6lf %7d\n";
	char hdr_format[] = "%4s %13s %6s %9s %9s %9s %9s %9s %9s %8s %7s\n";

	// One-time initialization.
	if(!initialized){
		//Write the header line.
		if(rank==0){
			fprintf(logfile, " ");
		}else{
			fprintf(logfile, "#");
		}
		fprintf(logfile, hdr_format,
						"Rank", "Function", "Hash", 
						"Time_in", "Time_out",
						"Comp", "Ratio", "GHz", 
						"T_Ratio",
						"Comm", "MsgSz");		
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
	
	//! @todo also log desired ratio/freq
	// Write to the logfile.
	fprintf(logfile, var_format, rank, 
					f2str(p->MPI_Dummy_p.shim_id),	
					current_hash,
					schedule[current_hash].start_time - 
					(time_total.start.tv_sec + time_total.start.tv_usec / 1000000.0),
					schedule[current_hash].end_time - 
					(time_total.start.tv_sec + time_total.start.tv_usec / 1000000.0),
					schedule[current_hash].observed_comp_seconds,
					schedule[current_hash].observed_ratio,
					schedule[current_hash].observed_freq / 1000000000.0,
					schedule[current_hash].desired_ratio == 0.0 ? 1.0 : 
					schedule[current_hash].desired_ratio,
					schedule[current_hash].observed_comm_seconds,
					MsgSz);

#ifdef BLR_USE_EAGER_LOGGING
	fflush( logfile );
#endif
	/*! @todo add up interval times, compare to total program time,
		defined as time between MPI_Init and MPI_Finalize
	 */
}


////////////////////////////////////////////////////////////////////////////////
// Interception redirect.
////////////////////////////////////////////////////////////////////////////////
void 
shim_pre( int shim_id, union shim_parameters *p ){
	// ft kludge.
	// In the normal case, any MPI call on or close to the critical path
	// will take less than GMPI_BLOCKING_BUFFER.  ft messages are so large,
	// though, that it takes ~10 seconds to complete an MPI_Alltoall on 32
	// nodes.  This makes it look like every process than slow down -- 
	// there's plenty of communiation time -- which inevitably leads to 
	// the process(es) on the critical path slowing down.
	//
	// If we had a reliable way to distinguish blocking time from 
	// communication time, we might be able to work with this.  However, 
	// I don't think that even a complex scheme will hold up over 32 
	// processes trying to communicate with 31 other processes, each
	// starting at a slightly different time.
	//
	// To fix the problem, we're going to stick a barrier in front of the
	// MPI_Alltoall call.  This will do two things:
	//
	// 1)  The communication time for the barrier is much less than 
	// GMPI_BLOCKING_BUFFER.  Any extra time spent here is real blocking
	// time, and computation can be slowed to take advantage of this.
	// Processes on the critical path will see very little communication
	// time and no blocking time, and thus their computation will not
	// be slowed.
	// 
	// 2)  The task preceding the MPI_Alltoall will take to little time
	// to be scheduled, leaving the fermata algorithm to kick in to 
	// pick up the energy savings.
	
	// DO NOT MOVE THIS CALL.  This code isn't particularly reentrant,
	// so this needs to be executed before any bookkeeping occurs.
	if( (g_algo  &  algo_ANDANTE || g_algo & algo_FERMATA) 
	&&  (g_algo  &  mods_BIGCOMM) 
	&&  (shim_id == GMPI_ALLTOALL)
	  ) { MPI_Barrier( MPI_COMM_WORLD ); }	//NOT PMPI.  
	
	// Kill the timer.
	set_alarm(0.0);

	// Bookkeeping.
	in_computation = 0;
	mark_time(&time_comp, 0);

	// Which call is this?
	current_hash=hash_backtrace(shim_id);
	
	// Function-specific intercept code.
	if(shim_id == GMPI_INIT){ pre_MPI_Init( p ); }
	if(shim_id == GMPI_FINALIZE){ pre_MPI_Finalize( p ); }
	
	// If we aren't initialized yet, stop here.
	if(!MPI_Initialized_Already){ return; }
	
	// Bookkeeping.
	//!  this count may correspond to multiple frequencies
	current_comp_insn=stop_papi();
	
	// Write the schedule entry.  MUST COME BEFORE LOGGING.
	schedule[current_hash].observed_freq = time_comp.freq;
	schedule[current_hash].observed_ratio = time_comp.ratio;
#ifdef _DEBUG
	printf("rank %d set comp r: %f f: %f GHz tsc f: %f GHz tsc: %llu aperf: %lu "
				 "mperf: %lu s: %f\n", 
				 rank,
				 time_comp.ratio,
				 time_comp.freq / 1000000000,
				 time_comp.tsc_accum / time_comp.elapsed_time / 1000000000,
				 time_comp.tsc_accum,
				 time_comp.aperf_accum,
				 time_comp.mperf_accum,
				 time_comp.elapsed_time);
#endif
	// Copy time accrued before we shifted into current freq.
	// current_comp_seconds is set in the signal handler
	schedule[current_hash].observed_comp_seconds = current_comp_seconds;
	current_comp_seconds = 0;

	// Copy insn accrued before we shifted into current freq.
	schedule[current_hash].observed_comp_insn = current_comp_insn;
	current_comp_insn = 0;
	
	/*! @todo multiple assignment to observed_comp_seconds;
	 which is correct? */
	schedule[current_hash].observed_comp_seconds = time_comp.elapsed_time;

	// Schedule communication.
	clear_time(&time_comm);
	mark_time(&time_comm, 1);
	if( g_algo & algo_FERMATA ) { schedule_communication( current_hash ); }
}

void 
shim_post( int shim_id, union shim_parameters *p ){
	// If we aren't initialized yet, stop here.
	if(!MPI_Initialized_Already){ return; }
	
	// Kill the timer (may have been set by fermata algo).
	set_alarm(0.0);

	// Bookkeeping.
	mark_time(&time_comm, 0);
	//dump_timeval(logfile, "Communication halted. ", &ts_stop_communication);

	// (Most) Function-specific intercept code.
	if(shim_id == GMPI_INIT){ post_MPI_Init( p ); }
	
	schedule[current_hash].observed_comm_seconds = time_comm.elapsed_time;
	schedule[current_hash].start_time = current_start_time;
	schedule[current_hash].end_time = time_comm.stop.tv_sec + 
		time_comm.stop.tv_usec / 1000000.0;
	if( previous_hash >= 0 ){
		schedule[previous_hash].following_entry = current_hash;
	}

	// Logging occurs as we're about to leave the task.
	if(g_trace){ Log( shim_id, p ); };
	
	// Bookkeeping.  MUST COME AFTER LOGGING.
	previous_hash = current_hash;
	clear_time(&time_comp);
	mark_time(&time_comp, 1);
	current_start_time = time_comp.start.tv_sec + 
		time_comp.start.tv_usec / 1000000.0;
	//dump_timeval(logfile, "COMPUTATION started.  ", &ts_start_computation);
	start_papi();	//Computation.

	// Setup computation schedule.
	if(g_algo & algo_ANDANTE){
		schedule_computation( schedule[current_hash].following_entry );
	}else{
		current_freq = FASTEST_FREQ;	// Default case.
	}
	// Regardless of computation scheduling algorithm, always shift here.
	// (Most of the time it should have no effect.)
	if( ! (g_algo & mods_FAKEFREQ) ) { shift_core( my_core, current_freq ); }

	// NOTE:  THIS HAS TO GO LAST.  Otherwise the logfile will be closed
	// prematurely.
	if(shim_id == GMPI_FINALIZE){ post_MPI_Finalize( p ); }
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
	//fprintf( logfile, "++> SIGNAL HANDLER\n");
	if(in_computation){
		mark_time(&time_comp, 0);
		current_comp_insn=stop_papi();	                                   //  <---|
		current_comp_seconds = time_comp.elapsed_time;                          // |
	}                                                                         // |
                                                                            // |
	if( ! (g_algo & mods_FAKEFREQ) ) {                                        // |
	  shift_core( my_core, next_freq );                                       // |
	}                                                                         // |
#ifdef _DEBUG                                                               // |
	fprintf(logfile,                                                          // |
		"==> Signal handler shifting to %d, in_computation=%d\n",               // |
		next_freq, in_computation);                                             // |
#endif                                                                      // |
	current_freq = next_freq;                                                 // |
	next_freq = FASTEST_FREQ;                                                 // |
                                                                            // |
	if(in_computation){                                                       // |
		mark_time(&time_comp, 1);                                               // |
		start_papi();	                                                     //  <---|
	}
}

static void
initialize_handler(void){
        sigset_t sig;
        struct sigaction act;
        sigemptyset(&sig);
        act.sa_handler = signal_handler;
        act.sa_flags = SA_RESTART;
        act.sa_mask = sig;
        sigaction(SIGALRM, &act, NULL);
}

static void
set_alarm(double s){
	itv.it_value.tv_sec  = (suseconds_t)s;
	itv.it_value.tv_usec = (suseconds_t)( (s-floor(s) )*1000000.0);
	setitimer(ITIMER_REAL, &itv, NULL);
}

////////////////////////////////////////////////////////////////////////////////
// Schedules
////////////////////////////////////////////////////////////////////////////////

static void
schedule_communication( int idx ){
	// If we have no data to work with, go home.
	if( idx==0 ){ return; }

	// If we haven't measured communication time yet, or there isn't 
	// enough to bother with, go home.
	if( schedule[ idx ].observed_comm_seconds < 2*GMPI_MIN_COMM_SECONDS ){
		return;
	}

	// Ok, so we have a chunk of communication time to work with.
	// Set the timer to go off and return.
	next_freq = SLOWEST_FREQ;
	set_alarm( GMPI_MIN_COMM_SECONDS );
}

static void
schedule_computation( int idx ){
	int i; 
	double p=0.0, d=0.0, I=0.0, seconds_until_interrupt=0.0;

	// WARNING:  The actual shift to the value placed in current_freq
	// happens after this function returns.  The default case is to 
	// run at the highest frequency (0).  
	current_freq = FASTEST_FREQ;

#ifdef _DEBUG
	fprintf( logfile, "==> schedule_computation called with idx=%d\n", idx);
#endif
	// If we have no data to work with, go home.
	if( idx==0 ){ return; }

	schedule[idx].desired_ratio = 1.0;

	// On the first time through, establish worst-case slowdown rates.
	//! @todo fix for average frequency
	if( schedule[ idx ].seconds_per_insn == 0.0 ){
#ifdef _DEBUG
		fprintf( logfile, "==> schedule_computation First time through.\n");
#endif
	//! @todo fix for average frequency
		if( schedule[ idx ].observed_comp_seconds <= GMPI_MIN_COMP_SECONDS ){
#ifdef _DEBUG
			fprintf( logfile, "==> schedule_computation min_seconds violation.\n");
#endif
			return;
		}
		schedule[ idx ].seconds_per_insn = 
			schedule[ idx ].observed_comp_seconds /
			schedule[ idx ].observed_comp_insn;
		/*! @todo we should save the numbers from the first high freq run
		 */
#ifdef _DEBUG
		if(!isnan(schedule[idx].seconds_per_insn))
			fprintf(logfile, "&&& SPI = %16.15lf\n", 
							schedule[idx].seconds_per_insn);
#endif
	}

	// On subsequent execution, only update where we have data.
	//for( i=FASTEST_FREQ; i<NUM_FREQS; i++ ){
	if( schedule[ idx ].observed_comp_seconds > GMPI_MIN_COMP_SECONDS ){
			schedule[ idx ].seconds_per_insn = 
				schedule[ idx ].observed_comp_seconds/
				schedule[ idx ].observed_comp_insn;
		}
#ifdef _DEBUG
	if(!isnan(schedule[idx].seconds_per_insn))
		fprintf(logfile, "&&& SPI = %16.15lf\n", 
						schedule[idx].seconds_per_insn);
#endif
		//}

	// Given I instructions to be executed over d time using available
	// rates r[], 
	//
	// pIr[i] + (1-p)Ir[i+1] = d
	//
	// thus
	//
	// p = ( d - Ir[i+1] )/( Ir[i] - Ir[i+1] ) 
	
	d += schedule[ idx ].observed_comp_seconds;
	I += schedule[ idx ].observed_comp_insn;
	
	// If there's not enough computation to bother scaling, skip it.
	if( d <= GMPI_MIN_COMP_SECONDS ){
		//fprintf( logfile, "==> schedule_computation min_seconds total violation.\n");
		return;
	}

	// The deadline includes blocking time, minus a buffer.
	d += schedule[ idx ].observed_comm_seconds - GMPI_BLOCKING_BUFFER;

	// Create the schedule.
	schedule[idx].desired_ratio = 
		min(1.0, 
				schedule[idx].observed_comp_seconds / 
				(schedule[idx].observed_comm_seconds + 
				 schedule[idx].observed_comp_seconds - GMPI_BLOCKING_BUFFER));
				
	// If the fastest frequency isn't fast enough, use f0 all the time.
	//! @todo fix for average frequency measurement; need prediction
	if( I * schedule[ idx ].seconds_per_insn * 
			schedule[ idx ].observed_freq/frequencies[FASTEST_FREQ] >= d ){
#ifdef _DEBUG
		fprintf( logfile, "==> schedule_computation GO FASTEST.\n");
		fprintf( logfile, "==>     I=%lf\n", I);
		fprintf( logfile, "==>   SPI=%16.15lf\n",   
				schedule[ idx ].seconds_per_insn * 
						 schedule[idx].observed_freq / frequencies[FASTEST_FREQ]);
		fprintf( logfile, "==> I*SPI=%16.15lf\n", 
				I*schedule[ idx ].seconds_per_insn *
						 schedule[idx].observed_freq / frequencies[FASTEST_FREQ]);
		fprintf( logfile, "==>     d=%lf\n", d);
#endif
		current_freq = FASTEST_FREQ;
	}
	// If the slowest frequency isn't slow enough, use that.
	//! @todo fix for average frequency measurement; need prediction
	else if( I * schedule[ idx ].seconds_per_insn * 
					 schedule[ idx ].observed_freq/frequencies[ SLOWEST_FREQ ] <= d ){
#ifdef _DEBUG
		fprintf( logfile, "==> schedule_computation GO SLOWEST.\n");
		fprintf( logfile, "==>     I=%lf\n", I);
		fprintf( logfile, "==>   SPI=%16.15lf\n",   
				schedule[ idx ].seconds_per_insn * 
						 schedule[idx].observed_freq / frequencies[ SLOWEST_FREQ ]);
		fprintf( logfile, "==> I*SPI=%16.15lf\n", 
				I*schedule[ idx ].seconds_per_insn * 
						 schedule[idx].observed_freq / frequencies[ SLOWEST_FREQ ]);
		fprintf( logfile, "==>     d=%lf\n", d);
#endif
		current_freq = SLOWEST_FREQ;
	}
	// Find the slowest frequency that allows the work to be completed in time.
	else{
		for( i=SLOWEST_FREQ-1; i>FASTEST_FREQ; i-- ){
			if( I * schedule[ idx ].seconds_per_insn * 
					schedule[ idx ].observed_freq/frequencies[ i ] < d ){
				// We have a winner.
				//! @todo adjust for average frequency measurement
				p = 
					( d - I * schedule[ idx ].seconds_per_insn * 
						schedule[ idx ].observed_freq/frequencies[ i+1 ] )
					/
					( I * schedule[ idx ].seconds_per_insn * 
						schedule[ idx ].observed_freq/frequencies[ i ] - 
					  I * schedule[ idx ].seconds_per_insn * 
						schedule[ idx ].observed_freq/frequencies[ i+1 ] );

#ifdef _DEBUG
				fprintf( logfile, "==> schedule_computation GO %d.\n", i);
				fprintf( logfile, "----> SPI[%d] = %15.14lf\n", FASTEST_FREQ, 
								 schedule[idx].seconds_per_insn * 
								 schedule[idx].observed_freq / frequencies[FASTEST_FREQ]);
				fprintf( logfile, "----> SPI[%i] = %15.14lf\n", i, 
								 schedule[idx].seconds_per_insn *
								 schedule[idx].observed_freq / frequencies[i]);
				fprintf( logfile, "---->       d = %lf\n", d);
				fprintf( logfile, "---->       I = %lf\n", I);
				fprintf( logfile, "---->       p = %lf\n", p);
				fprintf( logfile, "---->     idx = %d\n", idx);
#endif
				current_freq = i;

				// Do we need to shift down partway through?
				if((    p * I * schedule[idx].seconds_per_insn * 
								schedule[ idx ].observed_freq/frequencies[i  ]>GMPI_MIN_COMP_SECONDS)
				&& (1.0-p * I * schedule[idx].seconds_per_insn * 
						schedule[ idx ].observed_freq/frequencies[i+1]>GMPI_MIN_COMP_SECONDS)
				){
					seconds_until_interrupt = 
						p * I * schedule[ idx ].seconds_per_insn * 
						schedule[ idx ].observed_freq/frequencies[ i ];
					next_freq = i+1;
#ifdef _DEBUG
					fprintf( logfile, "==> schedule_computation alarm=%15.14lf.\n", 
						 seconds_until_interrupt);
#endif
					set_alarm(seconds_until_interrupt);
				}
				break;
			}
		}
	}
}


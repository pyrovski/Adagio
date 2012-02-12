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
#include "shm.h"        // shared memory
#include "meters.h"     // power meters
#include "cpuid.h"

#define ARCH_SANDY_BRIDGE
#include "msr_rapl.h"
#include "msr_core.h"

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

// Logging
void Log( int shim_id, union shim_parameters *p );

int rank;
static int size;

static char rapl_filename[16];
FILE *rapl_file = 0;
static struct rapl_state_s rapl_state; 

typedef struct {
	struct timeval start, stop;
	uint64_t aperf_start, aperf_stop, aperf_accum,
		mperf_start, mperf_stop, mperf_accum,
		tsc_start, tsc_stop, tsc_accum;

	double freq, ratio, elapsed_time, 
		Pkg_joules, PP0_joules;
#ifdef ARCH_062D
	double DRAM_joules;
#endif
} timing_t;

static timing_t time_comp, time_comm, time_total;
static unsigned critical_path_fires = 0;

static int g_algo;	// which algorithm(s) to use.
static int g_freq;	// frequency to use with fixedfreq.  
int g_trace;	// tracing level.  
int g_bind;  // cpu binding
int g_cores_per_socket; // 
int my_core, my_socket, my_local;
int binding_stable = 0;

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
FILE *runTimeLog = 0;
FILE *runTimeStats = 0;

// Don't change this without altering the hash function.
//! @todo change hash function?
static struct entryHist schedule[8192];

//static double current_comp_seconds;
static double current_comp_insn;
//static double current_start_time;

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

/*! @todo perhaps there should be one perf file per core
 */
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
	calculate aperf/mperf ratio and corresponding frequency, elapsed time
	@todo deal with counter overflow; see turbostat code
 */
static void calc_rates(timing_t *t){
	assert(t);

	t->elapsed_time += delta_seconds(&t->start, &t->stop);

	if(t->aperf_stop <= t->aperf_start){
		// double-check core binding
		int tmp_core, tmp_socket, tmp_local;
		get_cpuid(&tmp_core, &tmp_socket, &tmp_local);
		printf("rank %d aperf went backwards 0x%lx to 0x%lx; core %d delta: %e\n", 
					 rank,
					 t->aperf_start, t->aperf_stop, tmp_core, t->elapsed_time);
		
	}
	if(t->mperf_stop <= t->mperf_start){
		int tmp_core, tmp_socket, tmp_local;
		get_cpuid(&tmp_core, &tmp_socket, &tmp_local);
		printf("rank %d mperf went backwards 0x%lx to 0x%lx; core %d delta: %e\n", 
					 rank,
					 t->mperf_start, t->mperf_stop, tmp_core, t->elapsed_time);
	}
	if(t->tsc_stop <= t->tsc_start)
		printf("rank %d tsc went backwards?\n", rank);

	t->aperf_accum += t->aperf_stop - t->aperf_start;
	t->mperf_accum += t->mperf_stop - t->mperf_start;
	t->tsc_accum += t->tsc_stop - t->tsc_start;

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
#if _DEBUG > 1
		printf("rank %d timing start 0x%lx\n", rank, t);
#endif
		gettimeofday(&t->start, 0);
		t->tsc_start = rdtsc();
		read_aperf_mperf(&t->aperf_start, &t->mperf_start);
		//if(!socket_rank)
		get_all_status(my_socket, &rapl_state);
	} else { // stop
		/*
			elapsed_time should be initialized to zero and added to on each stop.
		 */
		gettimeofday(&t->stop, 0);
		t->tsc_stop = rdtsc();
		read_aperf_mperf(&t->aperf_stop, &t->mperf_stop);
		calc_rates(t);
		//if(!socket_rank)
		//!	joules counter only updates once every 0.0009765625s.
		if(t->elapsed_time >= 2 * 0.0009765625){
			get_all_status(my_socket, &rapl_state);
			t->Pkg_joules = rapl_state.energy_status[my_socket][PKG_DOMAIN];
			t->PP0_joules = rapl_state.energy_status[my_socket][PP0_DOMAIN];
#ifdef ARCH_062D
			t->DRAM_joules = rapl_state.energy_status[my_socket][DRAM_DOMAIN];
#endif
		}
#if _DEBUG > 1
		printf("rank %d timing stop 0x%lx delta: %11.7f ratio: %f min ratio: %f "
					 "hash: %06d f: %f GHz tsc f: %f GHz aperf: 0x%lx mperf: 0x%lx\n", 
					 rank, t, 
					 t->elapsed_time, t->ratio, 
					 ratios[current_freq],
					 current_hash,
					 t->freq / 1000000000,
					 time_comp.tsc_accum / time_comp.elapsed_time / 1000000000,
					 time_comp.aperf_stop,
					 time_comp.mperf_stop);
#endif
#ifdef _DEBUG
		if(ratios[current_freq] > 0 && t->ratio < 0.95 * ratios[current_freq] && t->elapsed_time > GMPI_MIN_COMP_SECONDS)
			printf("rank %d 0x%lx delta: %11.7f ratio %f < min ratio %f\n",
						 rank, t, t->elapsed_time, t->ratio, ratios[current_freq]);
		else if(ratios[current_freq] <= 0)
			printf("rank %d 0x%lx delta: %11.7f ratio: %f no target ratio defined\n",
						 rank, t, t->elapsed_time, t->ratio);
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
	char *env_algo, *env_trace, *env_freq, *env_badnode, *env_mods, *env_bind;
	p=p;

	// Using "-mca key value" on the command line, the environment variable
	// "OMPI_MCA_key" is set to "value".  mpirun doesn't check the validity
	// of these (yay!), so we end up with an easy way of controlling the 
	// runtime environment without confusing the application 

	env_algo=getenv("OMPI_MCA_gmpi_algo");
	if(env_algo && strlen(env_algo) > 0){
		g_algo |= strstr(env_algo, "fermata"   ) ? algo_FERMATA   : 0;
		g_algo |= strstr(env_algo, "andante"   ) ? algo_ANDANTE   : 0;
		g_algo |= strstr(env_algo, "adagio"    ) ? algo_FERMATA | algo_ANDANTE    : 0;
		g_algo |= strstr(env_algo, "allegro"   ) ? algo_ALLEGRO   : 0;
		g_algo |= strstr(env_algo, "fixedfreq" ) ? algo_FIXEDFREQ : 0;
		g_algo |= strstr(env_algo, "jitter"    ) ? algo_JITTER    : 0;
		g_algo |= strstr(env_algo, "miser"     ) ? algo_MISER     : 0;
		if(strstr(env_algo, "fixedfreq")){
			g_algo = algo_FIXEDFREQ;
			env_freq=getenv("OMPI_MCA_gmpi_freq");
			if(env_freq && strlen(env_freq) > 0){
				g_freq = atoi(env_freq);	//FIXME make this a little more idiotproof.
#ifdef _DEBUG
				printf("g_freq=%s %d\n", env_freq, g_freq);
#endif
			} else
				g_freq = 0;
		}
		else if(strstr(env_algo, "none") || strstr(env_algo, "clean")){
			g_algo = algo_FIXEDFREQ;
			g_freq = 0;
		}
		
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

	env_trace=getenv("OMPI_MCA_gmpi_trace");
	if(env_trace && strlen(env_trace) > 0){
		g_trace &= strstr(env_trace, "none"    ) ? trace_NONE  	: ~trace_NONE;
		g_trace |= strstr(env_trace, "all"     ) ? trace_ALL   	: 0;
		g_trace |= strstr(env_trace, "thresh"  ) ? trace_THRESH : 0;
		/*
		g_trace |= strstr(env_trace, "ts"      ) ? trace_TS    	: 0;
		g_trace |= strstr(env_trace, "file"    ) ? trace_FILE  	: 0;
		g_trace |= strstr(env_trace, "line"    ) ? trace_LINE  	: 0;
		g_trace |= strstr(env_trace, "fn"      ) ? trace_FN    	: 0;
		g_trace |= strstr(env_trace, "comp"    ) ? trace_COMP  	: 0;
		g_trace |= strstr(env_trace, "comm"    ) ? trace_COMM  	: 0;
		g_trace |= strstr(env_trace, "rank"    ) ? trace_RANK  	: 0;
		*/
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
	
	// initialize mcsup
	parse_proc_cpuinfo();

	env_bind = getenv("OMPI_MCA_gmpi_bind");
	if(env_bind && strlen(env_bind) > 0){
		g_bind |= strstr(env_bind, "collapse") ? bind_COLLAPSE : 0;
		if(strlen(env_bind) > strlen("collapse")){
			g_cores_per_socket = strtoul(env_bind + strlen("collapse"), 0, 0);
			if(g_cores_per_socket > config.cores_per_socket || 
				 g_cores_per_socket <= 0){
				printf("error: invalid gmpi_bind collapse: %s\n", env_bind);
				MPI_Abort(MPI_COMM_WORLD, 1);
			}
		} else
			g_cores_per_socket = 1;
	} else
		g_bind = 0;
	
#ifdef _DEBUG
	printf("g_algo=%s %d\n", env_algo, g_algo);
	printf("g_bind=%s %d\n", env_bind, g_bind);
	if(g_bind)
		printf("g_cores_per_socket: %d\n", g_cores_per_socket);
#endif
	
	memset(schedule, 0, sizeof(schedule));

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


	// dummy init; we don't keep these measurements
  //if(!socket_rank){
		init_msr();
		rapl_init(&rapl_state, 0, 0, 0, 0);
		//}

	MPI_Initialized_Already=1;

	// we won't use the data gathered between pre_MPI_Init and shim_pre
	start_papi();	
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
	if(!rank){
		runTimeLog = fopen("globalRunTime.dat", "w");
		assert(runTimeLog);
	}

	runTimeStats = initialize_global_logfile(rank);
		
	// Set up signal handling.
	initialize_handler();

	// Put a reasonable value in.
	/* from this point to the end of the function exists mostly to make 
		 the timing values for MPI_Init sensible.  We don't count any time before 
		 MPI_Init, which makes timing accounting more complicated.
	 */

	// clear timers
	clear_time(&time_comp);
	clear_time(&time_total);

	// clear rapl
	//! only one core per socket needs to do this
  //if(!socket_rank){
		rapl_filename[15] = 0;
		snprintf(rapl_filename, 14, "rapl.%04d.dat", rank);
		rapl_file = fopen(rapl_filename, "w");
		if(!rapl_file){
			perror("error opening rapl file");
			MPI_Abort(MPI_COMM_WORLD, 1);
		}
		rapl_init(&rapl_state, 0, 0, rapl_file, 0);
		//}
	
	mark_time(&time_total, 1);
	time_comp = time_total;
	mark_time(&time_comp, 0);
	ind(schedule[current_hash]).observed_comp_seconds = time_comp.elapsed_time;

	clear_time(&time_comm);
	mark_time(&time_comm, 1);
	mark_time(&time_comm, 0);

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
	rapl_finalize(&rapl_state);
	if(!rank){
		fprintf(runTimeLog, "%lf\n", time_total.elapsed_time);
		fprintf(runTimeStats, "rank\td_time\td_mperf\td_aperf\td_tsc\tratio\tcrit_path_fires\n");
	}

 	/*!
		log total mperf, aperf, tsc, elapsed time, ratio
	 */
	fprintf(runTimeStats, 
					"%d\t%le\t%llu\t%llu\t%llu\t%f\t%u\n", 
					rank, 
					time_total.elapsed_time,
					time_total.mperf_stop - time_total.mperf_start,
					time_total.aperf_stop - time_total.aperf_start,
					time_total.tsc_stop - time_total.tsc_start,
					time_total.ratio,
					critical_path_fires
					);
	// Leave us in a known frequency.  
	// This should always be the fastest available.
  if(!socket_rank){
    shift_init_socket(my_socket, "userspace", FASTEST_FREQ);
  }
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

void
Log( const char *fname, int MsgSz, int MsgDest, int MsgSrc){

	static int initialized=0;
	int log = 0;

	char var_format[] = 
		"%5d %13s %06d %9.6f %9.6f "
		"%9.6f %e %9.6f %9.6f %9f "
		"%9f %8.6f %9.6f %8.6f "
		 "%8.2e "// CompPkgJ
		 "%8.2e "// CommPkgJ
		 "%8.2e "// CompPP0J
		 "%8.2e "// CommPP0J
#ifdef ARCH_062D
		 "%9.2e "// CompDRAMJ
		 "%9.2e "// CommDRAMJ
#endif
		"%8d %7d %7d\n";
	char hdr_format[] = 
		"%4s %13s %6s %9s %9s "
		"%9s %12s %9s %9s %9s "
		"%9s %8s %7s %8s "
		"%8s "// CompPkgJ
		"%8s "// CommPkgJ
		"%8s "// CompPP0J
		"%8s "// CommPP0J
#ifdef ARCH_062D
		"%9s "// CompDRAMJ
		"%9s "// CommDRAMJ
#endif
		"%8s %7s %7s\n";

	// One-time initialization.
	if(!initialized){
		//Write the header line.
		if(rank==0){
			if(g_trace){
				fprintf(logfile, " ");
				fprintf(logfile, hdr_format,
								"Rank", "Function", "Hash", "Time_in", "Time_out",
								"Comp", "Insn", "Ratio", "T_Ratio", "ReqRatio", 
								"C0_Ratio", "Comm", "CommRatio", "CommC0", 
								"CompPkgJ", "CommPkgJ",
								"CompPP0J", "CommPP0J", 
#ifdef ARCH_062D
								"CompDRAMJ", "CommDRAMJ",
#endif
								"MsgSz", "MsgDest", "MsgSrc");
			}
		}
		initialized=1;
	}

	// Determine message size for selected function calls.
	/*! @todo in order to make a task graph, need to deal with tags, 
		communicators, MPI_Wait_any, and MPI_Wait_all.
	 */

	/*! @todo log filter 
		
	 */

	if((g_trace & trace_THRESH) && 
		 (ind(schedule[current_hash]).observed_comp_seconds >= GMPI_MIN_COMP_SECONDS ||
			ind(schedule[current_hash]).observed_comm_seconds >= GMPI_MIN_COMM_SECONDS))
		log = 1;
	
	if(shim_id == GMPI_PCONTROL && (g_trace & trace_PCONTROL))
		log = 1;

	if(g_trace & trace_ALL)
		log = 1;
	
	if(log){
		// Write to the logfile.
			fprintf(logfile, var_format, rank, 
							fname,	
							current_hash,
							ind(schedule[current_hash]).start_time - 
							(time_total.start.tv_sec + time_total.start.tv_usec / 1000000.0),
							ind(schedule[current_hash]).end_time - 
							(time_total.start.tv_sec + time_total.start.tv_usec / 1000000.0),
							ind(schedule[current_hash]).observed_comp_seconds,
							ind(schedule[current_hash]).observed_comp_insn,
							ind(schedule[current_hash]).observed_ratio,
							schedule[current_hash].desired_ratio == 0.0 ? 1.0 : 
							schedule[current_hash].desired_ratio,
							schedule[current_hash].requested_ratio,
							ind(schedule[current_hash]).c0_ratio,
							ind(schedule[current_hash]).observed_comm_seconds,
							ind(schedule[current_hash]).observed_comm_ratio,
							ind(schedule[current_hash]).observed_comm_c0,
							ind(schedule[current_hash]).comp_Pkg_joules,
							ind(schedule[current_hash]).comm_Pkg_joules,
							ind(schedule[current_hash]).comp_PP0_joules,
							ind(schedule[current_hash]).comm_PP0_joules,
#ifdef ARCH_062D
							ind(schedule[current_hash]).comp_DRAM_joules,
							ind(schedule[current_hash]).comm_DRAM_joules,
#endif
							MsgSz,
							MsgDest,
							MsgSrc);

#ifdef BLR_USE_EAGER_LOGGING
			fflush( logfile );
#endif
	}
	/*! @todo add up interval times, compare to total program time,
		defined as time between MPI_Init and MPI_Finalize
	 */
}


////////////////////////////////////////////////////////////////////////////////
// Interception redirect.
////////////////////////////////////////////////////////////////////////////////
/*
 ft kludge.
 In the normal case, any MPI call on or close to the critical path
 will take less than GMPI_BLOCKING_BUFFER.  ft messages are so large,
 though, that it takes ~10 seconds to complete an MPI_Alltoall on 32
 nodes.  This makes it look like every process than slow down -- 
 there's plenty of communiation time -- which inevitably leads to 
 the process(es) on the critical path slowing down.

 If we had a reliable way to distinguish blocking time from 
 communication time, we might be able to work with this.  However, 
 I don't think that even a complex scheme will hold up over 32 
 processes trying to communicate with 31 other processes, each
 starting at a slightly different time.

 To fix the problem, we're going to stick a barrier in front of the
 MPI_Alltoall call.  This will do two things:

 1)  The communication time for the barrier is much less than 
 GMPI_BLOCKING_BUFFER.  Any extra time spent here is real blocking
 time, and computation can be slowed to take advantage of this.
 Processes on the critical path will see very little communication
 time and no blocking time, and thus their computation will not
 be slowed.
 
 2)  The task preceding the MPI_Alltoall will take to little time
 to be scheduled, leaving the fermata algorithm to kick in to 
 pick up the energy savings.
*/

void shim_pre_1(){
	// DO NOT MOVE THIS CALL.  This code isn't particularly reentrant,
	// so this needs to be executed before any bookkeeping occurs.
	if( (g_algo  &  algo_ANDANTE || g_algo & algo_FERMATA) 
	&&  (g_algo  &  mods_BIGCOMM) 
	&&  (shim_id == GMPI_ALLTOALL)
	  ) { MPI_Barrier( MPI_COMM_WORLD ); }	//NOT PMPI.  
	
	// Kill the timer.
	set_alarm(0.0);
}

void shim_pre_2(){
	
	// If we aren't initialized yet, stop here.
	if(!MPI_Initialized_Already){ return; }

	// Bookkeeping.
	in_computation = 0;
	mark_time(&time_comp, 0);

	//!  this count may correspond to multiple frequencies
	current_comp_insn=stop_papi();

	// Which call is this?
	current_hash=hash_backtrace(shim_id, rank);

	// rotate schedule entry for this hash
	schedule[current_hash].index = 
		(schedule[current_hash].index + 1) % histEntries;
	
	// Write the schedule entry.  MUST COME BEFORE LOGGING.
	ind(schedule[current_hash]).observed_freq = time_comp.freq;
	ind(schedule[current_hash]).observed_ratio = time_comp.ratio;
	ind(schedule[current_hash]).c0_ratio = (double)time_comp.mperf_accum / time_comp.tsc_accum;
	ind(schedule[current_hash]).comp_Pkg_joules = time_comp.Pkg_joules;
	ind(schedule[current_hash]).comp_PP0_joules = time_comp.PP0_joules;
#ifdef ARCH_062D
	ind(schedule[current_hash]).comp_DRAM_joules = time_comp.DRAM_joules;
#endif
#if _DEBUG > 1
	printf("rank %d set comp r: %f tr: %f hash: %06d f: %f GHz tsc f: %f GHz tsc: %llu aperf: %lu "
				 "mperf: %lu s: %f\n", 
				 rank,
				 time_comp.ratio,
				 schedule[current_hash].desired_ratio,
				 current_hash,
				 time_comp.freq / 1000000000,
				 time_comp.tsc_accum / time_comp.elapsed_time / 1000000000,
				 time_comp.tsc_accum,
				 time_comp.aperf_accum,
				 time_comp.mperf_accum,
				 time_comp.elapsed_time);
#endif
	// Copy time accrued before we shifted into current freq.
	// current_comp_seconds is set in the signal handler
	//schedule[current_hash].observed_comp_seconds = current_comp_seconds;
	//current_comp_seconds = 0;

	// Copy insn accrued before we shifted into current freq.
	ind(schedule[current_hash]).observed_comp_insn = current_comp_insn;
	current_comp_insn = 0;
	
	/*! @todo multiple assignment to observed_comp_seconds;
	 which is correct? */
	ind(schedule[current_hash]).observed_comp_seconds = time_comp.elapsed_time;

	// Schedule communication.
	clear_time(&time_comm);
	mark_time(&time_comm, 1);
	if( g_algo & algo_FERMATA ) { schedule_communication( current_hash ); }
}

void shim_post_1(){
	// If we aren't initialized yet, stop here.
	if(!MPI_Initialized_Already){ return; }
	
	// Kill the timer (may have been set by fermata algo).
	set_alarm(0.0);

	// Bookkeeping.
	mark_time(&time_comm, 0);
	//dump_timeval(logfile, "Communication halted. ", &ts_stop_communication);
}

void shim_post_2(){
	ind(schedule[current_hash]).observed_comm_seconds = time_comm.elapsed_time;
	ind(schedule[current_hash]).start_time = time_comp.start.tv_sec + 
		time_comp.start.tv_usec / 1000000.0;
	ind(schedule[current_hash]).end_time = time_comm.stop.tv_sec + 
		time_comm.stop.tv_usec / 1000000.0;
	ind(schedule[current_hash]).observed_comm_ratio = time_comm.ratio;
	ind(schedule[current_hash]).observed_comm_c0 = 
		(double)time_comm.mperf_accum / time_comm.tsc_accum;
	ind(schedule[current_hash]).comm_Pkg_joules = time_comm.Pkg_joules;
	ind(schedule[current_hash]).comm_PP0_joules = time_comm.PP0_joules;
#ifdef ARCH_062D
	ind(schedule[current_hash]).comm_DRAM_joules = time_comm.DRAM_joules;
#endif
	if( previous_hash >= 0 ){
		//! @todo we can detect mispredictions here
		schedule[previous_hash].following_entry = current_hash;
	}
}

void 
shim_post_3(){	
	// Bookkeeping.  MUST COME AFTER LOGGING.
	previous_hash = current_hash;
	clear_time(&time_comp);
	mark_time(&time_comp, 1);
	/*
	current_start_time = time_comp.start.tv_sec + 
		time_comp.start.tv_usec / 1000000.0;
	*/
	//dump_timeval(logfile, "COMPUTATION started.  ", &ts_start_computation);
	start_papi();	//Computation.

	// Setup computation schedule.
	if(g_algo & algo_ANDANTE){
		/*! @todo call schedule_computation to do prediction even when adagio is off
			This will allow us to track target freqs without DVFS
		 */
		schedule_computation( schedule[current_hash].following_entry );
	}else{
		current_freq = FASTEST_FREQ;	// Default case.
	}

	// Regardless of computation scheduling algorithm, always shift here.
	// (Most of the time it should have no effect.)
	if( ! (g_algo & mods_FAKEFREQ) ) { shift_core( my_core, current_freq ); }

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
		//		current_comp_seconds = time_comp.elapsed_time;                          // |
	}                                                                         // |
                                                                            // |
	if( ! (g_algo & mods_FAKEFREQ) ) {                                        // |
	  shift_core( my_core, next_freq );                                       // |
	}                                                                         // |
#ifdef _DEBUG                                                               // |
	if(g_trace)
	fprintf(logfile,                                                          // |
		"#==> Signal handler shifting to %d, in_computation=%d\n",               // |
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
	if( indp(schedule[ idx ]).observed_comm_seconds < 2*GMPI_MIN_COMM_SECONDS ){
		return;
	}

	// Ok, so we have a chunk of communication time to work with.
	// Set the timer to go off and return.
	next_freq = SLOWEST_FREQ;
	set_alarm( GMPI_MIN_COMM_SECONDS );

	/*! @todo what is the intended outcome if there is not enough time to 
		slow down the clock?
	 */
}

/*!
	Examine history for this entry, and make a prediction for the next iteration.
	For now, assume min(previous three times).
*/
static double updateDeadlineComp(struct entryHist *eh){
	double minTime = __builtin_inf();
	int i;
	
	for(i = 0; i < histEntries; i++)
		// if we have data
		if(eh->hist[i].observed_comp_seconds)
			minTime = min(minTime, eh->hist[i].observed_comp_seconds);
	
	return minTime;
}

static double updateDeadline(struct entryHist *eh){
	double minTime = __builtin_inf();
	int i;
	
	for(i = 0; i < histEntries; i++)
		if(eh->hist[i].observed_comp_seconds)
			minTime = min(minTime, eh->hist[i].observed_comp_seconds + 
										eh->hist[i].observed_comm_seconds);
	
	return minTime;
}

/*! this is the core scheduling decision.
	If the last task execution experienced short comm time 
	(< comm threshold, indicating it was on the critical path), speed up. 
	Otherwise, adjust according to worst-case slowdown
 */
static double updateRatio(struct entryHist *eh, double deadline){
	double newRatio;
	
#ifdef detectCritical
	//! @todo look into MPI_Iprobe
	//! @todo this is good for global sync, but probably not much else
	if(indd(eh).observed_comm_seconds < GMPI_BLOCKING_BUFFER){
#ifdef _DEBUG
		if(g_trace)
			fprintf( logfile, "on critical path?\n");
#endif
		critical_path_fires++;
		newRatio = 1.0;
	} else
#endif // #ifdef detectCritical
		{
			// calculate observed effective frequency ratio
		
			/* for now, just base rate on previous entry
				 double oldRatio = 0.0, totalCompTime = 0.0;
				 int i;
		
				 for(i = 0; i < histEntries; i++)
				 totalCompTime += eh->hist[i].observed_comp_seconds;
		
				 for(i = 0; i < histEntries; i++)
				 oldRatio += (eh->hist[i].observed_comp_seconds / totalCompTime) * 
				 eh->hist[i].observed_ratio;
		
				 if(g_trace)
				 fprintf( logfile, "\n");

				 newRatio = (totalCompTime / histEntries * oldRatio) / 
				 (deadline * ratios[FASTEST_FREQ]);
			*/

			newRatio = (indd(eh).observed_comp_seconds * indd(eh).observed_ratio) /
				(deadline * ratios[FASTEST_FREQ]);
		}

	return(min(1.0, newRatio));
}

static void
schedule_computation( int idx ){
	int i; 
	double p=0.0, d, I, seconds_until_interrupt=0.0;

	// WARNING:  The actual shift to the value placed in current_freq
	// happens after this function returns.  The default case is to 
	// run at the highest frequency (0).  
	current_freq = FASTEST_FREQ;

#ifdef _DEBUG
	if(g_trace)
		fprintf( logfile, "#==> schedule_computation called with idx=%d\n", idx);
#endif
	// If we have no data to work with, go home.
	if( idx==0 ){ goto schedule_computation_exit; }

	// On the first time through, establish worst-case slowdown rates.
	//! @todo fix for average frequency
	//! @todo this should never be executed
	if( ind(schedule[ idx ]).observed_comp_seconds == 0.0  || 
			ind(schedule[ idx ]).observed_comp_insn == 0 ){
		printf("fixme %s:%d\n", __FILE__, __LINE__);
#ifdef _DEBUG
		if(g_trace)
			fprintf( logfile, "#==> schedule_computation First time through.\n");
#endif
	}

	// only update where we have data.
	if( ind(schedule[ idx ]).observed_comp_seconds > GMPI_MIN_COMP_SECONDS ){
		ind(schedule[ idx ]).seconds_per_insn = 
			ind(schedule[ idx ]).observed_comp_seconds/
			ind(schedule[ idx ]).observed_comp_insn;
	}
#ifdef _DEBUG
	if(!isnan(ind(schedule[idx]).seconds_per_insn))
			if(g_trace)
				fprintf(logfile, "#&&& SPI = %16.15lf\n", 
								ind(schedule[idx]).seconds_per_insn);
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
	
	//d = ind(schedule[ idx ]).observed_comp_seconds;
	d = updateDeadlineComp(&schedule[idx]);
#ifdef _DEBUG
	if(g_trace)
		fprintf( logfile, "hash %d comp deadline: %le\n", idx, d);
#endif

	I = ind(schedule[ idx ]).observed_comp_insn;
	
	// If there's not enough computation to bother scaling, skip it.
	if( d <= GMPI_MIN_COMP_SECONDS ){
		//fprintf( logfile, "==> schedule_computation min_seconds total violation.\n");
		goto schedule_computation_exit;
	}

	// The deadline includes blocking time, minus a buffer.
	//d += ind(schedule[ idx ]).observed_comm_seconds - GMPI_BLOCKING_BUFFER;
	d = updateDeadline(&schedule[idx]) - GMPI_BLOCKING_BUFFER;

#ifdef _DEBUG
	if(g_trace)
		fprintf( logfile, "hash %d deadline: %le\n", idx, d);
#endif

	// Create the schedule.
	/*! @todo base on previous instruction rate
	 */
	#warning this needs much verification
	schedule[idx].desired_ratio = updateRatio(&schedule[idx], d);

#ifdef _DEBUG
	if(g_trace)
		fprintf( logfile, "hash %d T_Ratio: %le\n", idx, schedule[idx].desired_ratio);
#endif

	// generate discrete index from ratio
	current_freq = SLOWEST_FREQ;
	for(i = SLOWEST_FREQ; i >= FASTEST_FREQ; i--)
		if(schedule[idx].desired_ratio > ratios[i])
			current_freq = max(i-1, FASTEST_FREQ);

#ifdef _DEBUG
	if(g_trace)
		fprintf( logfile, "hash %d ReqRatio: %le\n", idx, ratios[current_freq]);
#endif

	//! @todo use alarm to implement split freqs

	/*
		min(1.0, (schedule[idx].observed_comp_seconds * 
		schedule[idx].observed_freq) / 
		(d * frequencies[FASTEST_FREQ]));
	*/

	/*! @todo integrate into updateRatio
#if _DEBUG > 1
	printf("rank %d updating hash %06d target ratio from %f to %f; insn: %le comp: %f comm: %f\n",
				 rank, idx, schedule[idx].desired_ratio,
				 updatedRatio,
				 I,
				 schedule[idx].observed_comp_seconds, 
				 schedule[idx].observed_comm_seconds);
#endif	
	
	// If the fastest frequency isn't fast enough, use f0 all the time.
	//! @todo fix for average frequency measurement; need prediction
	if( I * ind(schedule[ idx ]).seconds_per_insn * 
			ind(schedule[ idx ]).observed_ratio >= d ){
#ifdef _DEBUG
		if(g_trace){
			fprintf( logfile, "#==> schedule_computation GO FASTEST.\n");
			fprintf( logfile, "#==>     I=%lf\n", I);
			fprintf( logfile, "#==>   SPI=%16.15lf\n",   
							 ind(schedule[ idx ]).seconds_per_insn * 
							 schedule[idx].observed_freq / frequencies[FASTEST_FREQ]);
			fprintf( logfile, "#==> I*SPI=%16.15lf\n", 
							 I*ind(schedule[ idx ]).seconds_per_insn *
							 schedule[idx].observed_freq / frequencies[FASTEST_FREQ]);
			fprintf( logfile, "#==>     d=%lf\n", d
							 );
		}
#endif
		current_freq = FASTEST_FREQ;
	}
	// If the slowest frequency isn't slow enough, use that.
	//! @todo fix for average frequency measurement; need prediction
	else if( I * ind(schedule[ idx ]).seconds_per_insn * 
					 ind(schedule[ idx ]).observed_freq/frequencies[ SLOWEST_FREQ ] <= d ){
#ifdef _DEBUG
		if(g_trace){
			fprintf( logfile, "#==> schedule_computation GO SLOWEST.\n");
			fprintf( logfile, "#==>     I=%lf\n", I);
			fprintf( logfile, "#==>   SPI=%16.15lf\n",   
							 ind(schedule[ idx ]).seconds_per_insn * 
							 schedule[idx].observed_freq / frequencies[ SLOWEST_FREQ ]);
			fprintf( logfile, "#==> I*SPI=%16.15lf\n", 
							 I*ind(schedule[ idx ]).seconds_per_insn * 
							 schedule[idx].observed_freq / frequencies[ SLOWEST_FREQ ]);
			fprintf( logfile, "#==>     d=%lf\n", d);
		}
#endif
		current_freq = SLOWEST_FREQ;
	}
	// Find the slowest frequency that allows the work to be completed in time.
	else{
		for( i=SLOWEST_FREQ-1; i>FASTEST_FREQ; i-- ){
			if( I * ind(schedule[ idx ]).seconds_per_insn * 
					ind(schedule[ idx ]).observed_freq/frequencies[ i ] < d ){
				// We have a winner.
				//! @todo adjust for average frequency measurement
				p = 
					( d - I * ind(schedule[ idx ]).seconds_per_insn * 
						ind(schedule[ idx ]).observed_freq/frequencies[ i+1 ] )
					/
					( I * ind(schedule[ idx ]).seconds_per_insn * 
						ind(schedule[ idx ]).observed_freq/frequencies[ i ] - 
					  I * ind(schedule[ idx ]).seconds_per_insn * 
						ind(schedule[ idx ]).observed_freq/frequencies[ i+1 ] );

#ifdef _DEBUG
				if(g_trace){
					fprintf( logfile, "#==> schedule_computation GO %d.\n", i);
					fprintf( logfile, "#----> SPI[%d] = %15.14lf\n", FASTEST_FREQ, 
									 schedule[idx].seconds_per_insn * 
									 schedule[idx].observed_freq / frequencies[FASTEST_FREQ]);
					fprintf( logfile, "#----> SPI[%i] = %15.14lf\n", i, 
									 schedule[idx].seconds_per_insn *
									 schedule[idx].observed_freq / frequencies[i]);
					fprintf( logfile, "#---->       d = %lf\n", d);
					fprintf( logfile, "#---->       I = %lf\n", I);
					fprintf( logfile, "#---->       p = %lf\n", p);
					fprintf( logfile, "#---->     idx = %d\n", idx);
				}
#endif
				current_freq = i;

				// Do we need to shift down partway through?
				if((    p * I * ind(schedule[idx]).seconds_per_insn * 
								ind(schedule[ idx ]).observed_freq/frequencies[i  ]>GMPI_MIN_COMP_SECONDS)
					 && (1.0-p * I * ind(schedule[idx]).seconds_per_insn * 
						ind(schedule[ idx ]).observed_freq/frequencies[i+1]>GMPI_MIN_COMP_SECONDS)
				){
					seconds_until_interrupt = 
						p * I * ind(schedule[ idx ]).seconds_per_insn * 
						ind(schedule[ idx ]).observed_freq/frequencies[ i ];
					next_freq = i+1;
#ifdef _DEBUG
					if(g_trace)
						fprintf( logfile, "#==> schedule_computation alarm=%15.14lf.\n", 
										 seconds_until_interrupt);
#endif
					set_alarm(seconds_until_interrupt);
				}
				break;
			}
		}
	}
	*/
 schedule_computation_exit:
	schedule[idx].requested_ratio = ratios[current_freq];
	return;
}

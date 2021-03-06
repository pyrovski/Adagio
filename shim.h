/* shim.h
 *
 * The two global functions here, shim_pre() and shim_post, are called from the
 * shim_functions file when an MPI call is intercepted.
 */
#include "shift.h"

extern double frequencies[MAX_NUM_FREQUENCIES], ratios[MAX_NUM_FREQUENCIES];
extern int current_freq;
extern int g_bind;
extern int g_trace;
extern int g_algo;
extern int my_core, my_socket, my_local; // intialized in post_MPI_Init
extern int rank;
extern int binding_stable;

// MPI_Init
void pre_MPI_Init 	();
void post_MPI_Init	(char ** argv);
// MPI_Finalize
void pre_MPI_Finalize 	();
void post_MPI_Finalize	();

void shim_pre_1();
void shim_pre_2(int shim_id);
void shim_post_1();
void shim_post_2();
void shim_post_3();

void Log( const char *fname, int MsgSz, int MsgDest, int MsgSrc);

// Schedule entry
struct entry{
  double observed_comm_seconds; // from previous
  
  double observed_comp_seconds; // from previous
  double observed_comp_insn; // from previous
  double seconds_per_insn; // from previous

  double observed_freq; // from previous
  double observed_ratio; // from previous
  double start_time; // from previous
  double end_time; // from previous
  double c0_ratio; // from previous
  double observed_comm_ratio;
  double observed_comm_c0;
};

#define histEntries 3

struct entryHist{
  struct entry hist[histEntries];
  unsigned index;

  double desired_ratio; // for next
  int following_entry; // from previous, for next
  float requested_ratio; // for next?
};

// indirection
#define ind(eh)(eh.hist[eh.index])

// double indirection
#define indd(eh)(eh->hist[eh->index])

// indirection to previous instance
#define indp(eh)(eh.hist[(eh.index + histEntries - 1) % histEntries])


enum{ 
	// To run without library overhead, use the .pristine binary.
	algo_NONE	      = 0x000,	// Identical to CLEAN.
	algo_FERMATA 	  = 0x001,	// slow communication
	algo_ANDANTE 	  = 0x002,	// slow comp before sync pts.
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

	trace_NONE    = 0x000,
	/*
	trace_TS      = 0x001,	// Timestamp.
	trace_FILE    = 0x002,	// Filename where MPI call occurred.
	trace_LINE    = 0x004,	// Line # where MPI call occurred.
	trace_FN      = 0x008,	// MPI Function Name.
	trace_COMP	  = 0x010,	// Elapsed comp time since the end of
	                        // the previous MPI comm call.
	trace_COMM	  = 0x020,	// Elapsed comm time spend in mpi lib.
	trace_RANK	  = 0x040,	// MPI rank.
	*/
	trace_PCONTROL = 0x080,	// Most recent pcontrol.
	trace_THRESH   = 0x100, // only tasks above thresholds
	trace_ALL      = 0xFFF,

	bind_COLLAPSE = 1
};

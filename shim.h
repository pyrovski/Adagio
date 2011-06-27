/* shim.h
 *
 * The two global functions here, shim_pre() and shim_post, are called from the
 * shim_functions file when an MPI call is intercepted.
 */
#include "shift.h"
#include "shim_parameters.h"

extern double frequencies[MAX_NUM_FREQUENCIES];

void shim_pre( int shim_id, union shim_parameters *p );
void shim_post( int shim_id, union shim_parameters *p );
char* f2str( int shim_id );
void Log( int shim_id, union shim_parameters *p );

//extern FILE *logfile;

// Schedule entry
/*! @todo this structure may need to change to accommodate multiple frequencies
  per task
 */
struct entry{
	double observed_comm_seconds;

	double observed_comp_seconds[MAX_NUM_FREQUENCIES];
	double observed_comp_insn[MAX_NUM_FREQUENCIES];
	double seconds_per_insn[MAX_NUM_FREQUENCIES];
	int following_entry;
};



/* shim.h
 *
 * The two global functions here, shim_pre() and shim_post, are called from the
 * shim_functions file when an MPI call is intercepted.
 */
#include "shift.h"
#include "shim_parameters.h"
void shim_pre( int shim_id, union shim_parameters *p );
void shim_post( int shim_id, union shim_parameters *p );
char* f2str( int shim_id );
void Log( int shim_id, union shim_parameters *p );



// Schedule entry
struct entry{
	double observed_comm_seconds;

  //! @todo allocate
	double *observed_comp_seconds;
	double *observed_comp_insn;
	double *seconds_per_insn;
	int following_entry;
};



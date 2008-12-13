/* shim.h
 *
 * The two global functions here, shim_pre() and shim_post, are called from the
 * shim_functions file when an MPI call is intercepted.
 */

#include "shim_parameters.h"
void shim_pre( int shim_id, union shim_parameters *p );
void shim_post( int shim_id, union shim_parameters *p );
char* f2str( int shim_id );
void Log( int shim_id, union shim_parameters *p );

// MPI_Init
static void pre_MPI_Init 	( union shim_parameters *p );
static void post_MPI_Init	( union shim_parameters *p );
// MPI_Finalize
static void pre_MPI_Finalize 	( union shim_parameters *p );
static void post_MPI_Finalize	( union shim_parameters *p );


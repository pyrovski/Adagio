/*
Wrap all relevant MPI functions.
For MPI_Init and MPI_Finalize, do extra setup/teardown work.
For some functions that send or receive, record extra message info.
Idea: don't ship around the shim_parameters union; just pass parameters directly
  to Log().
*/

#include "shim.h"


{{fn foo MPI_Init}}
     shim_pre_1();
     pre_MPI_Init();
     shim_pre_2();
     {{callfn}}
     shim_post1();
     post_MPI_Init();
     shim_post_2();
     if(g_trace)
          Log();
     shim_post_3();
{{endfn}}

{{fn foo MPI_Finalize}}
     shim_pre_1();
     pre_MPI_Finalize();
     shim_pre_2();
     {{callfn}}
     shim_post_1();
     shim_post_2();
     if(g_trace)
          Log();
     shim_post_3();
     post_MPI_Finalize();
{{endfn}}

/*
for all functions except init and finalize, use
{{fnall foo MPI_Init MPI_Finalize}}
*/
{{fn foo MPI_Send MPI_Recv MPI_Reduce MPI_Alltoall MPI_ALltoallv MPI_Allreduce MPI_Barrier MPI_Bcast MPI_Pcontrol MPI_Waitall MPI_Wait}}
{
     shim_pre();
     {{callfn}}
     shim_post_1();
     shim_post_2();
     if(g_trace)
          Log();
     shim_post_3();
}
{{endfnall}}
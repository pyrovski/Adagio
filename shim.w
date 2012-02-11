/*
Wrap all MPI functions.
Each function will have an associated integer descriptor.
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

{{fnall foo MPI_Init MPI_Finalize}}
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
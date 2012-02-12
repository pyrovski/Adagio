/*
Wrap all relevant MPI functions.
For MPI_Init and MPI_Finalize, do extra setup/teardown work.
For some functions that send or receive, record extra message info.
Idea: don't ship around the shim_parameters union; just pass parameters directly
  to Log().
*/

#include "shim.h"
#include "shim_h.h"

{{fn foo MPI_Init}}{
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
}
{{endfn}}

{{fn foo MPI_Finalize}}{
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
}
{{endfn}}

/*
for all functions except init and finalize, use
{{fnall foo MPI_Init MPI_Finalize}}
*/

{{fn foo MPI_Alltoallv MPI_Barrier MPI_Pcontrol MPI_Waitall MPI_Wait}}{
  shim_pre();
  {{callfn}}
  shim_post_1();
  shim_post_2();
  if(g_trace)
    Log("{{foo}}", -1, -1, -1);
  shim_post_3();
}
{{endfn}}

// MPI_Isend
{{fn foo MPI_Send MPI_Ssend MPI_Bsend MPI_Rsend}}{
  MPI_Aint lb; 
  MPI_Aint extent;
  int msgSize, msgSrc, msgDest;
  
  shim_pre();
  {{callfn}}
  shim_post_1();
  shim_post_2();
  if(g_trace){
    PMPI_Type_get_extent({{2}}, &lb, &extent);
    msgSize = extent * {{1}};
    msgSrc = rank;
    msgDest = {{3}};
    Log("{{foo}}", msgSize, msgSrc, msgDest);
  }
  shim_post_3();
}
{{endfn}}

{{fn foo MPI_Reduce MPI_Allreduce}}{
  MPI_Aint lb; 
  MPI_Aint extent;
  int msgSize, msgSrc, msgDest;
  
  shim_pre();
  {{callfn}}
  shim_post_1();
  shim_post_2();
  if(g_trace){
    PMPI_Type_get_extent({{3}}, &lb, &extent);
    msgSize = extent * {{2}};
    msgSrc = -2;
    msgDest = G{{foo}} == GMPI_Reduce ? {{5}} : -2;
    Log("{{foo}}", msgSize, msgSrc, msgDest);
  }
  shim_post_3();
}
{{endfn}}

// MPI_Irecv
{{fn foo MPI_Recv MPI_Bcast}}{
  MPI_Aint lb; 
  MPI_Aint extent;
  int msgSize, msgSrc, msgDest;
  
  shim_pre();
  {{callfn}}
  shim_post_1();
  shim_post_2();
  if(g_trace){
    PMPI_Type_get_extent({{2}}, &lb, &extent);
    msgSize = extent * {{1}};
    msgSrc = {{3}};
    msgDest = G{{foo}} == GMPI_Bcast ? -2 : rank;
    Log("{{foo}}", msgSize, msgSrc, msgDest);
  }
  shim_post_3();
}
{{endfn}}


{{fn foo MPI_Alltoall MPI_Scatter MPI_Gather}}{
  MPI_Aint lb; 
  MPI_Aint extent;
  int msgSize, msgSrc, msgDest;
  
  shim_pre();
  {{callfn}}
  shim_post_1();
  shim_post_2();
  if(g_trace){
    PMPI_Type_get_extent({{2}}, &lb, &extent);
    msgSize = extent * {{1}};
    PMPI_Type_get_extent({{5}}, &lb, &extent);
    msgSize += extent * {{4}};
    msgDest = G{{foo}} == GMPI_Gather ? rank : -2;
    msgSrc = G{{foo}} == GMPI_Scatter ? rank : -2;
    Log("{{foo}}", msgSize, msgSrc, msgDest);
  }
  shim_post_3();
}
{{endfn}}


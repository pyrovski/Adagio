/*
Wrap all relevant MPI functions.
Each function will have an associated integer descriptor.
*/

enum {
{{foreachfn foo MPI_Init MPI_Finalize MPI_Send MPI_Ssend MPI_Bsend MPI_Rsend MPI_Recv MPI_Reduce MPI_Alltoall MPI_Scatter MPI_Gather MPI_Alltoallv MPI_Allreduce MPI_Barrier MPI_Bcast MPI_Pcontrol MPI_Waitall MPI_Wait}}
  G{{foo}} = {{fn_num}},
{{endforeachfn}}
  SHIM_NUM_FUNCTIONS
};

extern char* fnNames[SHIM_NUM_FUNCTIONS];

/*
 * Implementation for pre and post processing for MPI calls.  This
 * library is originally created with 6.5.8 because that is what I
 * can easily test with.  API compatability changes may cause
 * adjustments to library functionality depending on your specific
 * requirements.
 *
 * $Id: impie.c,v 1.6 2004/04/21 19:36:01 mefemal Exp $
 *
 * TODO:
 *    ??
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <link.h>
#include <mpi.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include "trace.h"
#include "impie.h"
#include "macros.h"
#include "mark.h"

/* globals */
int idebug = 0;                    /* current function index */
int tdebug = 0;                    /* set to 1 to enable timings */
int etrace = 0;                    /* execution tracing (1=enabled) */

/* local prototypes */
static int get_table_offset(char *name, int *p);
static int parse_path(char *path);
static int read_config();
static int read_directives(FILE *fp);
static void fatal_error(const char *fmt, ...);
static char *parse_name(const char *name);

/* local variables */
static int config_read = 0;        /* flag to indicate config done */
static entry_t fctable[250];       /* table of function calls: 236 last count */
static void **data;                /* extra info kept for calls */

double timer()
{
 double tm;
 struct timeval t;
 if (gettimeofday(&t, NULL) < 0) {
         printf("gettimeofday failed\n");
         exit(-1);
        }
 tm=((t.tv_sec + t.tv_usec/(double)1000000));
 return (tm);
}																                                                                                                                                                              

char *impie_function() {
   if (idebug)
      return fctable[idebug].name;
   else
      return NULL;
}

void impie_statistics() {
   int i;
   unsigned int total;

   if (!tdebug)
      return;

   printf("%8s %14s %14s %14s %s\n",
      "#", "Time Pre", "Time MPI", "Time Post", "MPI Call");
   printf("%8s %14s %14s %14s %s\n",
      "--------",
      "--------------",
      "--------------",
      "--------------",
      "------------------------");
   for (i = 0; i < IMPIE_NULL; i++)
      if (fctable[i].name != NULL && fctable[i].count > 0) {
         total = fctable[i].tm_pre + fctable[i].tm_mpi + fctable[i].tm_post;
         if (total)
            printf("%8u %8.1f/%0.3f %8.1f/%0.3f %8.1f/%0.3f %s\n",
               fctable[i].count,
               fctable[i].tm_pre / 1000.0 + .05,
               fctable[i].tm_pre / (double)total,
               fctable[i].tm_mpi / 1000.0 + .05,
               fctable[i].tm_mpi / (double)total,
               fctable[i].tm_post / 1000.0 + .05,
               fctable[i].tm_post / (double)total,
               fctable[i].name);
      }
}

/*
 * Variable argument routine that will print debugging messages to
 * the console via stderr.
 */
static void fatal_error(const char *fmt, ...) {
   char *p = NULL;
   va_list ap;
   int n, size = 100;

   if ((p = malloc(size)) == NULL)
      return;
   while (1)
   {
      va_start(ap, fmt);
      n = vsnprintf(p, size, fmt, ap);
      va_end(ap);
      if (n > -1 && n < size)
         break;
      if (n > -1)
         size = n + 1;
      else
         size *= 2;
      if ((p = realloc(p, size)) == NULL)
         return;
   }
   fprintf(stderr, "%d:%s\n", getpid(), p);
   exit(1);
}

/*
 * This function is horribly inefficient, but offsets are only needed
 * on initial configuration so this may or may not be an issue.
 */
static int get_table_offset(char *name, int *p) {
   enum arindex r = UNKNOWN;

   if (strcasecmp(name, "ABORT_PRE") == 0) {
      r = IMPIE_ABORT; *p = 0;
   } else if (strcasecmp(name, "ABORT_POST") == 0) {
      r = IMPIE_ABORT; *p = 1;
#ifdef _IMPIE_HAVE_LAM
   } else if (strcasecmp(name, "ACCUMULATE_PRE") == 0) {
      r = IMPIE_ACCUMULATE; *p = 0;
   } else if (strcasecmp(name, "ACCUMULATE_POST") == 0) {
      r = IMPIE_ACCUMULATE; *p = 1;
#endif
   } else if (strcasecmp(name, "ADDRESS_PRE") == 0) {
      r = IMPIE_ADDRESS; *p = 0;
   } else if (strcasecmp(name, "ADDRESS_POST") == 0) {
      r = IMPIE_ADDRESS; *p = 1;
   } else if (strcasecmp(name, "ALLGATHER_PRE") == 0) {
      r = IMPIE_ALLGATHER; *p = 0;
   } else if (strcasecmp(name, "ALLGATHER_POST") == 0) {
      r = IMPIE_ALLGATHER; *p = 1;
   } else if (strcasecmp(name, "ALLGATHERV_PRE") == 0) {
      r = IMPIE_ALLGATHERV; *p = 0;
   } else if (strcasecmp(name, "ALLGATHERV_POST") == 0) {
      r = IMPIE_ALLGATHERV; *p = 1;
   } else if (strcasecmp(name, "ALLOC_MEM_PRE") == 0) {
      r = IMPIE_ALLOC_MEM; *p = 0;
   } else if (strcasecmp(name, "ALLOC_MEM_POST") == 0) {
      r = IMPIE_ALLOC_MEM; *p = 1;
   } else if (strcasecmp(name, "ALLREDUCE_PRE") == 0) {
      r = IMPIE_ALLREDUCE; *p = 0;
   } else if (strcasecmp(name, "ALLREDUCE_POST") == 0) {
      r = IMPIE_ALLREDUCE; *p = 1;
   } else if (strcasecmp(name, "ALLTOALL_PRE") == 0) {
      r = IMPIE_ALLTOALL; *p = 0;
   } else if (strcasecmp(name, "ALLTOALL_POST") == 0) {
      r = IMPIE_ALLTOALL; *p = 1;
   } else if (strcasecmp(name, "ALLTOALLV_PRE") == 0) {
      r = IMPIE_ALLTOALLV; *p = 0;
   } else if (strcasecmp(name, "ALLTOALLV_POST") == 0) {
      r = IMPIE_ALLTOALLV; *p = 1;
   } else if (strcasecmp(name, "ATTR_DELETE_PRE") == 0) {
      r = IMPIE_ATTR_DELETE; *p = 0;
   } else if (strcasecmp(name, "ATTR_DELETE_POST") == 0) {
      r = IMPIE_ATTR_DELETE; *p = 1;
   } else if (strcasecmp(name, "ATTR_GET_PRE") == 0) {
      r = IMPIE_ATTR_GET; *p = 0;
   } else if (strcasecmp(name, "ATTR_GET_POST") == 0) {
      r = IMPIE_ATTR_GET; *p = 1;
   } else if (strcasecmp(name, "ATTR_PUT_PRE") == 0) {
      r = IMPIE_ATTR_PUT; *p = 0;
   } else if (strcasecmp(name, "ATTR_PUT_POST") == 0) {
      r = IMPIE_ATTR_PUT; *p = 1;
   } else if (strcasecmp(name, "BARRIER_PRE") == 0) {
      r = IMPIE_BARRIER; *p = 0;
   } else if (strcasecmp(name, "BARRIER_POST") == 0) {
      r = IMPIE_BARRIER; *p = 1;
   } else if (strcasecmp(name, "BCAST_PRE") == 0) {
      r = IMPIE_BCAST; *p = 0;
   } else if (strcasecmp(name, "BCAST_POST") == 0) {
      r = IMPIE_BCAST; *p = 1;
   } else if (strcasecmp(name, "BSEND_PRE") == 0) {
      r = IMPIE_BSEND; *p = 0;
   } else if (strcasecmp(name, "BSEND_POST") == 0) {
      r = IMPIE_BSEND; *p = 1;
   } else if (strcasecmp(name, "BSEND_INIT_PRE") == 0) {
      r = IMPIE_BSEND_INIT; *p = 0;
   } else if (strcasecmp(name, "BSEND_INIT_POST") == 0) {
      r = IMPIE_BSEND_INIT; *p = 1;
   } else if (strcasecmp(name, "BUFFER_ATTACH_PRE") == 0) {
      r = IMPIE_BUFFER_ATTACH; *p = 0;
   } else if (strcasecmp(name, "BUFFER_ATTACH_POST") == 0) {
      r = IMPIE_BUFFER_ATTACH; *p = 1;
   } else if (strcasecmp(name, "BUFFER_DETACH_PRE") == 0) {
      r = IMPIE_BUFFER_DETACH; *p = 0;
   } else if (strcasecmp(name, "BUFFER_DETACH_POST") == 0) {
      r = IMPIE_BUFFER_DETACH; *p = 1;
   } else if (strcasecmp(name, "CANCEL_PRE") == 0) {
      r = IMPIE_CANCEL; *p = 0;
   } else if (strcasecmp(name, "CANCEL_POST") == 0) {
      r = IMPIE_CANCEL; *p = 1;
   } else if (strcasecmp(name, "CART_COORDS_PRE") == 0) {
      r = IMPIE_CART_COORDS; *p = 0;
   } else if (strcasecmp(name, "CART_COORDS_POST") == 0) {
      r = IMPIE_CART_COORDS; *p = 1;
   } else if (strcasecmp(name, "CART_CREATE_PRE") == 0) {
      r = IMPIE_CART_CREATE; *p = 0;
   } else if (strcasecmp(name, "CART_CREATE_POST") == 0) {
      r = IMPIE_CART_CREATE; *p = 1;
   } else if (strcasecmp(name, "CARTDIM_GET_PRE") == 0) {
      r = IMPIE_CARTDIM_GET; *p = 0;
   } else if (strcasecmp(name, "CARTDIM_GET_POST") == 0) {
      r = IMPIE_CARTDIM_GET; *p = 1;
   } else if (strcasecmp(name, "CART_GET_PRE") == 0) {
      r = IMPIE_CART_GET; *p = 0;
   } else if (strcasecmp(name, "CART_GET_POST") == 0) {
      r = IMPIE_CART_GET; *p = 1;
   } else if (strcasecmp(name, "CART_MAP_PRE") == 0) {
      r = IMPIE_CART_MAP; *p = 0;
   } else if (strcasecmp(name, "CART_MAP_POST") == 0) {
      r = IMPIE_CART_MAP; *p = 1;
   } else if (strcasecmp(name, "CART_RANK_PRE") == 0) {
      r = IMPIE_CART_RANK; *p = 0;
   } else if (strcasecmp(name, "CART_RANK_POST") == 0) {
      r = IMPIE_CART_RANK; *p = 1;
   } else if (strcasecmp(name, "CART_SHIFT_PRE") == 0) {
      r = IMPIE_CART_SHIFT; *p = 0;
   } else if (strcasecmp(name, "CART_SHIFT_POST") == 0) {
      r = IMPIE_CART_SHIFT; *p = 1;
   } else if (strcasecmp(name, "CART_SUB_PRE") == 0) {
      r = IMPIE_CART_SUB; *p = 0;
   } else if (strcasecmp(name, "CART_SUB_POST") == 0) {
      r = IMPIE_CART_SUB; *p = 1;
   } else if (strcasecmp(name, "CLOSE_PORT_PRE") == 0) {
      r = IMPIE_CLOSE_PORT; *p = 0;
   } else if (strcasecmp(name, "CLOSE_PORT_POST") == 0) {
      r = IMPIE_CLOSE_PORT; *p = 1;
#ifdef _IMPIE_HAVE_LAM
   } else if (strcasecmp(name, "COMM_C2F_PRE") == 0) {
      r = IMPIE_COMM_C2F; *p = 0;
   } else if (strcasecmp(name, "COMM_C2F_POST") == 0) {
      r = IMPIE_COMM_C2F; *p = 1;
#endif
   } else if (strcasecmp(name, "COMM_ACCEPT_PRE") == 0) {
      r = IMPIE_COMM_ACCEPT; *p = 0;
   } else if (strcasecmp(name, "COMM_ACCEPT_POST") == 0) {
      r = IMPIE_COMM_ACCEPT; *p = 1;
   } else if (strcasecmp(name, "COMM_COMPARE_PRE") == 0) {
      r = IMPIE_COMM_COMPARE; *p = 0;
   } else if (strcasecmp(name, "COMM_COMPARE_POST") == 0) {
      r = IMPIE_COMM_COMPARE; *p = 1;
   } else if (strcasecmp(name, "COMM_CONNECT_PRE") == 0) {
      r = IMPIE_COMM_CONNECT; *p = 0;
   } else if (strcasecmp(name, "COMM_CONNECT_POST") == 0) {
      r = IMPIE_COMM_CONNECT; *p = 1;
   } else if (strcasecmp(name, "COMM_CREATE_PRE") == 0) {
      r = IMPIE_COMM_CREATE; *p = 0;
   } else if (strcasecmp(name, "COMM_CREATE_POST") == 0) {
      r = IMPIE_COMM_CREATE; *p = 1;
#ifdef _IMPIE_HAVE_LAM
   } else if (strcasecmp(name, "COMM_CREATE_ERRHANDLER_PRE") == 0) {
      r = IMPIE_COMM_CREATE_ERRHANDLER; *p = 0;
   } else if (strcasecmp(name, "COMM_CREATE_ERRHANDLER_POST") == 0) {
      r = IMPIE_COMM_CREATE_ERRHANDLER; *p = 1;
   } else if (strcasecmp(name, "COMM_CREATE_KEYVAL_PRE") == 0) {
      r = IMPIE_COMM_CREATE_KEYVAL; *p = 0;
   } else if (strcasecmp(name, "COMM_CREATE_KEYVAL_POST") == 0) {
      r = IMPIE_COMM_CREATE_KEYVAL; *p = 1;
#endif
   } else if (strcasecmp(name, "COMM_DELETE_ATTR_PRE") == 0) {
      r = IMPIE_COMM_DELETE_ATTR; *p = 0;
   } else if (strcasecmp(name, "COMM_DELETE_ATTR_POST") == 0) {
      r = IMPIE_COMM_DELETE_ATTR; *p = 1;
   } else if (strcasecmp(name, "COMM_DISCONNECT_PRE") == 0) {
      r = IMPIE_COMM_DISCONNECT; *p = 0;
   } else if (strcasecmp(name, "COMM_DISCONNECT_POST") == 0) {
      r = IMPIE_COMM_DISCONNECT; *p = 1;
   } else if (strcasecmp(name, "COMM_DUP_PRE") == 0) {
      r = IMPIE_COMM_DUP; *p = 0;
   } else if (strcasecmp(name, "COMM_DUP_POST") == 0) {
      r = IMPIE_COMM_DUP; *p = 1;
#ifdef _IMPIE_HAVE_LAM
   } else if (strcasecmp(name, "COMM_F2C_PRE") == 0) {
      r = IMPIE_COMM_F2C; *p = 0;
   } else if (strcasecmp(name, "COMM_F2C_POST") == 0) {
      r = IMPIE_COMM_F2C; *p = 1;
#endif
   } else if (strcasecmp(name, "COMM_FREE_PRE") == 0) {
      r = IMPIE_COMM_FREE; *p = 0;
   } else if (strcasecmp(name, "COMM_FREE_POST") == 0) {
      r = IMPIE_COMM_FREE; *p = 1;
   } else if (strcasecmp(name, "COMM_FREE_KEYVAL_PRE") == 0) {
      r = IMPIE_COMM_FREE_KEYVAL; *p = 0;
   } else if (strcasecmp(name, "COMM_FREE_KEYVAL_POST") == 0) {
      r = IMPIE_COMM_FREE_KEYVAL; *p = 1;
   } else if (strcasecmp(name, "COMM_GET_ATTR_PRE") == 0) {
      r = IMPIE_COMM_GET_ATTR; *p = 0;
   } else if (strcasecmp(name, "COMM_GET_ATTR_POST") == 0) {
      r = IMPIE_COMM_GET_ATTR; *p = 1;
   } else if (strcasecmp(name, "COMM_GET_ERRHANDLER_PRE") == 0) {
      r = IMPIE_COMM_GET_ERRHANDLER; *p = 0;
   } else if (strcasecmp(name, "COMM_GET_ERRHANDLER_POST") == 0) {
      r = IMPIE_COMM_GET_ERRHANDLER; *p = 1;
   } else if (strcasecmp(name, "COMM_GET_NAME_PRE") == 0) {
      r = IMPIE_COMM_GET_NAME; *p = 0;
   } else if (strcasecmp(name, "COMM_GET_NAME_POST") == 0) {
      r = IMPIE_COMM_GET_NAME; *p = 1;
   } else if (strcasecmp(name, "COMM_GET_PARENT_PRE") == 0) {
      r = IMPIE_COMM_GET_PARENT; *p = 0;
   } else if (strcasecmp(name, "COMM_GET_PARENT_POST") == 0) {
      r = IMPIE_COMM_GET_PARENT; *p = 1;
   } else if (strcasecmp(name, "COMM_GROUP_PRE") == 0) {
      r = IMPIE_COMM_GROUP; *p = 0;
   } else if (strcasecmp(name, "COMM_GROUP_POST") == 0) {
      r = IMPIE_COMM_GROUP; *p = 1;
   } else if (strcasecmp(name, "COMM_JOIN_PRE") == 0) {
      r = IMPIE_COMM_JOIN; *p = 0;
   } else if (strcasecmp(name, "COMM_JOIN_POST") == 0) {
      r = IMPIE_COMM_JOIN; *p = 1;
   } else if (strcasecmp(name, "COMM_RANK_PRE") == 0) {
      r = IMPIE_COMM_RANK; *p = 0;
   } else if (strcasecmp(name, "COMM_RANK_POST") == 0) {
      r = IMPIE_COMM_RANK; *p = 1;
   } else if (strcasecmp(name, "COMM_REMOTE_GROUP_PRE") == 0) {
      r = IMPIE_COMM_REMOTE_GROUP; *p = 0;
   } else if (strcasecmp(name, "COMM_REMOTE_GROUP_POST") == 0) {
      r = IMPIE_COMM_REMOTE_GROUP; *p = 1;
   } else if (strcasecmp(name, "COMM_REMOTE_SIZE_PRE") == 0) {
      r = IMPIE_COMM_REMOTE_SIZE; *p = 0;
   } else if (strcasecmp(name, "COMM_REMOTE_SIZE_POST") == 0) {
      r = IMPIE_COMM_REMOTE_SIZE; *p = 1;
   } else if (strcasecmp(name, "COMM_SET_ATTR_PRE") == 0) {
      r = IMPIE_COMM_SET_ATTR; *p = 0;
   } else if (strcasecmp(name, "COMM_SET_ATTR_POST") == 0) {
      r = IMPIE_COMM_SET_ATTR; *p = 1;
   } else if (strcasecmp(name, "COMM_SET_ERRHANDLER_PRE") == 0) {
      r = IMPIE_COMM_SET_ERRHANDLER; *p = 0;
   } else if (strcasecmp(name, "COMM_SET_ERRHANDLER_POST") == 0) {
      r = IMPIE_COMM_SET_ERRHANDLER; *p = 1;
   } else if (strcasecmp(name, "COMM_SET_NAME_PRE") == 0) {
      r = IMPIE_COMM_SET_NAME; *p = 0;
   } else if (strcasecmp(name, "COMM_SET_NAME_POST") == 0) {
      r = IMPIE_COMM_SET_NAME; *p = 1;
   } else if (strcasecmp(name, "COMM_SIZE_PRE") == 0) {
      r = IMPIE_COMM_SIZE; *p = 0;
   } else if (strcasecmp(name, "COMM_SIZE_POST") == 0) {
      r = IMPIE_COMM_SIZE; *p = 1;
   } else if (strcasecmp(name, "COMM_SPAWN_PRE") == 0) {
      r = IMPIE_COMM_SPAWN; *p = 0;
   } else if (strcasecmp(name, "COMM_SPAWN_POST") == 0) {
      r = IMPIE_COMM_SPAWN; *p = 1;
   } else if (strcasecmp(name, "COMM_SPAWN_MULTIPLE_PRE") == 0) {
      r = IMPIE_COMM_SPAWN_MULTIPLE; *p = 0;
   } else if (strcasecmp(name, "COMM_SPAWN_MULTIPLE_POST") == 0) {
      r = IMPIE_COMM_SPAWN_MULTIPLE; *p = 1;
   } else if (strcasecmp(name, "COMM_SPLIT_PRE") == 0) {
      r = IMPIE_COMM_SPLIT; *p = 0;
   } else if (strcasecmp(name, "COMM_SPLIT_POST") == 0) {
      r = IMPIE_COMM_SPLIT; *p = 1;
   } else if (strcasecmp(name, "COMM_TEST_INTER_PRE") == 0) {
      r = IMPIE_COMM_TEST_INTER; *p = 0;
   } else if (strcasecmp(name, "COMM_TEST_INTER_POST") == 0) {
      r = IMPIE_COMM_TEST_INTER; *p = 1;
   } else if (strcasecmp(name, "DIMS_CREATE_PRE") == 0) {
      r = IMPIE_DIMS_CREATE; *p = 0;
   } else if (strcasecmp(name, "DIMS_CREATE_POST") == 0) {
      r = IMPIE_DIMS_CREATE; *p = 1;
   } else if (strcasecmp(name, "ERRHANDLER_CREATE_PRE") == 0) {
      r = IMPIE_ERRHANDLER_CREATE; *p = 0;
   } else if (strcasecmp(name, "ERRHANDLER_CREATE_POST") == 0) {
      r = IMPIE_ERRHANDLER_CREATE; *p = 1;
#ifdef _IMPIE_HAVE_LAM
   } else if (strcasecmp(name, "ERRHANDLER_C2F_PRE") == 0) {
      r = IMPIE_ERRHANDLER_C2F; *p = 0;
   } else if (strcasecmp(name, "ERRHANDLER_C2F_POST") == 0) {
      r = IMPIE_ERRHANDLER_C2F; *p = 1;
   } else if (strcasecmp(name, "ERRHANDLER_F2C_PRE") == 0) {
      r = IMPIE_ERRHANDLER_F2C; *p = 0;
   } else if (strcasecmp(name, "ERRHANDLER_F2C_POST") == 0) {
      r = IMPIE_ERRHANDLER_F2C; *p = 1;
#endif
   } else if (strcasecmp(name, "ERRHANDLER_FREE_PRE") == 0) {
      r = IMPIE_ERRHANDLER_FREE; *p = 0;
   } else if (strcasecmp(name, "ERRHANDLER_FREE_POST") == 0) {
      r = IMPIE_ERRHANDLER_FREE; *p = 1;
   } else if (strcasecmp(name, "ERRHANDLER_GET_PRE") == 0) {
      r = IMPIE_ERRHANDLER_GET; *p = 0;
   } else if (strcasecmp(name, "ERRHANDLER_GET_POST") == 0) {
      r = IMPIE_ERRHANDLER_GET; *p = 1;
   } else if (strcasecmp(name, "ERRHANDLER_SET_PRE") == 0) {
      r = IMPIE_ERRHANDLER_SET; *p = 0;
   } else if (strcasecmp(name, "ERRHANDLER_SET_POST") == 0) {
      r = IMPIE_ERRHANDLER_SET; *p = 1;
   } else if (strcasecmp(name, "ERROR_CLASS_PRE") == 0) {
      r = IMPIE_ERROR_CLASS; *p = 0;
   } else if (strcasecmp(name, "ERROR_CLASS_POST") == 0) {
      r = IMPIE_ERROR_CLASS; *p = 1;
   } else if (strcasecmp(name, "ERROR_STRING_PRE") == 0) {
      r = IMPIE_ERROR_STRING; *p = 0;
   } else if (strcasecmp(name, "ERROR_STRING_POST") == 0) {
      r = IMPIE_ERROR_STRING; *p = 1;
   } else if (strcasecmp(name, "FINALIZE_PRE") == 0) {
      r = IMPIE_FINALIZE; *p = 0;
   } else if (strcasecmp(name, "FINALIZE_POST") == 0) {
      r = IMPIE_FINALIZE; *p = 1;
   } else if (strcasecmp(name, "FINALIZED_PRE") == 0) {
      r = IMPIE_FINALIZED; *p = 0;
   } else if (strcasecmp(name, "FINALIZED_POST") == 0) {
      r = IMPIE_FINALIZED; *p = 1;
   } else if (strcasecmp(name, "FREE_MEM_PRE") == 0) {
      r = IMPIE_FREE_MEM; *p = 0;
   } else if (strcasecmp(name, "FREE_MEM_POST") == 0) {
      r = IMPIE_FREE_MEM; *p = 1;
   } else if (strcasecmp(name, "GATHER_PRE") == 0) {
      r = IMPIE_GATHER; *p = 0;
   } else if (strcasecmp(name, "GATHER_POST") == 0) {
      r = IMPIE_GATHER; *p = 1;
   } else if (strcasecmp(name, "GATHERV_PRE") == 0) {
      r = IMPIE_GATHERV; *p = 0;
   } else if (strcasecmp(name, "GATHERV_POST") == 0) {
      r = IMPIE_GATHERV; *p = 1;
   } else if (strcasecmp(name, "GET_ADDRESS_PRE") == 0) {
      r = IMPIE_GET_ADDRESS; *p = 0;
   } else if (strcasecmp(name, "GET_ADDRESS_POST") == 0) {
      r = IMPIE_GET_ADDRESS; *p = 1;
#ifdef _IMPIE_HAVE_LAM
   } else if (strcasecmp(name, "GET_PRE") == 0) {
      r = IMPIE_GET; *p = 0;
   } else if (strcasecmp(name, "GET_POST") == 0) {
      r = IMPIE_GET; *p = 1;
#endif
   } else if (strcasecmp(name, "GET_COUNT_PRE") == 0) {
      r = IMPIE_GET_COUNT; *p = 0;
   } else if (strcasecmp(name, "GET_COUNT_POST") == 0) {
      r = IMPIE_GET_COUNT; *p = 1;
   } else if (strcasecmp(name, "GET_ELEMENTS_PRE") == 0) {
      r = IMPIE_GET_ELEMENTS; *p = 0;
   } else if (strcasecmp(name, "GET_ELEMENTS_POST") == 0) {
      r = IMPIE_GET_ELEMENTS; *p = 1;
   } else if (strcasecmp(name, "GET_PROCESSOR_NAME_PRE") == 0) {
      r = IMPIE_GET_PROCESSOR_NAME; *p = 0;
   } else if (strcasecmp(name, "GET_PROCESSOR_NAME_POST") == 0) {
      r = IMPIE_GET_PROCESSOR_NAME; *p = 1;
   } else if (strcasecmp(name, "GET_VERSION_PRE") == 0) {
      r = IMPIE_GET_VERSION; *p = 0;
   } else if (strcasecmp(name, "GET_VERSION_POST") == 0) {
      r = IMPIE_GET_VERSION; *p = 1;
   } else if (strcasecmp(name, "GRAPH_CREATE_PRE") == 0) {
      r = IMPIE_GRAPH_CREATE; *p = 0;
   } else if (strcasecmp(name, "GRAPH_CREATE_POST") == 0) {
      r = IMPIE_GRAPH_CREATE; *p = 1;
   } else if (strcasecmp(name, "GRAPHDIMS_GET_PRE") == 0) {
      r = IMPIE_GRAPHDIMS_GET; *p = 0;
   } else if (strcasecmp(name, "GRAPHDIMS_GET_POST") == 0) {
      r = IMPIE_GRAPHDIMS_GET; *p = 1;
   } else if (strcasecmp(name, "GRAPH_GET_PRE") == 0) {
      r = IMPIE_GRAPH_GET; *p = 0;
   } else if (strcasecmp(name, "GRAPH_GET_POST") == 0) {
      r = IMPIE_GRAPH_GET; *p = 1;
   } else if (strcasecmp(name, "GRAPH_MAP_PRE") == 0) {
      r = IMPIE_GRAPH_MAP; *p = 0;
   } else if (strcasecmp(name, "GRAPH_MAP_POST") == 0) {
      r = IMPIE_GRAPH_MAP; *p = 1;
   } else if (strcasecmp(name, "GRAPH_NEIGHBORS_PRE") == 0) {
      r = IMPIE_GRAPH_NEIGHBORS; *p = 0;
   } else if (strcasecmp(name, "GRAPH_NEIGHBORS_POST") == 0) {
      r = IMPIE_GRAPH_NEIGHBORS; *p = 1;
   } else if (strcasecmp(name, "GRAPH_NEIGHBORS_COUNT_PRE") == 0) {
      r = IMPIE_GRAPH_NEIGHBORS_COUNT; *p = 0;
   } else if (strcasecmp(name, "GRAPH_NEIGHBORS_COUNT_POST") == 0) {
      r = IMPIE_GRAPH_NEIGHBORS_COUNT; *p = 1;
   } else if (strcasecmp(name, "GROUP_COMPARE_PRE") == 0) {
      r = IMPIE_GROUP_COMPARE; *p = 0;
   } else if (strcasecmp(name, "GROUP_COMPARE_POST") == 0) {
      r = IMPIE_GROUP_COMPARE; *p = 1;
   } else if (strcasecmp(name, "GROUP_DIFFERENCE_PRE") == 0) {
      r = IMPIE_GROUP_DIFFERENCE; *p = 0;
   } else if (strcasecmp(name, "GROUP_DIFFERENCE_POST") == 0) {
      r = IMPIE_GROUP_DIFFERENCE; *p = 1;
   } else if (strcasecmp(name, "GROUP_EXCL_PRE") == 0) {
      r = IMPIE_GROUP_EXCL; *p = 0;
   } else if (strcasecmp(name, "GROUP_EXCL_POST") == 0) {
      r = IMPIE_GROUP_EXCL; *p = 1;
#ifdef _IMPIE_HAVE_LAM
   } else if (strcasecmp(name, "GROUP_C2F_PRE") == 0) {
      r = IMPIE_GROUP_C2F; *p = 0;
   } else if (strcasecmp(name, "GROUP_C2F_POST") == 0) {
      r = IMPIE_GROUP_C2F; *p = 1;
   } else if (strcasecmp(name, "GROUP_F2C_PRE") == 0) {
      r = IMPIE_GROUP_F2C; *p = 0;
   } else if (strcasecmp(name, "GROUP_F2C_POST") == 0) {
      r = IMPIE_GROUP_F2C; *p = 1;
#endif
   } else if (strcasecmp(name, "GROUP_FREE_PRE") == 0) {
      r = IMPIE_GROUP_FREE; *p = 0;
   } else if (strcasecmp(name, "GROUP_FREE_POST") == 0) {
      r = IMPIE_GROUP_FREE; *p = 1;
   } else if (strcasecmp(name, "GROUP_INCL_PRE") == 0) {
      r = IMPIE_GROUP_INCL; *p = 0;
   } else if (strcasecmp(name, "GROUP_INCL_POST") == 0) {
      r = IMPIE_GROUP_INCL; *p = 1;
   } else if (strcasecmp(name, "GROUP_INTERSECTION_PRE") == 0) {
      r = IMPIE_GROUP_INTERSECTION; *p = 0;
   } else if (strcasecmp(name, "GROUP_INTERSECTION_POST") == 0) {
      r = IMPIE_GROUP_INTERSECTION; *p = 1;
   } else if (strcasecmp(name, "GROUP_RANGE_EXCL_PRE") == 0) {
      r = IMPIE_GROUP_RANGE_EXCL; *p = 0;
   } else if (strcasecmp(name, "GROUP_RANGE_EXCL_POST") == 0) {
      r = IMPIE_GROUP_RANGE_EXCL; *p = 1;
   } else if (strcasecmp(name, "GROUP_RANGE_INCL_PRE") == 0) {
      r = IMPIE_GROUP_RANGE_INCL; *p = 0;
   } else if (strcasecmp(name, "GROUP_RANGE_INCL_POST") == 0) {
      r = IMPIE_GROUP_RANGE_INCL; *p = 1;
   } else if (strcasecmp(name, "GROUP_RANK_PRE") == 0) {
      r = IMPIE_GROUP_RANK; *p = 0;
   } else if (strcasecmp(name, "GROUP_RANK_POST") == 0) {
      r = IMPIE_GROUP_RANK; *p = 1;
   } else if (strcasecmp(name, "GROUP_SIZE_PRE") == 0) {
      r = IMPIE_GROUP_SIZE; *p = 0;
   } else if (strcasecmp(name, "GROUP_SIZE_POST") == 0) {
      r = IMPIE_GROUP_SIZE; *p = 1;
   } else if (strcasecmp(name, "GROUP_TRANSLATE_RANKS_PRE") == 0) {
      r = IMPIE_GROUP_TRANSLATE_RANKS; *p = 0;
   } else if (strcasecmp(name, "GROUP_TRANSLATE_RANKS_POST") == 0) {
      r = IMPIE_GROUP_TRANSLATE_RANKS; *p = 1;
   } else if (strcasecmp(name, "GROUP_UNION_PRE") == 0) {
      r = IMPIE_GROUP_UNION; *p = 0;
   } else if (strcasecmp(name, "GROUP_UNION_POST") == 0) {
      r = IMPIE_GROUP_UNION; *p = 1;
   } else if (strcasecmp(name, "IBSEND_PRE") == 0) {
      r = IMPIE_IBSEND; *p = 0;
   } else if (strcasecmp(name, "IBSEND_POST") == 0) {
      r = IMPIE_IBSEND; *p = 1;
   } else if (strcasecmp(name, "INFO_CREATE_PRE") == 0) {
      r = IMPIE_INFO_CREATE; *p = 0;
   } else if (strcasecmp(name, "INFO_CREATE_POST") == 0) {
      r = IMPIE_INFO_CREATE; *p = 1;
   } else if (strcasecmp(name, "INFO_DELETE_PRE") == 0) {
      r = IMPIE_INFO_DELETE; *p = 0;
   } else if (strcasecmp(name, "INFO_DELETE_POST") == 0) {
      r = IMPIE_INFO_DELETE; *p = 1;
   } else if (strcasecmp(name, "INFO_DUP_PRE") == 0) {
      r = IMPIE_INFO_DUP; *p = 0;
   } else if (strcasecmp(name, "INFO_DUP_POST") == 0) {
      r = IMPIE_INFO_DUP; *p = 1;
#ifdef _IMPIE_HAVE_LAM
   } else if (strcasecmp(name, "INFO_F2C_PRE") == 0) {
      r = IMPIE_INFO_F2C; *p = 0;
   } else if (strcasecmp(name, "INFO_F2C_POST") == 0) {
      r = IMPIE_INFO_F2C; *p = 1;
#endif
   } else if (strcasecmp(name, "INFO_FREE_PRE") == 0) {
      r = IMPIE_INFO_FREE; *p = 0;
   } else if (strcasecmp(name, "INFO_FREE_POST") == 0) {
      r = IMPIE_INFO_FREE; *p = 1;
   } else if (strcasecmp(name, "INFO_GET_PRE") == 0) {
      r = IMPIE_INFO_GET; *p = 0;
   } else if (strcasecmp(name, "INFO_GET_POST") == 0) {
      r = IMPIE_INFO_GET; *p = 1;
   } else if (strcasecmp(name, "INFO_GET_NKEYS_PRE") == 0) {
      r = IMPIE_INFO_GET_NKEYS; *p = 0;
   } else if (strcasecmp(name, "INFO_GET_NKEYS_POST") == 0) {
      r = IMPIE_INFO_GET_NKEYS; *p = 1;
   } else if (strcasecmp(name, "INFO_GET_NTHKEY_PRE") == 0) {
      r = IMPIE_INFO_GET_NTHKEY; *p = 0;
   } else if (strcasecmp(name, "INFO_GET_NTHKEY_POST") == 0) {
      r = IMPIE_INFO_GET_NTHKEY; *p = 1;
   } else if (strcasecmp(name, "INFO_GET_VALUELEN_PRE") == 0) {
      r = IMPIE_INFO_GET_VALUELEN; *p = 0;
   } else if (strcasecmp(name, "INFO_GET_VALUELEN_POST") == 0) {
      r = IMPIE_INFO_GET_VALUELEN; *p = 1;
   } else if (strcasecmp(name, "INFO_SET_PRE") == 0) {
      r = IMPIE_INFO_SET; *p = 0;
   } else if (strcasecmp(name, "INFO_SET_POST") == 0) {
      r = IMPIE_INFO_SET; *p = 1;
   } else if (strcasecmp(name, "INIT_PRE") == 0) {
      r = IMPIE_INIT; *p = 0;
   } else if (strcasecmp(name, "INIT_POST") == 0) {
      r = IMPIE_INIT; *p = 1;
   } else if (strcasecmp(name, "INITIALIZED_PRE") == 0) {
      r = IMPIE_INITIALIZED; *p = 0;
   } else if (strcasecmp(name, "INITIALIZED_POST") == 0) {
      r = IMPIE_INITIALIZED; *p = 1;
   } else if (strcasecmp(name, "INIT_THREAD_PRE") == 0) {
      r = IMPIE_INIT_THREAD; *p = 0;
   } else if (strcasecmp(name, "INIT_THREAD_POST") == 0) {
      r = IMPIE_INIT_THREAD; *p = 1;
   } else if (strcasecmp(name, "INTERCOMM_CREATE_PRE") == 0) {
      r = IMPIE_INTERCOMM_CREATE; *p = 0;
   } else if (strcasecmp(name, "INTERCOMM_CREATE_POST") == 0) {
      r = IMPIE_INTERCOMM_CREATE; *p = 1;
   } else if (strcasecmp(name, "INTERCOMM_MERGE_PRE") == 0) {
      r = IMPIE_INTERCOMM_MERGE; *p = 0;
   } else if (strcasecmp(name, "INTERCOMM_MERGE_POST") == 0) {
      r = IMPIE_INTERCOMM_MERGE; *p = 1;
   } else if (strcasecmp(name, "IPROBE_PRE") == 0) {
      r = IMPIE_IPROBE; *p = 0;
   } else if (strcasecmp(name, "IPROBE_POST") == 0) {
      r = IMPIE_IPROBE; *p = 1;
   } else if (strcasecmp(name, "IRECV_PRE") == 0) {
      r = IMPIE_IRECV; *p = 0;
   } else if (strcasecmp(name, "IRECV_POST") == 0) {
      r = IMPIE_IRECV; *p = 1;
   } else if (strcasecmp(name, "IRSEND_PRE") == 0) {
      r = IMPIE_IRSEND; *p = 0;
   } else if (strcasecmp(name, "IRSEND_POST") == 0) {
      r = IMPIE_IRSEND; *p = 1;
   } else if (strcasecmp(name, "ISEND_PRE") == 0) {
      r = IMPIE_ISEND; *p = 0;
   } else if (strcasecmp(name, "ISEND_POST") == 0) {
      r = IMPIE_ISEND; *p = 1;
   } else if (strcasecmp(name, "ISSEND_PRE") == 0) {
      r = IMPIE_ISSEND; *p = 0;
   } else if (strcasecmp(name, "ISSEND_POST") == 0) {
      r = IMPIE_ISSEND; *p = 1;
   } else if (strcasecmp(name, "IS_THREAD_MAIN_PRE") == 0) {
      r = IMPIE_IS_THREAD_MAIN; *p = 0;
   } else if (strcasecmp(name, "IS_THREAD_MAIN_POST") == 0) {
      r = IMPIE_IS_THREAD_MAIN; *p = 1;
   } else if (strcasecmp(name, "KEYVAL_CREATE_PRE") == 0) {
      r = IMPIE_KEYVAL_CREATE; *p = 0;
   } else if (strcasecmp(name, "KEYVAL_CREATE_POST") == 0) {
      r = IMPIE_KEYVAL_CREATE; *p = 1;
   } else if (strcasecmp(name, "KEYVAL_FREE_PRE") == 0) {
      r = IMPIE_KEYVAL_FREE; *p = 0;
   } else if (strcasecmp(name, "KEYVAL_FREE_POST") == 0) {
      r = IMPIE_KEYVAL_FREE; *p = 1;
   } else if (strcasecmp(name, "LOOKUP_NAME_PRE") == 0) {
      r = IMPIE_LOOKUP_NAME; *p = 0;
   } else if (strcasecmp(name, "LOOKUP_NAME_POST") == 0) {
      r = IMPIE_LOOKUP_NAME; *p = 1;
#ifdef _IMPIE_HAVE_MPICH
   } else if (strcasecmp(name, "NULL_COPY_FN_PRE") == 0) {
      r = IMPIE_NULL_COPY_FN; *p = 0;
   } else if (strcasecmp(name, "NULL_COPY_FN_POST") == 0) {
      r = IMPIE_NULL_COPY_FN; *p = 1;
   } else if (strcasecmp(name, "NULL_DELETE_FN_PRE") == 0) {
      r = IMPIE_NULL_DELETE_FN; *p = 0;
   } else if (strcasecmp(name, "NULL_DELETE_FN_POST") == 0) {
      r = IMPIE_NULL_DELETE_FN; *p = 1;
#endif
#ifdef _IMPIE_HAVE_LAM
   } else if (strcasecmp(name, "OP_C2F_PRE") == 0) {
      r = IMPIE_OP_C2F; *p = 0;
   } else if (strcasecmp(name, "OP_C2F_POST") == 0) {
      r = IMPIE_OP_C2F; *p = 1;
   } else if (strcasecmp(name, "OP_F2C_PRE") == 0) {
      r = IMPIE_OP_F2C; *p = 0;
   } else if (strcasecmp(name, "OP_F2C_POST") == 0) {
      r = IMPIE_OP_F2C; *p = 1;
#endif
   } else if (strcasecmp(name, "OP_CREATE_PRE") == 0) {
      r = IMPIE_OP_CREATE; *p = 0;
   } else if (strcasecmp(name, "OP_CREATE_POST") == 0) {
      r = IMPIE_OP_CREATE; *p = 1;
   } else if (strcasecmp(name, "OPEN_PORT_PRE") == 0) {
      r = IMPIE_OPEN_PORT; *p = 0;
   } else if (strcasecmp(name, "OPEN_PORT_POST") == 0) {
      r = IMPIE_OPEN_PORT; *p = 1;
   } else if (strcasecmp(name, "OP_FREE_PRE") == 0) {
      r = IMPIE_OP_FREE; *p = 0;
   } else if (strcasecmp(name, "OP_FREE_POST") == 0) {
      r = IMPIE_OP_FREE; *p = 1;
   } else if (strcasecmp(name, "PACK_PRE") == 0) {
      r = IMPIE_PACK; *p = 0;
   } else if (strcasecmp(name, "PACK_POST") == 0) {
      r = IMPIE_PACK; *p = 1;
   } else if (strcasecmp(name, "PACK_SIZE_PRE") == 0) {
      r = IMPIE_PACK_SIZE; *p = 0;
   } else if (strcasecmp(name, "PACK_SIZE_POST") == 0) {
      r = IMPIE_PACK_SIZE; *p = 1;
   } else if (strcasecmp(name, "PCONTROL_PRE") == 0) {
      r = IMPIE_PCONTROL; *p = 0;
   } else if (strcasecmp(name, "PCONTROL_POST") == 0) {
      r = IMPIE_PCONTROL; *p = 1;
   } else if (strcasecmp(name, "PROBE_PRE") == 0) {
      r = IMPIE_PROBE; *p = 0;
   } else if (strcasecmp(name, "PROBE_POST") == 0) {
      r = IMPIE_PROBE; *p = 1;
   } else if (strcasecmp(name, "PUBLISH_NAME_PRE") == 0) {
      r = IMPIE_PUBLISH_NAME; *p = 0;
   } else if (strcasecmp(name, "PUBLISH_NAME_POST") == 0) {
      r = IMPIE_PUBLISH_NAME; *p = 1;
#ifdef _IMPIE_HAVE_LAM
   } else if (strcasecmp(name, "PUT_PRE") == 0) {
      r = IMPIE_PUT; *p = 0;
   } else if (strcasecmp(name, "PUT_POST") == 0) {
      r = IMPIE_PUT; *p = 1;
#endif
   } else if (strcasecmp(name, "QUERY_THREAD_PRE") == 0) {
      r = IMPIE_QUERY_THREAD; *p = 0;
   } else if (strcasecmp(name, "QUERY_THREAD_POST") == 0) {
      r = IMPIE_QUERY_THREAD; *p = 1;
   } else if (strcasecmp(name, "RECV_PRE") == 0) {
      r = IMPIE_RECV; *p = 0;
   } else if (strcasecmp(name, "RECV_POST") == 0) {
      r = IMPIE_RECV; *p = 1;
   } else if (strcasecmp(name, "RECV_INIT_PRE") == 0) {
      r = IMPIE_RECV_INIT; *p = 0;
   } else if (strcasecmp(name, "RECV_INIT_POST") == 0) {
      r = IMPIE_RECV_INIT; *p = 1;
   } else if (strcasecmp(name, "REDUCE_PRE") == 0) {
      r = IMPIE_REDUCE; *p = 0;
   } else if (strcasecmp(name, "REDUCE_POST") == 0) {
      r = IMPIE_REDUCE; *p = 1;
   } else if (strcasecmp(name, "REDUCE_SCATTER_PRE") == 0) {
      r = IMPIE_REDUCE_SCATTER; *p = 0;
   } else if (strcasecmp(name, "REDUCE_SCATTER_POST") == 0) {
      r = IMPIE_REDUCE_SCATTER; *p = 1;
#ifdef _IMPIE_HAVE_LAM
   } else if (strcasecmp(name, "REQUEST_C2F_PRE") == 0) {
      r = IMPIE_REQUEST_C2F; *p = 0;
   } else if (strcasecmp(name, "REQUEST_C2F_POST") == 0) {
      r = IMPIE_REQUEST_C2F; *p = 1;
   } else if (strcasecmp(name, "REQUEST_F2C_PRE") == 0) {
      r = IMPIE_REQUEST_F2C; *p = 0;
   } else if (strcasecmp(name, "REQUEST_F2C_POST") == 0) {
      r = IMPIE_REQUEST_F2C; *p = 1;
#endif
   } else if (strcasecmp(name, "REQUEST_FREE_PRE") == 0) {
      r = IMPIE_REQUEST_FREE; *p = 0;
   } else if (strcasecmp(name, "REQUEST_FREE_POST") == 0) {
      r = IMPIE_REQUEST_FREE; *p = 1;
   } else if (strcasecmp(name, "RSEND_PRE") == 0) {
      r = IMPIE_RSEND; *p = 0;
   } else if (strcasecmp(name, "RSEND_POST") == 0) {
      r = IMPIE_RSEND; *p = 1;
   } else if (strcasecmp(name, "RSEND_INIT_PRE") == 0) {
      r = IMPIE_RSEND_INIT; *p = 0;
   } else if (strcasecmp(name, "RSEND_INIT_POST") == 0) {
      r = IMPIE_RSEND_INIT; *p = 1;
   } else if (strcasecmp(name, "SCAN_PRE") == 0) {
      r = IMPIE_SCAN; *p = 0;
   } else if (strcasecmp(name, "SCAN_POST") == 0) {
      r = IMPIE_SCAN; *p = 1;
   } else if (strcasecmp(name, "SCATTER_PRE") == 0) {
      r = IMPIE_SCATTER; *p = 0;
   } else if (strcasecmp(name, "SCATTER_POST") == 0) {
      r = IMPIE_SCATTER; *p = 1;
   } else if (strcasecmp(name, "SCATTERV_PRE") == 0) {
      r = IMPIE_SCATTERV; *p = 0;
   } else if (strcasecmp(name, "SCATTERV_POST") == 0) {
      r = IMPIE_SCATTERV; *p = 1;
   } else if (strcasecmp(name, "SEND_PRE") == 0) {
      r = IMPIE_SEND; *p = 0;
   } else if (strcasecmp(name, "SEND_POST") == 0) {
      r = IMPIE_SEND; *p = 1;
   } else if (strcasecmp(name, "SEND_INIT_PRE") == 0) {
      r = IMPIE_SEND_INIT; *p = 0;
   } else if (strcasecmp(name, "SEND_INIT_POST") == 0) {
      r = IMPIE_SEND_INIT; *p = 1;
   } else if (strcasecmp(name, "SENDRECV_PRE") == 0) {
      r = IMPIE_SENDRECV; *p = 0;
   } else if (strcasecmp(name, "SENDRECV_POST") == 0) {
      r = IMPIE_SENDRECV; *p = 1;
   } else if (strcasecmp(name, "SENDRECV_REPLACE_PRE") == 0) {
      r = IMPIE_SENDRECV_REPLACE; *p = 0;
   } else if (strcasecmp(name, "SENDRECV_REPLACE_POST") == 0) {
      r = IMPIE_SENDRECV_REPLACE; *p = 1;
   } else if (strcasecmp(name, "SSEND_PRE") == 0) {
      r = IMPIE_SSEND; *p = 0;
   } else if (strcasecmp(name, "SSEND_POST") == 0) {
      r = IMPIE_SSEND; *p = 1;
   } else if (strcasecmp(name, "SSEND_INIT_PRE") == 0) {
      r = IMPIE_SSEND_INIT; *p = 0;
   } else if (strcasecmp(name, "SSEND_INIT_POST") == 0) {
      r = IMPIE_SSEND_INIT; *p = 1;
   } else if (strcasecmp(name, "STARTALL_PRE") == 0) {
      r = IMPIE_STARTALL; *p = 0;
   } else if (strcasecmp(name, "STARTALL_POST") == 0) {
      r = IMPIE_STARTALL; *p = 1;
   } else if (strcasecmp(name, "START_PRE") == 0) {
      r = IMPIE_START; *p = 0;
   } else if (strcasecmp(name, "START_POST") == 0) {
      r = IMPIE_START; *p = 1;
   } else if (strcasecmp(name, "STATUS_C2F_PRE") == 0) {
      r = IMPIE_STATUS_C2F; *p = 0;
   } else if (strcasecmp(name, "STATUS_C2F_POST") == 0) {
      r = IMPIE_STATUS_C2F; *p = 1;
   } else if (strcasecmp(name, "STATUS_F2C_PRE") == 0) {
      r = IMPIE_STATUS_F2C; *p = 0;
   } else if (strcasecmp(name, "STATUS_F2C_POST") == 0) {
      r = IMPIE_STATUS_F2C; *p = 1;
#ifdef _IMPIE_HAVE_MPICH
   } else if (strcasecmp(name, "STATUS_SET_CANCELLED_PRE") == 0) {
      r = IMPIE_STATUS_SET_CANCELLED; *p = 0;
   } else if (strcasecmp(name, "STATUS_SET_CANCELLED_POST") == 0) {
      r = IMPIE_STATUS_SET_CANCELLED; *p = 1;
   } else if (strcasecmp(name, "STATUS_SET_ELEMENTS_PRE") == 0) {
      r = IMPIE_STATUS_SET_ELEMENTS; *p = 0;
   } else if (strcasecmp(name, "STATUS_SET_ELEMENTS_POST") == 0) {
      r = IMPIE_STATUS_SET_ELEMENTS; *p = 1;
#endif
   } else if (strcasecmp(name, "TESTALL_PRE") == 0) {
      r = IMPIE_TESTALL; *p = 0;
   } else if (strcasecmp(name, "TESTALL_POST") == 0) {
      r = IMPIE_TESTALL; *p = 1;
   } else if (strcasecmp(name, "TESTANY_PRE") == 0) {
      r = IMPIE_TESTANY; *p = 0;
   } else if (strcasecmp(name, "TESTANY_POST") == 0) {
      r = IMPIE_TESTANY; *p = 1;
   } else if (strcasecmp(name, "TEST_PRE") == 0) {
      r = IMPIE_TEST; *p = 0;
   } else if (strcasecmp(name, "TEST_POST") == 0) {
      r = IMPIE_TEST; *p = 1;
   } else if (strcasecmp(name, "TEST_CANCELLED_PRE") == 0) {
      r = IMPIE_TEST_CANCELLED; *p = 0;
   } else if (strcasecmp(name, "TEST_CANCELLED_POST") == 0) {
      r = IMPIE_TEST_CANCELLED; *p = 1;
   } else if (strcasecmp(name, "TESTSOME_PRE") == 0) {
      r = IMPIE_TESTSOME; *p = 0;
   } else if (strcasecmp(name, "TESTSOME_POST") == 0) {
      r = IMPIE_TESTSOME; *p = 1;
   } else if (strcasecmp(name, "TOPO_TEST_PRE") == 0) {
      r = IMPIE_TOPO_TEST; *p = 0;
   } else if (strcasecmp(name, "TOPO_TEST_POST") == 0) {
      r = IMPIE_TOPO_TEST; *p = 1;
   } else if (strcasecmp(name, "TYPE_C2F_PRE") == 0) {
      r = IMPIE_TYPE_C2F; *p = 0;
   } else if (strcasecmp(name, "TYPE_C2F_POST") == 0) {
      r = IMPIE_TYPE_C2F; *p = 1;
   } else if (strcasecmp(name, "TYPE_COMMIT_PRE") == 0) {
      r = IMPIE_TYPE_COMMIT; *p = 0;
   } else if (strcasecmp(name, "TYPE_COMMIT_POST") == 0) {
      r = IMPIE_TYPE_COMMIT; *p = 1;
   } else if (strcasecmp(name, "TYPE_CONTIGUOUS_PRE") == 0) {
      r = IMPIE_TYPE_CONTIGUOUS; *p = 0;
   } else if (strcasecmp(name, "TYPE_CONTIGUOUS_POST") == 0) {
      r = IMPIE_TYPE_CONTIGUOUS; *p = 1;
#ifdef _IMPIE_HAVE_MPICH
   } else if (strcasecmp(name, "TYPE_COUNT_PRE") == 0) {
      r = IMPIE_TYPE_COUNT; *p = 0;
   } else if (strcasecmp(name, "TYPE_COUNT_POST") == 0) {
      r = IMPIE_TYPE_COUNT; *p = 1;
#endif
   } else if (strcasecmp(name, "TYPE_CREATE_DARRAY_PRE") == 0) {
      r = IMPIE_TYPE_CREATE_DARRAY; *p = 0;
   } else if (strcasecmp(name, "TYPE_CREATE_DARRAY_POST") == 0) {
      r = IMPIE_TYPE_CREATE_DARRAY; *p = 1;
   } else if (strcasecmp(name, "TYPE_CREATE_HINDEXED_PRE") == 0) {
      r = IMPIE_TYPE_CREATE_HINDEXED; *p = 0;
   } else if (strcasecmp(name, "TYPE_CREATE_HINDEXED_POST") == 0) {
      r = IMPIE_TYPE_CREATE_HINDEXED; *p = 1;
   } else if (strcasecmp(name, "TYPE_CREATE_HVECTOR_PRE") == 0) {
      r = IMPIE_TYPE_CREATE_HVECTOR; *p = 0;
   } else if (strcasecmp(name, "TYPE_CREATE_HVECTOR_POST") == 0) {
      r = IMPIE_TYPE_CREATE_HVECTOR; *p = 1;
#ifdef _IMPIE_HAVE_MPICH
   } else if (strcasecmp(name, "TYPE_CREATE_INDEXED_BLOCK_PRE") == 0) {
      r = IMPIE_TYPE_CREATE_INDEXED_BLOCK; *p = 0;
   } else if (strcasecmp(name, "TYPE_CREATE_INDEXED_BLOCK_POST") == 0) {
      r = IMPIE_TYPE_CREATE_INDEXED_BLOCK; *p = 1;
#endif
#ifdef _IMPIE_HAVE_LAM
   } else if (strcasecmp(name, "TYPE_CREATE_KEYVAL_PRE") == 0) {
      r = IMPIE_TYPE_CREATE_KEYVAL; *p = 0;
   } else if (strcasecmp(name, "TYPE_CREATE_KEYVAL_POST") == 0) {
      r = IMPIE_TYPE_CREATE_KEYVAL; *p = 1;
#endif
   } else if (strcasecmp(name, "TYPE_CREATE_RESIZED_PRE") == 0) {
      r = IMPIE_TYPE_CREATE_RESIZED; *p = 0;
   } else if (strcasecmp(name, "TYPE_CREATE_RESIZED_POST") == 0) {
      r = IMPIE_TYPE_CREATE_RESIZED; *p = 1;
   } else if (strcasecmp(name, "TYPE_CREATE_STRUCT_PRE") == 0) {
      r = IMPIE_TYPE_CREATE_STRUCT; *p = 0;
   } else if (strcasecmp(name, "TYPE_CREATE_STRUCT_POST") == 0) {
      r = IMPIE_TYPE_CREATE_STRUCT; *p = 1;
   } else if (strcasecmp(name, "TYPE_CREATE_SUBARRAY_PRE") == 0) {
      r = IMPIE_TYPE_CREATE_SUBARRAY; *p = 0;
   } else if (strcasecmp(name, "TYPE_CREATE_SUBARRAY_POST") == 0) {
      r = IMPIE_TYPE_CREATE_SUBARRAY; *p = 1;
   } else if (strcasecmp(name, "TYPE_DELETE_ATTR_PRE") == 0) {
      r = IMPIE_TYPE_DELETE_ATTR; *p = 0;
   } else if (strcasecmp(name, "TYPE_DELETE_ATTR_POST") == 0) {
      r = IMPIE_TYPE_DELETE_ATTR; *p = 1;
   } else if (strcasecmp(name, "TYPE_DUP_PRE") == 0) {
      r = IMPIE_TYPE_DUP; *p = 0;
   } else if (strcasecmp(name, "TYPE_DUP_POST") == 0) {
      r = IMPIE_TYPE_DUP; *p = 1;
   } else if (strcasecmp(name, "TYPE_EXTENT_PRE") == 0) {
      r = IMPIE_TYPE_EXTENT; *p = 0;
   } else if (strcasecmp(name, "TYPE_EXTENT_POST") == 0) {
      r = IMPIE_TYPE_EXTENT; *p = 1;
   } else if (strcasecmp(name, "TYPE_F2C_PRE") == 0) {
      r = IMPIE_TYPE_F2C; *p = 0;
   } else if (strcasecmp(name, "TYPE_F2C_POST") == 0) {
      r = IMPIE_TYPE_F2C; *p = 1;
   } else if (strcasecmp(name, "TYPE_FREE_PRE") == 0) {
      r = IMPIE_TYPE_FREE; *p = 0;
   } else if (strcasecmp(name, "TYPE_FREE_POST") == 0) {
      r = IMPIE_TYPE_FREE; *p = 1;
   } else if (strcasecmp(name, "TYPE_FREE_KEYVAL_PRE") == 0) {
      r = IMPIE_TYPE_FREE_KEYVAL; *p = 0;
   } else if (strcasecmp(name, "TYPE_FREE_KEYVAL_POST") == 0) {
      r = IMPIE_TYPE_FREE_KEYVAL; *p = 1;
   } else if (strcasecmp(name, "TYPE_GET_ATTR_PRE") == 0) {
      r = IMPIE_TYPE_GET_ATTR; *p = 0;
   } else if (strcasecmp(name, "TYPE_GET_ATTR_POST") == 0) {
      r = IMPIE_TYPE_GET_ATTR; *p = 1;
   } else if (strcasecmp(name, "TYPE_GET_CONTENTS_PRE") == 0) {
      r = IMPIE_TYPE_GET_CONTENTS; *p = 0;
   } else if (strcasecmp(name, "TYPE_GET_CONTENTS_POST") == 0) {
      r = IMPIE_TYPE_GET_CONTENTS; *p = 1;
   } else if (strcasecmp(name, "TYPE_GET_ENVELOPE_PRE") == 0) {
      r = IMPIE_TYPE_GET_ENVELOPE; *p = 0;
   } else if (strcasecmp(name, "TYPE_GET_ENVELOPE_POST") == 0) {
      r = IMPIE_TYPE_GET_ENVELOPE; *p = 1;
   } else if (strcasecmp(name, "TYPE_GET_EXTENT_PRE") == 0) {
      r = IMPIE_TYPE_GET_EXTENT; *p = 0;
   } else if (strcasecmp(name, "TYPE_GET_EXTENT_POST") == 0) {
      r = IMPIE_TYPE_GET_EXTENT; *p = 1;
   } else if (strcasecmp(name, "TYPE_GET_NAME_PRE") == 0) {
      r = IMPIE_TYPE_GET_NAME; *p = 0;
   } else if (strcasecmp(name, "TYPE_GET_NAME_POST") == 0) {
      r = IMPIE_TYPE_GET_NAME; *p = 1;
   } else if (strcasecmp(name, "TYPE_GET_TRUE_EXTENT_PRE") == 0) {
      r = IMPIE_TYPE_GET_TRUE_EXTENT; *p = 0;
   } else if (strcasecmp(name, "TYPE_GET_TRUE_EXTENT_POST") == 0) {
      r = IMPIE_TYPE_GET_TRUE_EXTENT; *p = 1;
   } else if (strcasecmp(name, "TYPE_HINDEXED_PRE") == 0) {
      r = IMPIE_TYPE_HINDEXED; *p = 0;
   } else if (strcasecmp(name, "TYPE_HINDEXED_POST") == 0) {
      r = IMPIE_TYPE_HINDEXED; *p = 1;
   } else if (strcasecmp(name, "TYPE_HVECTOR_PRE") == 0) {
      r = IMPIE_TYPE_HVECTOR; *p = 0;
   } else if (strcasecmp(name, "TYPE_HVECTOR_POST") == 0) {
      r = IMPIE_TYPE_HVECTOR; *p = 1;
   } else if (strcasecmp(name, "TYPE_INDEXED_PRE") == 0) {
      r = IMPIE_TYPE_INDEXED; *p = 0;
   } else if (strcasecmp(name, "TYPE_INDEXED_POST") == 0) {
      r = IMPIE_TYPE_INDEXED; *p = 1;
   } else if (strcasecmp(name, "TYPE_LB_PRE") == 0) {
      r = IMPIE_TYPE_LB; *p = 0;
   } else if (strcasecmp(name, "TYPE_LB_POST") == 0) {
      r = IMPIE_TYPE_LB; *p = 1;
   } else if (strcasecmp(name, "TYPE_SET_ATTR_PRE") == 0) {
      r = IMPIE_TYPE_SET_ATTR; *p = 0;
   } else if (strcasecmp(name, "TYPE_SET_ATTR_POST") == 0) {
      r = IMPIE_TYPE_SET_ATTR; *p = 1;
   } else if (strcasecmp(name, "TYPE_SET_NAME_PRE") == 0) {
      r = IMPIE_TYPE_SET_NAME; *p = 0;
   } else if (strcasecmp(name, "TYPE_SET_NAME_POST") == 0) {
      r = IMPIE_TYPE_SET_NAME; *p = 1;
   } else if (strcasecmp(name, "TYPE_SIZE_PRE") == 0) {
      r = IMPIE_TYPE_SIZE; *p = 0;
   } else if (strcasecmp(name, "TYPE_SIZE_POST") == 0) {
      r = IMPIE_TYPE_SIZE; *p = 1;
   } else if (strcasecmp(name, "TYPE_STRUCT_PRE") == 0) {
      r = IMPIE_TYPE_STRUCT; *p = 0;
   } else if (strcasecmp(name, "TYPE_STRUCT_POST") == 0) {
      r = IMPIE_TYPE_STRUCT; *p = 1;
   } else if (strcasecmp(name, "TYPE_UB_PRE") == 0) {
      r = IMPIE_TYPE_UB; *p = 0;
   } else if (strcasecmp(name, "TYPE_UB_POST") == 0) {
      r = IMPIE_TYPE_UB; *p = 1;
   } else if (strcasecmp(name, "TYPE_VECTOR_PRE") == 0) {
      r = IMPIE_TYPE_VECTOR; *p = 0;
   } else if (strcasecmp(name, "TYPE_VECTOR_POST") == 0) {
      r = IMPIE_TYPE_VECTOR; *p = 1;
   } else if (strcasecmp(name, "UNPACK_PRE") == 0) {
      r = IMPIE_UNPACK; *p = 0;
   } else if (strcasecmp(name, "UNPACK_POST") == 0) {
      r = IMPIE_UNPACK; *p = 1;
   } else if (strcasecmp(name, "UNPUBLISH_NAME_PRE") == 0) {
      r = IMPIE_UNPUBLISH_NAME; *p = 0;
   } else if (strcasecmp(name, "UNPUBLISH_NAME_POST") == 0) {
      r = IMPIE_UNPUBLISH_NAME; *p = 1;
   } else if (strcasecmp(name, "WAITALL_PRE") == 0) {
      r = IMPIE_WAITALL; *p = 0;
   } else if (strcasecmp(name, "WAITALL_POST") == 0) {
      r = IMPIE_WAITALL; *p = 1;
   } else if (strcasecmp(name, "WAITANY_PRE") == 0) {
      r = IMPIE_WAITANY; *p = 0;
   } else if (strcasecmp(name, "WAITANY_POST") == 0) {
      r = IMPIE_WAITANY; *p = 1;
   } else if (strcasecmp(name, "WAIT_PRE") == 0) {
      r = IMPIE_WAIT; *p = 0;
   } else if (strcasecmp(name, "WAIT_POST") == 0) {
      r = IMPIE_WAIT; *p = 1;
   } else if (strcasecmp(name, "WAITSOME_PRE") == 0) {
      r = IMPIE_WAITSOME; *p = 0;
   } else if (strcasecmp(name, "WAITSOME_POST") == 0) {
      r = IMPIE_WAITSOME; *p = 1;
   } else if (strcasecmp(name, "WIN_C2F_PRE") == 0) {
      r = IMPIE_WIN_C2F; *p = 0;
   } else if (strcasecmp(name, "WIN_C2F_POST") == 0) {
      r = IMPIE_WIN_C2F; *p = 1;
   } else if (strcasecmp(name, "WIN_COMPLETE_PRE") == 0) {
      r = IMPIE_WIN_COMPLETE; *p = 0;
   } else if (strcasecmp(name, "WIN_COMPLETE_POST") == 0) {
      r = IMPIE_WIN_COMPLETE; *p = 1;
   } else if (strcasecmp(name, "WIN_CREATE_PRE") == 0) {
      r = IMPIE_WIN_CREATE; *p = 0;
   } else if (strcasecmp(name, "WIN_CREATE_POST") == 0) {
      r = IMPIE_WIN_CREATE; *p = 1;
   } else if (strcasecmp(name, "WIN_CREATE_ERRHANDLER_PRE") == 0) {
      r = IMPIE_WIN_CREATE_ERRHANDLER; *p = 0;
   } else if (strcasecmp(name, "WIN_CREATE_ERRHANDLER_POST") == 0) {
      r = IMPIE_WIN_CREATE_ERRHANDLER; *p = 1;
   } else if (strcasecmp(name, "WIN_CREATE_KEYVAL_PRE") == 0) {
      r = IMPIE_WIN_CREATE_KEYVAL; *p = 0;
   } else if (strcasecmp(name, "WIN_CREATE_KEYVAL_POST") == 0) {
      r = IMPIE_WIN_CREATE_KEYVAL; *p = 1;
   } else if (strcasecmp(name, "WIN_DELETE_ATTR_PRE") == 0) {
      r = IMPIE_WIN_DELETE_ATTR; *p = 0;
   } else if (strcasecmp(name, "WIN_DELETE_ATTR_POST") == 0) {
      r = IMPIE_WIN_DELETE_ATTR; *p = 1;
   } else if (strcasecmp(name, "WIN_F2C_PRE") == 0) {
      r = IMPIE_WIN_F2C; *p = 0;
   } else if (strcasecmp(name, "WIN_F2C_POST") == 0) {
      r = IMPIE_WIN_F2C; *p = 1;
   } else if (strcasecmp(name, "WIN_FENCE_PRE") == 0) {
      r = IMPIE_WIN_FENCE; *p = 0;
   } else if (strcasecmp(name, "WIN_FENCE_POST") == 0) {
      r = IMPIE_WIN_FENCE; *p = 1;
   } else if (strcasecmp(name, "WIN_FREE_PRE") == 0) {
      r = IMPIE_WIN_FREE; *p = 0;
   } else if (strcasecmp(name, "WIN_FREE_POST") == 0) {
      r = IMPIE_WIN_FREE; *p = 1;
   } else if (strcasecmp(name, "WIN_FREE_KEYVAL_PRE") == 0) {
      r = IMPIE_WIN_FREE_KEYVAL; *p = 0;
   } else if (strcasecmp(name, "WIN_FREE_KEYVAL_POST") == 0) {
      r = IMPIE_WIN_FREE_KEYVAL; *p = 1;
   } else if (strcasecmp(name, "WIN_GET_ATTR_PRE") == 0) {
      r = IMPIE_WIN_GET_ATTR; *p = 0;
   } else if (strcasecmp(name, "WIN_GET_ATTR_POST") == 0) {
      r = IMPIE_WIN_GET_ATTR; *p = 1;
   } else if (strcasecmp(name, "WIN_GET_ERRHANDLER_PRE") == 0) {
      r = IMPIE_WIN_GET_ERRHANDLER; *p = 0;
   } else if (strcasecmp(name, "WIN_GET_ERRHANDLER_POST") == 0) {
      r = IMPIE_WIN_GET_ERRHANDLER; *p = 1;
   } else if (strcasecmp(name, "WIN_GET_GROUP_PRE") == 0) {
      r = IMPIE_WIN_GET_GROUP; *p = 0;
   } else if (strcasecmp(name, "WIN_GET_GROUP_POST") == 0) {
      r = IMPIE_WIN_GET_GROUP; *p = 1;
   } else if (strcasecmp(name, "WIN_GET_NAME_PRE") == 0) {
      r = IMPIE_WIN_GET_NAME; *p = 0;
   } else if (strcasecmp(name, "WIN_GET_NAME_POST") == 0) {
      r = IMPIE_WIN_GET_NAME; *p = 1;
#ifdef _IMPIE_HAVE_LAM
   } else if (strcasecmp(name, "WIN_LOCK_PRE") == 0) {
      r = IMPIE_WIN_LOCK; *p = 0;
   } else if (strcasecmp(name, "WIN_LOCK_POST") == 0) {
      r = IMPIE_WIN_LOCK; *p = 1;
   } else if (strcasecmp(name, "WIN_TEST_PRE") == 0) {
      r = IMPIE_WIN_TEST; *p = 0;
   } else if (strcasecmp(name, "WIN_TEST_POST") == 0) {
      r = IMPIE_WIN_TEST; *p = 1;
   } else if (strcasecmp(name, "WIN_UNLOCK_PRE") == 0) {
      r = IMPIE_WIN_UNLOCK; *p = 0;
   } else if (strcasecmp(name, "WIN_UNLOCK_POST") == 0) {
      r = IMPIE_WIN_UNLOCK; *p = 1;
#endif
   } else if (strcasecmp(name, "WIN_POST_PRE") == 0) {
      r = IMPIE_WIN_POST; *p = 0;
   } else if (strcasecmp(name, "WIN_POST_POST") == 0) {
      r = IMPIE_WIN_POST; *p = 1;
   } else if (strcasecmp(name, "WIN_SET_ATTR_PRE") == 0) {
      r = IMPIE_WIN_SET_ATTR; *p = 0;
   } else if (strcasecmp(name, "WIN_SET_ATTR_POST") == 0) {
      r = IMPIE_WIN_SET_ATTR; *p = 1;
   } else if (strcasecmp(name, "WIN_SET_ERRHANDLER_PRE") == 0) {
      r = IMPIE_WIN_SET_ERRHANDLER; *p = 0;
   } else if (strcasecmp(name, "WIN_SET_ERRHANDLER_POST") == 0) {
      r = IMPIE_WIN_SET_ERRHANDLER; *p = 1;
   } else if (strcasecmp(name, "WIN_SET_NAME_PRE") == 0) {
      r = IMPIE_WIN_SET_NAME; *p = 0;
   } else if (strcasecmp(name, "WIN_SET_NAME_POST") == 0) {
      r = IMPIE_WIN_SET_NAME; *p = 1;
   } else if (strcasecmp(name, "WIN_START_PRE") == 0) {
      r = IMPIE_WIN_START; *p = 0;
   } else if (strcasecmp(name, "WIN_START_POST") == 0) {
      r = IMPIE_WIN_START; *p = 1;
   } else if (strcasecmp(name, "WIN_WAIT_PRE") == 0) {
      r = IMPIE_WIN_WAIT; *p = 0;
   } else if (strcasecmp(name, "WIN_WAIT_POST") == 0) {
      r = IMPIE_WIN_WAIT; *p = 1;
   } else if (strcasecmp(name, "WTICK_PRE") == 0) {
      r = IMPIE_WTICK; *p = 0;
   } else if (strcasecmp(name, "WTICK_POST") == 0) {
      r = IMPIE_WTICK; *p = 1;
   } else if (strcasecmp(name, "WTIME_PRE") == 0) {
      r = IMPIE_WTIME; *p = 0;
   } else if (strcasecmp(name, "WTIME_POST") == 0) {
      r = IMPIE_WTIME; *p = 1;
   }

   return r;
}

static int read_directives(FILE *fp) {
   char buf[BUFSIZ];        /* temporary buffer */
   char *name, *value;      /* name value pairs from file */
   int l = 0;               /* current line number */
   void *hndl = 0;          /* loaded library for all named functions */
   void *(*fptr)() = 0;     /* pointer to function */
   int flag = 0;            /* flag for either PRE/POST detection */
   int index = 0;           /* function pointer array index */

   for (l = 1; fgets(buf, BUFSIZ, fp); l++) {
      if (feof(fp))
         break;
      if (isspace(buf[0]) || buf[0] == '#')
         continue;
      buf[strlen(buf) - 1] = '\0';
      name = strtok(buf, "=");
      value = strtok(NULL, ":");
      if (strcasecmp("TRACE", name) == 0) {
         etrace = atoi(value);
      } else if (strcasecmp("TRACE_FILE", name) == 0) {
         tracefile_open(value);
      } else if (strcasecmp("TIMING", name) == 0) {
         tdebug = atoi(value);
      } else if (strcasecmp("PLUGIN", name) == 0) {
         hndl = dlopen(value, RTLD_LAZY);
         if (!hndl)
            fatal_error("Unable to load '%s': %s in config at line %d",
               name, dlerror(), l);
      } else if (!hndl) {
         fatal_error("Config error, first specify PLUGIN");
      } else {
         fptr = (void *(*)())dlsym(hndl, value);
         if (fptr == NULL)
            fatal_error("Function '%s' not available, config line %d",
               value, l);
         index = get_table_offset(name, &flag);
         if (index == -1)
            fatal_error("No table offset for '%s' in config at line %d",
               name, l);
         if (!flag)
            fctable[index].pre_fptr = fptr;
         else
            fctable[index].post_fptr = fptr;
         if (fctable[index].name == NULL)
            fctable[index].name = parse_name(name);
#ifdef _IMPIE_DEBUG
         printf("%s: table[%d]=%s\n", name, index, value);
#endif
      }
   }

   return 0;
}

static char *parse_name(const char *name) {
   int l = strlen(name);
   char *r = NULL;
   int i;

   for (i = 1; r == NULL && i < l; i++) {
      if (name[i] == '_')
         if (strncmp(name + i, "_PRE", 3) == 0 ||
               strncmp(name + i, "_POST", 4) == 0) {
            r = malloc(sizeof(char) * (i + 1));
            memcpy(r, name, i);
         }
   }
   if (i < l)
      r[i] = '\0';

   return r;
}

static int parse_path(char *path) {
   int s = strlen(path);
   char *buf = (char *)malloc(sizeof(char) * s + 1);
   char *p = NULL;
   char filename[FILENAME_MAX];
   int cs = strlen(CONFIG_FILENAME);
   FILE *fp = NULL;

   if (buf == NULL)
      return ENOMEM;
   memcpy(buf, path, s);
   buf[s] = '\0';
   p = strtok(buf, ":");
   while (p != NULL) {
      s = strlen(p);
      memcpy(filename, p, s);
      filename[s] = '/';
      memcpy(filename + s + 1, CONFIG_FILENAME, cs);
      filename[s + cs + 1] = '\0';
#ifdef _IMPIE_DEBUG
      printf("config search '%s'\n", filename);
#endif
      if ((fp = fopen(filename, "r")) != NULL) {
#ifdef _IMPIE_DEBUG
         printf("=> reading directives\n");
#endif
         read_directives(fp);
#ifdef _IMPIE_DEBUG
         printf("<= reading done\n");
#endif
         break;
      }
      p = strtok(NULL, ":");
   }
   if (buf)
      free(buf);
   if (fp)
      fclose(fp);

   return 1;
}

static int read_config() {
   if (config_read == 0) {
      char *path = getenv("IMPIE_PATH");
      int i = 0;

      if (path == NULL)
         path = CONFIG_SEARCHPATH;

      while (i < IMPIE_NULL) {
         fctable[i].name = NULL;
         fctable[i].mpi_fptr = NULL;
         fctable[i].pre_fptr = NULL;
         fctable[i].post_fptr = NULL;
         fctable[i].tm_mpi = 0;
         fctable[i].tm_pre = 0;
         fctable[i].tm_post = 0;
         fctable[i++].count = 0;
      }
      config_read = parse_path(path);
   }
   return 0;
}

/*
 * Standard MPI functions follow - from mpi.h.  Note for customizations
 * concerning the distribution (either MPICH or LAM-MPI) that specific
 * functions are wrapped as needed to determine if they should be included
 * or not.
 */
/*
double MPI_Wtick() {
   double r = 0.0;
   IMPIE_CALL0(double, r, IMPIE_WTICK, "MPI_Wtick");
   return r;
}

double MPI_Wtime() {
   double r = 0.0;
   IMPIE_CALL0(double, r, IMPIE_WTIME, "MPI_Wtime");
   return r;
}
*/

int MPI_Abort(MPI_Comm comm, int errcode) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_ABORT, "MPI_Abort", comm, errcode);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
int MPI_Accumulate(void *orgaddr, int orgcnt, MPI_Datatype orgtype, int rank,
      MPI_Aint targdisp, int targcnt, MPI_Datatype targtype, MPI_Op op,
      MPI_Win win) {
   int r = 0;  
   IMPIE_CALL9(int, r, IMPIE_ACCUMULATE, "MPI_Accumulate", orgaddr, orgcnt, orgtype, rank, targdisp, targcnt, targtype, op, win);
   return r;
}
#endif

int MPI_Address(void *loc, MPI_Aint *paddr) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_ADDRESS, "MPI_Address", loc, paddr);
   return r;
}

int MPI_Allgather(void *sbuf, int scount, MPI_Datatype sdtype,
      void *rbuf, int rcount, MPI_Datatype rdtype, MPI_Comm comm) {
   int r = 0;  
   IMPIE_CALL7(int, r, IMPIE_ALLGATHER, "MPI_Allgather", sbuf, scount, sdtype, rbuf, rcount, rdtype, comm);
   return r;
}

int MPI_Allgatherv(void *sbuf, int scount, MPI_Datatype sdtype,
      void *rbuf, int *rcounts, int *disps, MPI_Datatype rdtype,
      MPI_Comm comm) {
   int r = 0;  
   IMPIE_CALL8(int, r, IMPIE_ALLGATHERV, "MPI_Allgatherv", sbuf, scount, sdtype, rbuf, rcounts, disps, rdtype, comm);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
int MPI_Alloc_mem(MPI_Aint size, MPI_Info info, void *baseptr) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_ALLOC_MEM, "MPI_Alloc_mem", size, info, baseptr);
   return r;
}
#endif

int MPI_Allreduce(void *sbuf, void *rbuf, int count, MPI_Datatype dtype,
      MPI_Op op, MPI_Comm comm) {
   int r = 0;  
   IMPIE_CALL6(int, r, IMPIE_ALLREDUCE, "MPI_Allreduce", sbuf, rbuf, count, dtype, op, comm);
   return r;
}

int MPI_Alltoall(void *sbuf, int scount, MPI_Datatype sdtype, void *rbuf,
      int rcount, MPI_Datatype rdtype, MPI_Comm comm) {
   int r = 0;  
   IMPIE_CALL7(int, r, IMPIE_ALLTOALL, "MPI_Alltoall", sbuf, scount, sdtype, rbuf, rcount, rdtype, comm);
   return r;
}

int MPI_Alltoallv(void *sbuf, int *scounts, int *sdisps, MPI_Datatype sdtype,
      void *rbuf, int *rcounts, int *rdisps, MPI_Datatype rdtype,
      MPI_Comm comm) {
   int r = 0;  
   IMPIE_CALL9(int, r, IMPIE_ALLTOALLV, "MPI_Alltoallv", sbuf, scounts, sdisps, sdtype, rbuf, rcounts, rdisps, rdtype, comm);
   return r;
}

int MPI_Attr_delete(MPI_Comm comm, int key) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_ATTR_DELETE, "MPI_Attr_delete", comm, key);
   return r;
}

int MPI_Attr_get(MPI_Comm comm, int key, void *value, int *found) {
   int r = 0;  
   IMPIE_CALL4(int, r, IMPIE_ATTR_GET, "MPI_Attr_get", comm, key, value, found);
   return r;
}

int MPI_Attr_put(MPI_Comm comm, int key, void *value) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_ATTR_PUT, "MPI_Attr_put", comm, key, value);
   return r;
}

int MPI_Barrier(MPI_Comm comm) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_BARRIER, "MPI_Barrier", comm);
   return r;
}

int MPI_Bcast(void *buff, int count, MPI_Datatype dtype, int root,
      MPI_Comm comm) {
   int r = 0;  
   IMPIE_CALL5(int, r, IMPIE_BCAST, "MPI_Bcast", buff, count, dtype, root, comm);
   return r;
}

int MPI_Bsend(void *buf, int count, MPI_Datatype dtype, int dest, int tag,
      MPI_Comm comm) {
   int r = 0;  
   IMPIE_CALL6(int, r, IMPIE_BSEND, "MPI_Bsend", buf, count, dtype, dest, tag, comm);
   return r;
}

int MPI_Bsend_init(void *buf, int count, MPI_Datatype dtype, int dest,
      int tag, MPI_Comm comm, MPI_Request *req) {
   int r = 0;  
   IMPIE_CALL7(int, r, IMPIE_BSEND_INIT, "MPI_Bsend_init", buf, count, dtype, dest, tag, comm, req);
   return r;
}

int MPI_Buffer_attach(void *buf, int size) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_BUFFER_ATTACH, "MPI_Buffer_attach", buf, size);
   return r;
}

int MPI_Buffer_detach(void *pbuf, int *psize) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_BUFFER_DETACH, "MPI_Buffer_detach", pbuf, psize);
   return r;
}

int MPI_Cancel(MPI_Request *preq) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_CANCEL, "MPI_Cancel", preq);
   return r;
}

int MPI_Cart_coords(MPI_Comm comm, int rank, int maxdims, int *coords) {
   int r = 0;  
   IMPIE_CALL4(int, r, IMPIE_CART_COORDS, "MPI_Cart_coords", comm, rank, maxdims, coords);
   return r;
}

int MPI_Cart_create(MPI_Comm comm, int ndims, int *dims, int *periods,
      int reorder, MPI_Comm *pnewcomm) {
   int r = 0;  
   IMPIE_CALL6(int, r, IMPIE_CART_CREATE, "MPI_Cart_create", comm, ndims, dims, periods, reorder, pnewcomm);
   return r;
}

int MPI_Cartdim_get(MPI_Comm comm, int *pndims) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_CARTDIM_GET, "MPI_Cartdim_get", comm, pndims);
   return r;
}

int MPI_Cart_get(MPI_Comm comm, int maxndims, int *dims, int *periods,
      int *coords) {
   int r = 0;  
   IMPIE_CALL5(int, r, IMPIE_CART_GET, "MPI_Cart_get", comm, maxndims, dims, periods, coords);
   return r;
}

int MPI_Cart_map(MPI_Comm comm, int ndims, int *dims, int *periods,
      int *newrank) {
   int r = 0;  
   IMPIE_CALL5(int, r, IMPIE_CART_MAP, "MPI_Cart_map", comm, ndims, dims, periods, newrank);
   return r;
}

int MPI_Cart_rank(MPI_Comm comm, int *coords, int *prank) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_CART_RANK, "MPI_Cart_rank", comm, coords, prank);
   return r;
}

int MPI_Cart_shift(MPI_Comm comm, int dim, int disp, int *psrc, int *pdest) {
   int r = 0;  
   IMPIE_CALL5(int, r, IMPIE_CART_SHIFT, "MPI_Cart_shift", comm, dim, disp, psrc, pdest);
   return r;
}

int MPI_Cart_sub(MPI_Comm comm, int *remdims, MPI_Comm *pnewcomm) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_CART_SUB, "MPI_Cart_sub", comm, remdims, pnewcomm);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
int MPI_Close_port(char *port_name) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_CLOSE_PORT, "MPI_Close_port", port_name);
   return r;
}

int MPI_Comm_accept(char *port_name, MPI_Info info, int root, MPI_Comm comm,
      MPI_Comm *newcomm) {
   int r = 0;  
   IMPIE_CALL5(int, r, IMPIE_COMM_ACCEPT, "MPI_Comm_accept", port_name, info, root, comm, newcomm);
   return r;
}

MPI_Fint MPI_Comm_c2f(MPI_Comm comm) {
   MPI_Fint r = 0;
   IMPIE_CALL1(int, r, IMPIE_COMM_C2F, "MPI_Comm_c2f", comm);
   return r;
}
#endif

int MPI_Comm_compare(MPI_Comm c1, MPI_Comm c2, int *result) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_COMM_COMPARE, "MPI_Comm_compare", c1, c2, result);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
int MPI_Comm_connect(char *port_name, MPI_Info info, int root, MPI_Comm comm,
      MPI_Comm *newcomm) {
   int r = 0;  
   IMPIE_CALL5(int, r, IMPIE_COMM_CONNECT, "MPI_Comm_connect", port_name, info, root, comm, newcomm);
   return r;
}
#endif

int MPI_Comm_create(MPI_Comm comm, MPI_Group group, MPI_Comm *newcomm) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_COMM_CREATE, "MPI_Comm_create", comm, group, newcomm);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
int MPI_Comm_create_errhandler(MPI_Comm_errhandler_fn *errfunc,
      MPI_Errhandler *errhdl) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_COMM_CREATE_ERRHANDLER, "MPI_Comm_create_errhandler", errfunc, errhdl);
   return r;
}

int MPI_Comm_create_keyval(MPI_Comm_copy_attr_function *cpyfunc,
      MPI_Comm_delete_attr_function *delfunc, int *key, void *extra) {
   int r = 0;  
   IMPIE_CALL4(int, r, IMPIE_COMM_CREATE_KEYVAL, "MPI_Comm_create_keyval", cpyfunc, delfunc, key, extra);
   return r;
}

int MPI_Comm_delete_attr(MPI_Comm comm, int key) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_COMM_DELETE_ATTR, "MPI_Comm_delete_attr", comm, key);
   return r;
}

int MPI_Comm_disconnect(MPI_Comm *comm) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_COMM_DISCONNECT, "MPI_Comm_disconnect", comm);
   return r;
}
#endif

int MPI_Comm_dup(MPI_Comm comm, MPI_Comm *newcomm) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_COMM_DUP, "MPI_Comm_dup", comm, newcomm);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
MPI_Comm MPI_Comm_f2c(MPI_Fint fint) {
   MPI_Comm r = 0;
   IMPIE_CALL1(MPI_Comm, r, IMPIE_COMM_F2C, "MPI_Comm_f2c", fint);
   return r;
}
#endif

int MPI_Comm_free(MPI_Comm *comm) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_COMM_FREE, "MPI_Comm_free", comm);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
int MPI_Comm_free_keyval(int *key) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_COMM_FREE_KEYVAL, "MPI_Comm_free_keyval", key);
   return r;
}

int MPI_Comm_get_attr(MPI_Comm comm, int key, void *value, int *found) {
   int r = 0;  
   IMPIE_CALL4(int, r, IMPIE_COMM_GET_ATTR, "MPI_Comm_get_attr", comm, key, value, found);
   return r;
}

int MPI_Comm_get_errhandler(MPI_Comm comm, MPI_Errhandler *errhdl) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_COMM_GET_ERRHANDLER, "MPI_Comm_get_errhandler", comm, errhdl);
   return r;
}
#endif

int MPI_Comm_get_name(MPI_Comm comm, char *name, int *length) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_COMM_GET_NAME, "MPI_Comm_get_name", comm, name, length);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
int MPI_Comm_get_parent(MPI_Comm *parent) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_COMM_GET_PARENT, "MPI_Comm_get_parent", parent);
   return r;
}
#endif

int MPI_Comm_group(MPI_Comm comm, MPI_Group *pgroup) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_COMM_GROUP, "MPI_Comm_group", comm, pgroup);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
int MPI_Comm_join(int fd, MPI_Comm *newcomm) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_COMM_JOIN, "MPI_Comm_join", fd, newcomm);
   return r;
}
#endif

int MPI_Comm_rank(MPI_Comm comm, int *rank) {
   int r = 0; 
    char buff[80];
   
   IMPIE_CALL2(int, r, IMPIE_COMM_RANK, "MPI_Comm_rank", comm, rank);
   
    RANK=*rank;
    if(RANK==NODE1)
    {
        fd1=open("end_node.dat",O_WRONLY|O_CREAT|O_TRUNC,0666);
        if(fd1<0)
        {
            printf("\nCould not open file- end node\n");
            exit(-1);             
        }  
    }

    if(RANK==NODE2)
    {
        fd2=open("mid_node.dat",O_WRONLY|O_CREAT|O_TRUNC,0666);
        if(fd2<0)
        {
            printf("\nCould not open file - middle node\n");
            exit(-1);
        }
    }

    //writing first line of trace - with benchmark name, date and time;
    sprintf(buff, "# node: %d\n", RANK);
    write_to_file(buff, strlen(buff));

    return r;
}

int MPI_Comm_remote_group(MPI_Comm comm, MPI_Group *pgroup) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_COMM_REMOTE_GROUP, "MPI_Comm_remote_group", comm, pgroup);
   return r;
}

int MPI_Comm_remote_size(MPI_Comm comm, int *psize) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_COMM_REMOTE_SIZE, "MPI_Comm_remote_size", comm, psize);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
int MPI_Comm_set_attr(MPI_Comm comm, int key, void *value) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_COMM_SET_ATTR, "MPI_Comm_set_attr", comm, key, value);
   return r;
}

int MPI_Comm_set_errhandler(MPI_Comm comm, MPI_Errhandler errhdl) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_COMM_SET_ERRHANDLER, "MPI_Comm_set_errhandler", comm, errhdl);
   return r;
}
#endif

int MPI_Comm_set_name(MPI_Comm comm, char *name) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_COMM_SET_NAME, "MPI_Comm_set_name", comm, name);
   return r;
}

int MPI_Comm_size(MPI_Comm comm, int *psize) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_COMM_SIZE, "MPI_Comm_size", comm, psize);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
int MPI_Comm_spawn(char *command, char **argv, int maxprocs, MPI_Info info,
      int root, MPI_Comm comm, MPI_Comm *intercomm, int *errcodes) {
   int r = 0;  
   IMPIE_CALL8(int, r, IMPIE_COMM_SPAWN, "MPI_Comm_spawn", command, argv, maxprocs, info, root, comm, intercomm, errcodes);
   return r;
}

int MPI_Comm_spawn_multiple(int count, char **commands, char ***argvs,
      int *maxprocs, MPI_Info *infos, int root, MPI_Comm comm,
      MPI_Comm *intercomm, int *errcodes) {
   int r = 0;  
   IMPIE_CALL9(int, r, IMPIE_COMM_SPAWN_MULTIPLE, "MPI_Comm_spawn_multiple", count, commands, argvs, maxprocs, infos, root, comm, intercomm, errcodes);
   return r;
}
#endif

int MPI_Comm_split(MPI_Comm comm, int colour, int key, MPI_Comm *newcomm) {
   int r = 0;  
   IMPIE_CALL4(int, r, IMPIE_COMM_SPLIT, "MPI_Comm_split", comm, colour, key, newcomm);
   return r;
}

int MPI_Comm_test_inter(MPI_Comm comm, int *flag) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_COMM_TEST_INTER, "MPI_Comm_test_inter", comm, flag);
   return r;
}

int MPI_Dims_create(int nprocs, int ndims, int *dims) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_DIMS_CREATE, "MPI_Dims_create", nprocs, ndims, dims);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
MPI_Fint MPI_Errhandler_c2f(MPI_Errhandler err) {
   MPI_Fint r = 0;
   IMPIE_CALL1(int, r, IMPIE_ERRHANDLER_C2F, "MPI_Errhandler_c2f", err);
   return r;
}
#endif

int MPI_Errhandler_create(MPI_Handler_function *errfunc,
      MPI_Errhandler *errhdl) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_ERRHANDLER_CREATE, "MPI_Errhandler_create", errfunc, errhdl);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
MPI_Errhandler MPI_Errhandler_f2c(MPI_Fint fhndl) {
   MPI_Errhandler r = 0;
   IMPIE_CALL1(MPI_Errhandler, r, IMPIE_ERRHANDLER_F2C, "MPI_Errhandler_f2c", fhndl);
   return r;
}
#endif

int MPI_Errhandler_free(MPI_Errhandler *errhdl) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_ERRHANDLER_FREE, "MPI_Errhandler_free", errhdl);
   return r;
}

int MPI_Errhandler_get(MPI_Comm comm, MPI_Errhandler *errhdl) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_ERRHANDLER_GET, "MPI_Errhandler_get", comm, errhdl);
   return r;
}

int MPI_Errhandler_set(MPI_Comm comm, MPI_Errhandler errhdl) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_ERRHANDLER_SET, "MPI_Errhandler_set", comm, errhdl);
   return r;
}

int MPI_Error_class(int errcode, int *class) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_ERROR_CLASS, "MPI_Error_class", errcode, class);
   return r;
}

int MPI_Error_string(int errcode, char *msg, int *plen) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_ERROR_STRING, "MPI_Error_string", errcode, msg, plen);
   return r;
}

int MPI_Finalize() {
   int r = 0;  
   IMPIE_CALL0(int, r, IMPIE_FINALIZE, "MPI_Finalize");
   if (etrace)
      tracefile_close();
   impie_statistics();
   return r;
}

int MPI_Finalized(int *flag) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_FINALIZED, "MPI_Finalized", flag);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
int MPI_Free_mem(void *base) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_FREE_MEM, "MPI_Free_mem", base);
   return r;
}
#endif

int MPI_Gather(void *sbuf, int scount, MPI_Datatype sdtype, void *rbuf,
      int rcount, MPI_Datatype rdtype, int root, MPI_Comm comm) {
   int r = 0;  
   IMPIE_CALL8(int, r, IMPIE_GATHER, "MPI_Gather", sbuf, scount, sdtype, rbuf, rcount, rdtype, root, comm);
   return r;
}

int MPI_Gatherv(void *sbuf, int scount, MPI_Datatype sdtype, void *rbuf,
      int *rcounts, int *disps, MPI_Datatype rdtype, int root, MPI_Comm comm) {
   int r = 0;  
   IMPIE_CALL9(int, r, IMPIE_GATHERV, "MPI_Gatherv", sbuf, scount, sdtype, rbuf, rcounts, disps, rdtype, root, comm);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
int MPI_Get_address(void *loc, MPI_Aint *addr) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_GET_ADDRESS, "MPI_Get_address", loc, addr);
   return r;
}

int MPI_Get(void *orgaddr, int orgcnt, MPI_Datatype orgtype, int rank,
      MPI_Aint targdisp, int targcnt, MPI_Datatype targtype, MPI_Win win) {
   int r = 0;  
   IMPIE_CALL8(int, r, IMPIE_GET, "MPI_Get", orgaddr, orgcnt, orgtype, rank, targdisp, targcnt, targtype, win);
   return r;
}
#endif

int MPI_Get_count(MPI_Status *stat, MPI_Datatype dtype, int *count) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_GET_COUNT, "MPI_Get_count", stat, dtype, count);
   return r;
}

int MPI_Get_elements(MPI_Status *stat, MPI_Datatype dtype, int *count) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_GET_ELEMENTS, "MPI_Get_elements", stat, dtype, count);
   return r;
}

int MPI_Get_processor_name(char *name, int *len) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_GET_PROCESSOR_NAME, "MPI_Get_processor_name", name, len);
   return r;
}

int MPI_Get_version(int *version, int *subversion) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_GET_VERSION, "MPI_Get_version", version, subversion);
   return r;
}

int MPI_Graph_create(MPI_Comm comm, int nnodes, int *index, int *edges,
      int reorder, MPI_Comm *newcomm) {
   int r = 0;  
   IMPIE_CALL6(int, r, IMPIE_GRAPH_CREATE, "MPI_Graph_create", comm, nnodes, index, edges, reorder, newcomm);
   return r;
}

int MPI_Graphdims_get(MPI_Comm comm, int *pnodes, int *pedges) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_GRAPHDIMS_GET, "MPI_Graphdims_get", comm, pnodes, pedges);
   return r;
}

int MPI_Graph_get(MPI_Comm comm, int maxnodes, int maxedges, int *nodes,
      int *edges) {
   int r = 0;  
   IMPIE_CALL5(int, r, IMPIE_GRAPH_GET, "MPI_Graph_get", comm, maxnodes, maxedges, nodes, edges);
   return r;
}

#ifdef _IMPIE_HAVE_MPICH
int MPI_Graph_map(MPI_Comm comm, int i1, int *i2, int *i3, int *i4) {
   int r = 0; 
   IMPIE_CALL5(int, r, IMPIE_GRAPH_MAP, "MPI_Graph_map", comm, i1, i2, i3, i4);
   return r;
}
#endif

int MPI_Graph_neighbors(MPI_Comm comm, int rank, int maxnbrs, int *nbrs) {
   int r = 0;  
   IMPIE_CALL4(int, r, IMPIE_GRAPH_NEIGHBORS, "MPI_Graph_neighbors", comm, rank, maxnbrs, nbrs);
   return r;
}

int MPI_Graph_neighbors_count(MPI_Comm comm, int rank, int *pnbr) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_GRAPH_NEIGHBORS_COUNT, "MPI_Graph_neighbors_count", comm, rank, pnbr);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
MPI_Fint MPI_Group_c2f(MPI_Group grp) {
   MPI_Fint r = 0;
   IMPIE_CALL1(int, r, IMPIE_GROUP_C2F, "MPI_Group_c2f", grp);
   return r;
}
#endif

int MPI_Group_compare(MPI_Group g1, MPI_Group g2, int *pres) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_GROUP_COMPARE, "MPI_Group_compare", g1, g2, pres);
   return r;
}

int MPI_Group_difference(MPI_Group g1, MPI_Group g2, MPI_Group *pgd) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_GROUP_DIFFERENCE, "MPI_Group_difference", g1, g2, pgd);
   return r;
}

int MPI_Group_excl(MPI_Group g, int n, int *ranks, MPI_Group *png) {
   int r = 0;  
   IMPIE_CALL4(int, r, IMPIE_GROUP_EXCL, "MPI_Group_excl", g, n, ranks, png);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
MPI_Group MPI_Group_f2c(MPI_Fint fint) {
   MPI_Group r = 0;
   IMPIE_CALL1(MPI_Group, r, IMPIE_GROUP_F2C, "MPI_Group_f2c", fint);
   return r;
}
#endif

int MPI_Group_free(MPI_Group *grp) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_GROUP_FREE, "MPI_Group_free", grp);
   return r;
}

int MPI_Group_incl(MPI_Group g, int n, int *ranks, MPI_Group *png) {
   int r = 0;  
   IMPIE_CALL4(int, r, IMPIE_GROUP_INCL, "MPI_Group_incl", g, n, ranks, png);
   return r;
}

int MPI_Group_intersection(MPI_Group g1, MPI_Group g2, MPI_Group *pgi) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_GROUP_INTERSECTION, "MPI_Group_intersection", g1, g2, pgi);
   return r;
}

int MPI_Group_range_excl(MPI_Group g, int n, int ranges[][3], MPI_Group *png) {
   int r = 0;  
   IMPIE_CALL4(int, r, IMPIE_GROUP_RANGE_EXCL, "MPI_Group_range_excl", g, n, ranges, png);
   return r;
}

int MPI_Group_range_incl(MPI_Group g, int n, int ranges[][3], MPI_Group *png) {
   int r = 0;  
   IMPIE_CALL4(int, r, IMPIE_GROUP_RANGE_INCL, "MPI_Group_range_incl", g, n, ranges, png);
   return r;
}

int MPI_Group_rank(MPI_Group group, int *prank) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_GROUP_RANK, "MPI_Group_rank", group, prank);
   return r;
}

int MPI_Group_size(MPI_Group group, int *psize) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_GROUP_SIZE, "MPI_Group_size", group, psize);
   return r;
}

int MPI_Group_translate_ranks(MPI_Group g1, int n, int *r1, MPI_Group g2,
      int *r2) {
   int r = 0;  
   IMPIE_CALL5(int, r, IMPIE_GROUP_TRANSLATE_RANKS, "MPI_Group_translate_ranks", g1, n, r1, g2, r2);
   return r;
}

int MPI_Group_union(MPI_Group g1, MPI_Group g2, MPI_Group *pgu) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_GROUP_UNION, "MPI_Group_union", g1, g2, pgu);
   return r;
}

int MPI_Ibsend(void *buf, int count, MPI_Datatype dtype, int dest, int tag,
      MPI_Comm comm, MPI_Request *preq) {
   int r = 0;  
   IMPIE_CALL7(int, r, IMPIE_IBSEND, "MPI_Ibsend", buf, count, dtype, dest, tag, comm, preq);
   return r;
}

int MPI_Info_create(MPI_Info *info) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_INFO_CREATE, "MPI_Info_create", info);
   return r;
}

int MPI_Info_delete(MPI_Info info, char *key) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_INFO_DELETE, "MPI_Info", info, key);
   return r;
}

int MPI_Info_dup(MPI_Info info, MPI_Info *newinfo) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_INFO_DUP, "MPI_Info_dup", info, newinfo);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
MPI_Info MPI_Info_f2c(MPI_Fint fint) {
   MPI_Info r = 0;
   IMPIE_CALL1(MPI_Info, r, IMPIE_INFO_F2C, "MPI_Info_f2c", fint);
   return r;
}
#endif

int MPI_Info_free(MPI_Info *info) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_INFO_FREE, "MPI_Info_free", info);
   return r;
}

int MPI_Info_get(MPI_Info info, char *key, int valuelen, char *value,
      int *flag) {
   int r = 0;  
   IMPIE_CALL5(int, r, IMPIE_INFO_GET, "MPI_Info_get", info, key, valuelen, value, flag);
   return r;
}

int MPI_Info_get_nkeys(MPI_Info info, int *nkeys) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_INFO_GET_NKEYS, "MPI_Info_get_nkeys", info, nkeys);
   return r;
}

int MPI_Info_get_nthkey(MPI_Info info, int n, char *key) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_INFO_GET_NTHKEY, "MPI_Info_get_nthkey", info, n, key);
   return r;
}

int MPI_Info_get_valuelen(MPI_Info info, char *key, int *valuelen, int *flag) {
   int r = 0;  
   IMPIE_CALL4(int, r, IMPIE_INFO_GET_VALUELEN, "MPI_Info_get_valuelen", info, key, valuelen, flag);
   return r;
}

int MPI_Info_set(MPI_Info info, char *key, char *value) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_INFO_SET, "MPI_Info_set", info, key, value);
   return r;
}

int MPI_Init(int *pargc, char ***pargv) {
    int r = 0; 
    int rval;
    FILE* solution_file;
    FILE* fd_temp;
    char choice[30];
    int i;
    int addr, number;

    initmark_();
       
   read_config();
   IMPIE_CALL2(int, r, IMPIE_INIT, "MPI_Init", pargc, pargv);

    fd_temp=fopen("config.dat","r");
    if(fd_temp<=0)
    {
        fprintf(stderr, "error in opening config.dat, needs to be in current directory\n");
        exit(-1);
    }

//0-log 1-shift
  fscanf(fd_temp,"%s\n",choice);

  if(strcmp(choice, "TRACE") == 0)
        TRACE=1;
  else if(strcmp(choice, "SHIFT") == 0)
        SHIFT=1;
  else if(strcmp(choice, "TRACE_SHIFT")==0 || strcmp(choice, "SHIFT_TRACE") == 0)
    TRACE=SHIFT=1;
  else if(strcmp(choice, "ORGANIC") == 0)
      TRACE = SHIFT = 0;
  else
  {
      fprintf(stderr, "incorrect config file\n");
      exit(1);
  }

    solution_file = fopen("solution.txt", "r");
    if(solution_file != NULL)
    {
    num_shiftingpoints = 0;
    num_trace_points = 0;
    while (fscanf(solution_file, "%lu %d", &addr, &number) != EOF)
    {
        if(number < 0)
        {
            //print list
            tracepoint[num_trace_points] = addr;
            num_trace_points++;
        }
        else
        {
            solutions[num_shiftingpoints].addr = addr;
            solutions[num_shiftingpoints].gear = number;
            num_shiftingpoints++;
        }
        //printf("%lu %d\n", solutions[num_shiftingpoints].addr, solutions[num_shiftingpoints].gear);
    }
    fclose(solution_file);

    }
    if(SHIFT)
        agent_connect(NULL, 0, 0);
    //set_device_pstate(0, 0);
   
   return r;
}

int MPI_Initialized(int *flag) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_INITIALIZED, "MPI_Initialized", flag);
   return r;
}

int MPI_Init_thread(int *pargc, char ***pargv, int requested, int *pprovided) {
   int r = 0;  
   IMPIE_CALL4(int, r, IMPIE_INIT_THREAD, "MPI_Init_thread", pargc, pargv, requested, pprovided);
   return r;
}

int MPI_Intercomm_create(MPI_Comm lcomm, int lleader, MPI_Comm pcomm,
      int pleader, int tag, MPI_Comm *newcomm) {
   int r = 0;  
   IMPIE_CALL6(int, r, IMPIE_INTERCOMM_CREATE, "MPI_Intercomm_create", lcomm, lleader, pcomm, pleader, tag, newcomm);
   return r;
}

int MPI_Intercomm_merge(MPI_Comm comm, int high, MPI_Comm *newcomm) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_INTERCOMM_MERGE, "MPI_Intercomm_merge", comm, high, newcomm);
   return r;
}

int MPI_Iprobe(int src, int tag, MPI_Comm comm, int *flag, MPI_Status *stat) {
   int r = 0;  
   IMPIE_CALL5(int, r, IMPIE_IPROBE, "MPI_Iprobe", src, tag, comm, flag, stat);
   return r;
}

int MPI_Irecv(void *buf, int count, MPI_Datatype dtype, int src, int tag,
      MPI_Comm comm, MPI_Request *req) {
   int r = 0;  
   IMPIE_CALL7(int, r, IMPIE_IRECV, "MPI_Irecv", buf, count, dtype, src, tag, comm, req);
   return r;
}

int MPI_Irsend(void *buf, int count, MPI_Datatype dtype, int dest, int tag,
      MPI_Comm comm, MPI_Request *req) {
   int r = 0;  
   IMPIE_CALL7(int, r, IMPIE_IRSEND, "MPI_Irsend", buf, count, dtype, dest, tag, comm, req);
   return r;
}

int MPI_Isend(void *buf, int count, MPI_Datatype dtype, int dest, int tag,
      MPI_Comm comm, MPI_Request *req) {
   int r = 0;  
   IMPIE_CALL7(int, r, IMPIE_ISEND, "MPI_Isend", buf, count, dtype, dest, tag, comm, req);
   return r;
}

int MPI_Issend(void *buf, int count, MPI_Datatype dtype, int dest, int tag,
      MPI_Comm comm, MPI_Request *req) {
   int r = 0;  
   IMPIE_CALL7(int, r, IMPIE_ISSEND, "MPI_Issend", buf, count, dtype, dest, tag, comm, req);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
int MPI_Is_thread_main(int *pflag) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_IS_THREAD_MAIN, "MPI_Is_thread_main", pflag);
   return r;
}
#endif

int MPI_Keyval_create(MPI_Copy_function *cpyfunc, MPI_Delete_function *delfunc,
      int *key, void *extra) {
   int r = 0;  
   IMPIE_CALL4(int, r, IMPIE_KEYVAL_CREATE, "MPI_Keyval_create", cpyfunc, delfunc, key, extra);
   return r;
}

int MPI_Keyval_free(int *key) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_KEYVAL_FREE, "MPI_Keyval_free", key);
   return r;
}

#ifdef _IMPIE_HAVE_MPICH
int MPI_NULL_COPY_FN(MPI_Comm comm, int i1, void *p1, void *p2, void *p3,
      int *i2) {
   int r = 0;
   IMPIE_CALL6(int, r, IMPIE_NULL_COPY_FN, "MPI_NULL_COPY_FN", comm, i1,
      p1, p2, p3, i2);
   return r;
}

int MPI_NULL_DELETE_FN(MPI_Comm comm, int i1, void *p1, void *p2) {
   int r = 0;
   IMPIE_CALL4(int, r, IMPIE_NULL_DELETE_FN, "MPI_NULL_DELETE_FN", comm, i1, p1, p2);
   return r;
}
#endif

#ifdef _IMPIE_HAVE_LAM
int MPI_Lookup_name(char *service_name, MPI_Info info, char *port_name) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_LOOKUP_NAME, "MPI_Lookup_name", service_name, info, port_name);
   return r;
}

MPI_Fint MPI_Op_c2f(MPI_Op op) {
   MPI_Fint r = 0;
   IMPIE_CALL1(int, r, IMPIE_OP_C2F, "MPI_Op_c2f", op);
   return r;
}
#endif

int MPI_Op_create(MPI_User_function *func, int commute, MPI_Op *pop) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_OP_CREATE, "MPI_Op_create", func, commute, pop);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
int MPI_Open_port(MPI_Info info, char *port_name) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_OPEN_PORT, "MPI_Open_port", info, port_name);
   return r;
}

MPI_Op MPI_Op_f2c(MPI_Fint fhndl) {
   MPI_Op r = 0;
   IMPIE_CALL1(MPI_Op, r, IMPIE_OP_F2C, "MPI_Op_f2c", fhndl);
   return r;
}
#endif

int MPI_Op_free(MPI_Op *op) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_OP_FREE, "MPI_Op_free", op);
   return r;
}

int MPI_Pack(void *buf, int count, MPI_Datatype dtype, void *packbuf,
      int packsize, int *packpos, MPI_Comm comm) {
   int r = 0;  
   IMPIE_CALL7(int, r, IMPIE_PACK, "MPI_Pack", buf, count, dtype, packbuf, packsize, packpos, comm);
   return r;
}

int MPI_Pack_size(int count, MPI_Datatype dtype, MPI_Comm comm, int *psize) {
   int r = 0;  
   IMPIE_CALL4(int, r, IMPIE_PACK_SIZE, "MPI_Pack_size", count, dtype, comm, psize);
   return r;
}

int MPI_Pcontrol(int level, ...) {
   int r = 0;  
   va_list ap;

   va_start(ap, level);
   IMPIE_CALL2(int, r, IMPIE_PCONTROL, "MPI_Pcontrol", level, ap);
   va_end(ap);

   return r;
}

int MPI_Probe(int src, int tag, MPI_Comm comm, MPI_Status *stat) {
   int r = 0;  
   IMPIE_CALL4(int, r, IMPIE_PROBE, "MPI_Probe", src, tag, comm, stat);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
int MPI_Publish_name(char *service_name, MPI_Info info, char *port_name) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_PUBLISH_NAME, "MPI_Publish_name", service_name, info, port_name);
   return r;
}

int MPI_Put(void *orgaddr, int orgcnt, MPI_Datatype orgtype, int rank,
      MPI_Aint targdisp, int targcnt, MPI_Datatype targtype, MPI_Win win) {
   int r = 0;  
   IMPIE_CALL8(int, r, IMPIE_PUT, "MPI_Put", orgaddr, orgcnt, orgtype, rank, targdisp, targcnt, targtype, win);
   return r;
}

int MPI_Query_thread(int *pprovided) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_QUERY_THREAD, "MPI_Query_thread", pprovided);
   return r;
}
#endif

int MPI_Recv(void *buf, int count, MPI_Datatype dtype, int src, int tag,
      MPI_Comm comm, MPI_Status *stat) {
   int r = 0;  
   IMPIE_CALL7(int, r, IMPIE_RECV, "MPI_Recv", buf, count, dtype, src, tag, comm, stat);
   return r;
}

int MPI_Recv_init(void *buf, int count, MPI_Datatype dtype, int src, int tag,
      MPI_Comm comm, MPI_Request *req) {
   int r = 0;  
   IMPIE_CALL7(int, r, IMPIE_RECV_INIT, "MPI_Recv_init", buf, count, dtype, src, tag, comm, req);
   return r;
}

int MPI_Reduce(void *sbuf, void *rbuf, int count, MPI_Datatype dtype, MPI_Op op,
      int root, MPI_Comm comm) {
   int r = 0;  
   IMPIE_CALL7(int, r, IMPIE_REDUCE, "MPI_Reduce", sbuf, rbuf, count, dtype, op, root, comm);
   return r;
}

int MPI_Reduce_scatter(void *sbuf, void *rbuf, int *rcounts, MPI_Datatype dtype,
      MPI_Op op, MPI_Comm comm) {
   int r = 0;  
   IMPIE_CALL6(int, r, IMPIE_REDUCE_SCATTER, "MPI_Reduce_scatter", sbuf, rbuf, rcounts, dtype, op, comm);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
MPI_Fint MPI_Request_c2f(MPI_Request req) {
   MPI_Fint r = 0;
   IMPIE_CALL1(int, r, IMPIE_REQUEST_C2F, "MPI_Request_c2f", req);
   return r;
}

MPI_Request MPI_Request_f2c(MPI_Fint fint) {
   MPI_Request r = 0;
   IMPIE_CALL1(MPI_Request, r, IMPIE_REQUEST_F2C, "MPI_Request_f2c", fint);
   return r;
}
#endif

int MPI_Request_free(MPI_Request *preq) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_REQUEST_FREE, "MPI_Request_free", preq);
   return r;
}

int MPI_Rsend(void *buf, int count, MPI_Datatype dtype, int dest, int tag,
      MPI_Comm comm) {
   int r = 0;  
   IMPIE_CALL6(int, r, IMPIE_RSEND, "MPI_Rsend", buf, count, dtype, dest, tag, comm);
   return r;
}

int MPI_Rsend_init(void *buf, int count, MPI_Datatype dtype, int dest, int tag,
      MPI_Comm comm, MPI_Request *req) {
   int r = 0;  
   IMPIE_CALL7(int, r, IMPIE_RSEND_INIT, "MPI_Rsend_init", buf, count, dtype, dest, tag, comm, req);
   return r;
}

int MPI_Scan(void *sbuf, void *rbuf, int count, MPI_Datatype dtype, MPI_Op op,
      MPI_Comm comm) {
   int r = 0;  
   IMPIE_CALL6(int, r, IMPIE_SCAN, "MPI_Scan", sbuf, rbuf, count, dtype, op, comm);
   return r;
}

int MPI_Scatter(void *sbuf, int scount, MPI_Datatype sdtype, void *rbuf,
      int rcount, MPI_Datatype rdtype, int root, MPI_Comm comm) {
   int r = 0;  
   IMPIE_CALL8(int, r, IMPIE_SCATTER, "MPI_Scatter", sbuf, scount, sdtype, rbuf, rcount, rdtype, root, comm);
   return r;
}

int MPI_Scatterv(void *sbuf, int *scounts, int *disps, MPI_Datatype sdtype,
      void *rbuf, int rcount, MPI_Datatype rdtype, int root, MPI_Comm comm) {
   int r = 0;  
   IMPIE_CALL9(int, r, IMPIE_SCATTERV, "MPI_Scatterv", sbuf, scounts, disps, sdtype, rbuf, rcount, rdtype, root, comm);
   return r;
}

int MPI_Send(void *buf, int count, MPI_Datatype dtype, int dest, int tag,
      MPI_Comm comm) {
   int r = 0;  
   IMPIE_CALL6(int, r, IMPIE_SEND, "MPI_Send", buf, count, dtype, dest, tag, comm);
   return r;
}

int MPI_Send_init(void *buf, int count, MPI_Datatype dtype, int dest, int tag,
      MPI_Comm comm, MPI_Request *req) {
   int r = 0;  
   IMPIE_CALL7(int, r, IMPIE_SEND_INIT, "MPI_Send_init", buf, count, dtype, dest, tag, comm, req);
   return r;
}

int MPI_Sendrecv(void *sbuf, int scount, MPI_Datatype sdtype, int dest,
      int stag, void *rbuf, int rcount, MPI_Datatype rdtype, int src,
      int rtag, MPI_Comm comm, MPI_Status *status) {
   int r = 0;  
   IMPIE_CALL12(int, r, IMPIE_SENDRECV, "MPI_Sendrecv", sbuf, scount, sdtype, dest, stag, rbuf, rcount, rdtype, src, rtag, comm, status);
   return r;
}

int MPI_Sendrecv_replace(void *buf, int count, MPI_Datatype dtype, int dest,
      int stag, int src, int rtag, MPI_Comm comm, MPI_Status *status) {
   int r = 0;  
   IMPIE_CALL9(int, r, IMPIE_SENDRECV_REPLACE, "MPI_Sendrecv_replace", buf, count, dtype, dest, stag, src, rtag, comm, status);
   return r;
}

int MPI_Ssend(void *buf, int count, MPI_Datatype dtype, int dest, int tag,
      MPI_Comm comm) {
   int r = 0;  
   IMPIE_CALL6(int, r, IMPIE_SSEND, "MPI_Ssend", buf, count, dtype, dest, tag, comm);
   return r;
}

int MPI_Ssend_init(void *buf, int count, MPI_Datatype dtype, int dest,
      int tag, MPI_Comm comm, MPI_Request *req) {
   int r = 0;  
   IMPIE_CALL7(int, r, IMPIE_SSEND_INIT, "MPI_Ssend_init", buf, count, dtype, dest, tag, comm, req);
   return r;
}

int MPI_Startall(int nreq, MPI_Request *reqs) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_STARTALL, "MPI_Startall", nreq, reqs);
   return r;
}

int MPI_Start(MPI_Request *req) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_START, "MPI_Start", req);
   return r;
}

int MPI_Status_c2f(MPI_Status *c_status, MPI_Fint *f_status) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_STATUS_C2F, "MPI_Status_c2f", c_status, f_status);
   return r;
}

int MPI_Status_f2c(MPI_Fint *f_status, MPI_Status *c_status) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_STATUS_F2C, "MPI_Status_f2c", f_status, c_status);
   return r;
}

#ifdef _IMPIE_HAVE_MPICH
int MPI_Status_set_cancelled(MPI_Status *s, int i) {
   int r = 0;
   IMPIE_CALL2(int, r, IMPIE_STATUS_SET_CANCELLED, "MPI_Status_set_cancelled", s, i);
   return r;
}

int MPI_Status_set_elements(MPI_Status *s, MPI_Datatype dtype, int i) {
   int r = 0;
   IMPIE_CALL3(int, r, IMPIE_STATUS_SET_ELEMENTS, "MPI_Status_set_elements",
      s, dtype, i);
   return r;
}
#endif

int MPI_Testall(int count, MPI_Request *reqs, int *flag, MPI_Status *stats) {
   int r = 0;  
   IMPIE_CALL4(int, r, IMPIE_TESTALL, "MPI_Testall", count, reqs, flag, stats);
   return r;
}

int MPI_Testany(int count, MPI_Request *reqs, int *index, int *flag,
      MPI_Status *stat) {
   int r = 0;  
   IMPIE_CALL5(int, r, IMPIE_TESTANY, "MPI_Testany", count, reqs, index, flag, stat);
   return r;
}

int MPI_Test(MPI_Request *req, int *flag, MPI_Status *stat) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_TEST, "MPI_Test", req, flag, stat);
   return r;
}

int MPI_Test_cancelled(MPI_Status *pstat, int *pflag) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_TEST_CANCELLED, "MPI_Test_cancelled", pstat, pflag);
   return r;
}

int MPI_Testsome(int count, MPI_Request *reqs, int *outcount, int *indices,
      MPI_Status *stats) {
   int r = 0;  
   IMPIE_CALL5(int, r, IMPIE_TESTSOME, "MPI_Testsome", count, reqs, outcount, indices, stats);
   return r;
}

int MPI_Topo_test(MPI_Comm comm, int *ptopo) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_TOPO_TEST, "MPI_Topo_test", comm, ptopo);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
MPI_Fint MPI_Type_c2f(MPI_Datatype dtype) {
   MPI_Fint r = 0;
   IMPIE_CALL1(int, r, IMPIE_TYPE_C2F, "MPI_Type_c2f", dtype);
   return r;
}
#endif

int MPI_Type_commit(MPI_Datatype *dtype) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_TYPE_COMMIT, "MPI_Type_commit", dtype);
   return r;
}

int MPI_Type_contiguous(int count, MPI_Datatype oldtype,
      MPI_Datatype *newtype) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_TYPE_CONTIGUOUS, "MPI_Type_contiguous", count, oldtype, newtype);
   return r;
}

#ifdef _IMPIE_HAVE_MPICH
int MPI_Type_count(MPI_Datatype dtype, int *count) {
   int r = 0;
   IMPIE_CALL2(int, r, IMPIE_TYPE_COUNT, "MPI_Type_count", dtype, count);
   return r;
}
#endif

int MPI_Type_create_darray(int size, int rank, int ndims, int *gsizes,
      int *distribs, int *dargs, int *psizes, int order, MPI_Datatype oldtype,
      MPI_Datatype *newtype) {
   int r = 0;  
   IMPIE_CALL10(int, r, IMPIE_TYPE_CREATE_DARRAY, "MPI_Type_create_darray", size, rank, ndims, gsizes, distribs, dargs, psizes, order, oldtype, newtype);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
int MPI_Type_create_hindexed(int count, int *lengths, MPI_Aint *disps,
      MPI_Datatype oldtype, MPI_Datatype *newtype) {
   int r = 0;  
   IMPIE_CALL5(int, r, IMPIE_TYPE_CREATE_HINDEXED, "MPI_Type_create_hindexed", count, lengths, disps, oldtype, newtype);
   return r;
}

int MPI_Type_create_hvector(int count, int length, MPI_Aint stride,
      MPI_Datatype oldtype, MPI_Datatype *newtype) {
   int r = 0;  
   IMPIE_CALL5(int, r, IMPIE_TYPE_CREATE_HVECTOR, "MPI_Type_create_hvector", count, length, stride, oldtype, newtype);
   return r;
}

int MPI_Type_create_keyval(MPI_Type_copy_attr_function *cpyfunc,
      MPI_Type_delete_attr_function *delfunc, int *key, void *extra) {
   int r = 0;  
   IMPIE_CALL4(int, r, IMPIE_TYPE_CREATE_KEYVAL, "MPI_Type_create_keyval", cpyfunc, delfunc, key, extra);
   return r;
}

int MPI_Type_create_resized(MPI_Datatype dtype, MPI_Aint aint1,
      MPI_Aint aint2, MPI_Datatype *pdtype) {
   int r = 0;  
   IMPIE_CALL4(int, r, IMPIE_TYPE_CREATE_RESIZED, "MPI_Type_create_resized", dtype, aint1, aint2, pdtype);
   return r;
}

int MPI_Type_create_struct(int count, int *lengths, MPI_Aint *disps,
      MPI_Datatype *oldtypes, MPI_Datatype *newtype) {
   int r = 0;  
   IMPIE_CALL5(int, r, IMPIE_TYPE_CREATE_STRUCT, "MPI_Type_create_struct", count, lengths, disps, oldtypes, newtype);
   return r;
}
#endif

#ifdef _IMPIE_HAVE_MPICH
int MPI_Type_create_indexed_block(int i1, int i2, int *i3,
      MPI_Datatype dtype1, MPI_Datatype *dtype2) {
   int r = 0;
   IMPIE_CALL5(int, r, IMPIE_TYPE_CREATE_INDEXED_BLOCK,
      "MPI_Type_create_indexed_block", i1, i2, i3, dtype1, dtype2);
   return r;
}
#endif

int MPI_Type_create_subarray(int ndims, int *sizes, int *subsizes,
      int *starts, int order, MPI_Datatype oldtype, MPI_Datatype *newtype) {
   int r = 0;  
   IMPIE_CALL7(int, r, IMPIE_TYPE_CREATE_SUBARRAY, "MPI_Type_create_subarray", ndims, sizes, subsizes, starts, order, oldtype, newtype);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
int MPI_Type_delete_attr(MPI_Datatype type, int key) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_TYPE_DELETE_ATTR, "MPI_Type_delete_attr", type, key);
   return r;
}

int MPI_Type_dup(MPI_Datatype type, MPI_Datatype *newtype) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_TYPE_DUP, "MPI_Type_dup", type, newtype);
   return r;
}
#endif

int MPI_Type_extent(MPI_Datatype dtype, MPI_Aint *pextent) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_TYPE_EXTENT, "MPI_Type_extent", dtype, pextent);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
MPI_Datatype MPI_Type_f2c(MPI_Fint fint) {
   MPI_Datatype r = 0;
   IMPIE_CALL1(MPI_Datatype, r, IMPIE_TYPE_F2C, "MPI_Type_f2c", fint);
   return r;
}
#endif

int MPI_Type_free(MPI_Datatype *dtype) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_TYPE_FREE, "MPI_Type_free", dtype);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
int MPI_Type_free_keyval(int *key) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_TYPE_FREE_KEYVAL, "MPI_Type_free_keyval", key);
   return r;
}

int MPI_Type_get_attr(MPI_Datatype type, int key, void *value, int *found) {
   int r = 0;  
   IMPIE_CALL4(int, r, IMPIE_TYPE_GET_ATTR, "MPI_Type_get_attr", type, key, value, found);
   return r;
}
#endif

int MPI_Type_get_contents(MPI_Datatype type, int nints, int naddrs,
      int ndtypes, int *ints, MPI_Aint *addrs, MPI_Datatype *dtypes) {
   int r = 0;  
   IMPIE_CALL7(int, r, IMPIE_TYPE_GET_CONTENTS, "MPI_Type_get_contents", type, nints, naddrs, ndtypes, ints, addrs, dtypes);
   return r;
}

int MPI_Type_get_envelope(MPI_Datatype dtype, int *nints, int *naddrs,
      int *ndtypes, int *combiner) {
   int r = 0;  
   IMPIE_CALL5(int, r, IMPIE_TYPE_GET_ENVELOPE, "MPI_Type_get_envelope", dtype, nints, naddrs, ndtypes, combiner);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
int MPI_Type_get_extent(MPI_Datatype dtype, MPI_Aint *lb, MPI_Aint *extent) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_TYPE_GET_EXTENT, "MPI_Type_get_extent", dtype, lb, extent);
   return r;
}

int MPI_Type_get_name(MPI_Datatype type, char *name, int *length) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_TYPE_GET_NAME, "MPI_Type_get_name", type, name, length);
   return r;
}

int MPI_Type_get_true_extent(MPI_Datatype dtype, MPI_Aint *lb,
      MPI_Aint *extent) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_TYPE_GET_TRUE_EXTENT, "MPI_Type_get_true_extent", dtype, lb, extent);
   return r;
}
#endif

int MPI_Type_hindexed(int count, int *lengths, MPI_Aint *disps,
      MPI_Datatype oldtype, MPI_Datatype *newtype) {
   int r = 0;  
   IMPIE_CALL5(int, r, IMPIE_TYPE_HINDEXED, "MPI_Type_hindexed", count, lengths, disps, oldtype, newtype);
   return r;
}

int MPI_Type_hvector(int count, int length, MPI_Aint stride,
      MPI_Datatype oldtype, MPI_Datatype *newtype) {
   int r = 0;  
   IMPIE_CALL5(int, r, IMPIE_TYPE_HVECTOR, "MPI_Type_hvector", count, length, stride, oldtype, newtype);
   return r;
}

int MPI_Type_indexed(int count, int *lengths, int *disps,
      MPI_Datatype oldtype, MPI_Datatype *newtype) {
   int r = 0;  
   IMPIE_CALL5(int, r, IMPIE_TYPE_INDEXED, "MPI_Type_indexed", count, lengths, disps, oldtype, newtype);
   return r;
}

int MPI_Type_lb(MPI_Datatype dtype, MPI_Aint *lb) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_TYPE_LB, "MPI_Type_lb", dtype, lb);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
int MPI_Type_set_attr(MPI_Datatype type, int key, void *value) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_TYPE_SET_ATTR, "MPI_Type_set_attr", type, key, value);
   return r;
}

int MPI_Type_set_name(MPI_Datatype type, char *name) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_TYPE_SET_NAME, "MPI_Type_set_name", type, name);
   return r;
}
#endif

int MPI_Type_size(MPI_Datatype dtype, int *psize) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_TYPE_SIZE, "MPI_Type_size", dtype, psize);
   return r;
}

int MPI_Type_struct(int count, int *lengths, MPI_Aint *disps,
      MPI_Datatype *oldtypes, MPI_Datatype *newtype) {
   int r = 0;  
   IMPIE_CALL5(int, r, IMPIE_TYPE_STRUCT, "MPI_Type_struct", count, lengths, disps, oldtypes, newtype);
   return r;
}

int MPI_Type_ub(MPI_Datatype dtype, MPI_Aint *ub) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_TYPE_UB, "MPI_Type_ub", dtype, ub);
   return r;
}

int MPI_Type_vector(int count, int length, int stride, MPI_Datatype oldtype,
      MPI_Datatype *newtype) {
   int r = 0;  
   IMPIE_CALL5(int, r, IMPIE_TYPE_VECTOR, "MPI_Type_vector", count, length, stride, oldtype, newtype);
   return r;
}

int MPI_Unpack(void *packbuf, int packsize, int *packpos, void *buf,
      int count, MPI_Datatype dtype, MPI_Comm comm) {
   int r = 0;  
   IMPIE_CALL7(int, r, IMPIE_UNPACK, "MPI_Unpack", packbuf, packsize, packpos, buf, count, dtype, comm);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
int MPI_Unpublish_name(char *s1, MPI_Info info, char *s2) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_UNPUBLISH_NAME, "MPI_Unpublish_name", s1, info, s2);
   return r;
}
#endif

int MPI_Waitall(int count, MPI_Request *reqs, MPI_Status *stats) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_WAITALL, "MPI_Waitall", count, reqs, stats);
   return r;
}

int MPI_Waitany(int i1, MPI_Request *reqs, int *i2, MPI_Status *stats) {
   int r = 0;
   IMPIE_CALL4(int, r, IMPIE_WAITANY, "MPI_Waitany", i1, reqs, i2, stats);
   return r;
}

int MPI_Waitsome(int count, MPI_Request *reqs, int *ndone, int *indices,
      MPI_Status *stats) {
   int r = 0;  
   IMPIE_CALL5(int, r, IMPIE_WAITSOME, "MPI_Waitsome", count, reqs, ndone, indices, stats);
   return r;
}

int MPI_Wait(MPI_Request *req, MPI_Status *stats) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_WAIT, "MPI_Wait", req, stats);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
MPI_Fint MPI_Win_c2f(MPI_Win win) {
   MPI_Fint r = 0;
   IMPIE_CALL1(int, r, IMPIE_WIN_C2F, "MPI_Win_c2f", win);
   return r;
}

int MPI_Win_complete(MPI_Win win) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_WIN_COMPLETE, "MPI_Win_complete", win);
   return r;
}

int MPI_Win_create(void *base, MPI_Aint size, int disp_unit, MPI_Info info,
      MPI_Comm comm, MPI_Win *newwin) {
   int r = 0;  
   IMPIE_CALL6(int, r, IMPIE_WIN_CREATE, "MPI_Win_create", base, size, disp_unit, info, comm, newwin);
   return r;
}

int MPI_Win_create_errhandler(MPI_Win_errhandler_fn *errfunc,
      MPI_Errhandler *errhdl) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_WIN_CREATE_ERRHANDLER, "MPI_Win_create_errhandler", errfunc, errhdl);
   return r;
}

int MPI_Win_create_keyval(MPI_Win_copy_attr_function *cpyfunc,
      MPI_Win_delete_attr_function *delfunc, int *key, void *extra) {
   int r = 0;  
   IMPIE_CALL4(int, r, IMPIE_WIN_CREATE_KEYVAL, "MPI_Win_create_keyval", cpyfunc, delfunc, key, extra)
   return r;
}

int MPI_Win_delete_attr(MPI_Win win, int key) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_WIN_DELETE_ATTR, "MPI_Win_delete_attr", win, key);
   return r;
}

MPI_Win MPI_Win_f2c(MPI_Fint fint) {
   MPI_Win r = 0;
   IMPIE_CALL1(MPI_Win, r, IMPIE_WIN_F2C, "MPI_Win_f2c", fint);
   return r;
}

int MPI_Win_fence(int assertion, MPI_Win win) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_WIN_FENCE, "MPI_Win_fence", assertion, win);
   return r;
}

int MPI_Win_free(MPI_Win *win) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_WIN_FREE, "MPI_Win_free", win);
   return r;
}
#endif


int MPI_Win_free_keyval(int *key) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_WIN_FREE_KEYVAL, "MPI_Win_free_keyval", key);
   return r;
}

#ifdef _IMPIE_HAVE_LAM
int MPI_Win_get_attr(MPI_Win win, int key, void *value, int *found) {
   int r = 0;  
   IMPIE_CALL4(int, r, IMPIE_WIN_GET_ATTR, "MPI_Win_get_attr", win, key, value, found);
   return r;
}

int MPI_Win_get_errhandler(MPI_Win win, MPI_Errhandler *errhdl) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_WIN_GET_ERRHANDLER, "MPI_Win_get_errhandler", win, errhdl);
   return r;
}

int MPI_Win_get_group(MPI_Win win, MPI_Group *group) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_WIN_GET_GROUP, "MPI_Win_get_group", win, group);
   return r;
}

int MPI_Win_get_name(MPI_Win win, char *name, int *length) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_WIN_GET_NAME, "MPI_Win_get_name", win, name, length);
   return r;
}

int MPI_Win_lock(int i1, int i2, int i3, MPI_Win win) {
   int r = 0;
   IMPIE_CALL4(int, r, IMPIE_WIN_LOCK, "MPI_Win_lock", i1, i2, i3, win);
   return r;
}

int MPI_Win_post(MPI_Group group, int assertion, MPI_Win win) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_WIN_POST, "MPI_Win_post", group, assertion, win);
   return r;
}

int MPI_Win_set_attr(MPI_Win win, int key, void *value) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_WIN_SET_ATTR, "MPI_Win_set_attr", win, key, value);
   return r;
}

int MPI_Win_set_errhandler(MPI_Win win, MPI_Errhandler errhdl) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_WIN_SET_ERRHANDLER, "MPI_Win_set_errhandler", win, errhdl);
   return r;
}

int MPI_Win_set_name(MPI_Win win, char *name) {
   int r = 0;  
   IMPIE_CALL2(int, r, IMPIE_WIN_SET_NAME, "MPI_Win_set_name", win, name);
   return r;
}

int MPI_Win_start(MPI_Group group, int assertion, MPI_Win win) {
   int r = 0;  
   IMPIE_CALL3(int, r, IMPIE_WIN_START, "MPI_Win_start", group, assertion, win);
   return r;
}

int MPI_Win_test(MPI_Win win, int *i1) {
   int r = 0;
   IMPIE_CALL2(int, r, IMPIE_WIN_TEST, "MPI_Win_test", win, i1);
   return r;
}

int MPI_Win_unlock(int i1, MPI_Win win) {
   int r = 0;
   IMPIE_CALL2(int, r, IMPIE_WIN_UNLOCK, "MPI_Win_unlock", i1, win);
   return r;
}

int MPI_Win_wait(MPI_Win win) {
   int r = 0;  
   IMPIE_CALL1(int, r, IMPIE_WIN_WAIT, "MPI_Win_wait", win);
   return r;
}
#endif


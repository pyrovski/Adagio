/*
 * These macro routines are used to provide convenience in facilitating
 * changes in functionality.  There is currently no standardized C
 * preprocessing facility for passing variable arguments for a macro
 * so multiple macros are created depending on the number of arguments.
 *
 * $Id: macros.h,v 1.3 2004/04/21 19:36:01 mefemal Exp $
 */
#ifndef __MACRO_H
#define __MACRO_H

#include "platform.h"

#define IMPIE_HEAD(i, n) \
   struct timeval tm; \
   long address = 0; \
   CALLER_ADDRESS(address); \
   idebug = i; \
   if (fctable[i].mpi_fptr == NULL) { \
      fctable[i].mpi_fptr = (void *(*)())dlsym(RTLD_NEXT, n); \
      if (fctable[i].mpi_fptr == NULL) { \
         fprintf(stderr, "dlsym: %s\n", dlerror()); \
         return MPI_ERR_OTHER; \
      } \
   } \
   if (etrace) \
      trace_msg("<begin %s, caller=0x%x>", n, address); \
   if (tdebug) \
      gettimeofday(&tm, NULL);

#define IMPIE_TAIL(i, n) \
   fctable[i].count++; \
   if (etrace) \
      trace_msg("<end   %s, count=%u, caller=0x%x>", n, fctable[i].count, \
         address); \
   if (tdebug) \
      add_time(&fctable[i].tm_post, &tm);

#define IMPIE_CALL0(type, r, i, n) \
   IMPIE_HEAD(i, n) \
   if (fctable[i].pre_fptr) { \
      if ((int)((*fctable[i].pre_fptr)(&data)) == 0) { \
         if (tdebug) \
            add_time(&fctable[i].tm_pre, &tm); \
         r = (type)((*fctable[i].mpi_fptr)()); \
         if (tdebug) \
            add_time(&fctable[i].tm_mpi, &tm); \
       } \
   } else { \
      r = (type)((*fctable[i].mpi_fptr)()); \
      if (tdebug) \
         add_time(&fctable[i].tm_mpi, &tm); \
   } \
   if (fctable[i].post_fptr) \
      (*fctable[i].post_fptr)(&data); \
   IMPIE_TAIL(i, n)

#define IMPIE_CALL1(type, r, i, n, a1) \
   IMPIE_HEAD(i, n) \
   if (fctable[i].pre_fptr) { \
      if ((int)((*fctable[i].pre_fptr)(a1, &data)) == 0) { \
         if (tdebug) \
            add_time(&fctable[i].tm_pre, &tm); \
         r = (type)(*fctable[i].mpi_fptr)(a1); \
         if (tdebug) \
            add_time(&fctable[i].tm_mpi, &tm); \
      } \
   } else { \
      r = (type)(*fctable[i].mpi_fptr)(a1); \
      if (tdebug) \
         add_time(&fctable[i].tm_mpi, &tm); \
   } \
   if (fctable[i].post_fptr) \
      (*fctable[i].post_fptr)(a1, &data); \
   IMPIE_TAIL(i, n)

#define IMPIE_CALL2(type, r, i, n, a1, a2) \
   IMPIE_HEAD(i, n) \
   if (fctable[i].pre_fptr) { \
      if ((int)((*fctable[i].pre_fptr)(a1, a2, &data)) == 0) { \
         if (tdebug) \
            add_time(&fctable[i].tm_pre, &tm); \
         r = (type)(*fctable[i].mpi_fptr)(a1, a2); \
         if (tdebug) \
            add_time(&fctable[i].tm_mpi, &tm); \
      } \
   } else { \
      r = (type)(*fctable[i].mpi_fptr)(a1, a2); \
      if (tdebug) \
         add_time(&fctable[i].tm_mpi, &tm); \
   } \
   if (fctable[i].post_fptr) \
      (*fctable[i].post_fptr)(a1, a2, &data); \
   IMPIE_TAIL(i, n)

#define IMPIE_CALL3(type, r, i, n, a1, a2, a3) \
   IMPIE_HEAD(i, n) \
   if (fctable[i].pre_fptr) { \
      if ((int)((*fctable[i].pre_fptr)(a1, a2, a3, &data)) == 0) { \
         if (tdebug) \
            add_time(&fctable[i].tm_pre, &tm); \
         r = (type)(*fctable[i].mpi_fptr)(a1, a2, a3); \
         if (tdebug) \
            add_time(&fctable[i].tm_mpi, &tm); \
      } \
   } else { \
      r = (type)(*fctable[i].mpi_fptr)(a1, a2, a3); \
      if (tdebug) \
         add_time(&fctable[i].tm_mpi, &tm); \
   } \
   if (fctable[i].post_fptr) \
      (*fctable[i].post_fptr)(a1, a2, a3, &data); \
   IMPIE_TAIL(i, n)

#define IMPIE_CALL4(type, r, i, n, a1, a2, a3, a4) \
   IMPIE_HEAD(i, n) \
   if (fctable[i].pre_fptr) { \
      if ((int)((*fctable[i].pre_fptr)(a1, a2, a3, a4, &data)) == 0) { \
         if (tdebug) \
            add_time(&fctable[i].tm_pre, &tm); \
         r = (type)(*fctable[i].mpi_fptr)(a1, a2, a3, a4); \
         if (tdebug) \
            add_time(&fctable[i].tm_mpi, &tm); \
      } \
   } else { \
      r = (type)(*fctable[i].mpi_fptr)(a1, a2, a3, a4); \
      if (tdebug) \
         add_time(&fctable[i].tm_mpi, &tm); \
   } \
   if (fctable[i].post_fptr) \
      (*fctable[i].post_fptr)(a1, a2, a3, a4, &data); \
   IMPIE_TAIL(i, n)

#define IMPIE_CALL5(type, r, i, n, a1, a2, a3, a4, a5) \
   IMPIE_HEAD(i, n) \
   if (fctable[i].pre_fptr) { \
      if ((int)((*fctable[i].pre_fptr)(a1, a2, a3, a4, a5, &data)) == 0) { \
         if (tdebug) \
            add_time(&fctable[i].tm_pre, &tm); \
         r = (type)(*fctable[i].mpi_fptr)(a1, a2, a3, a4, a5); \
         if (tdebug) \
            add_time(&fctable[i].tm_mpi, &tm); \
      } \
   } else { \
      r = (type)(*fctable[i].mpi_fptr)(a1, a2, a3, a4, a5); \
      if (tdebug) \
         add_time(&fctable[i].tm_mpi, &tm); \
   } \
   if (fctable[i].post_fptr) \
      (*fctable[i].post_fptr)(a1, a2, a3, a4, a5, &data); \
   IMPIE_TAIL(i, n)

#define IMPIE_CALL6(type, r, i, n, a1, a2, a3, a4, a5, a6) \
   IMPIE_HEAD(i, n) \
   if (fctable[i].pre_fptr) { \
      if ((int)((*fctable[i].pre_fptr)(a1, a2, a3, a4, a5, a6, &data)) == 0) { \
         if (tdebug) \
            add_time(&fctable[i].tm_pre, &tm); \
         r = (type)(*fctable[i].mpi_fptr)(a1, a2, a3, a4, a5, a6); \
         if (tdebug) \
            add_time(&fctable[i].tm_mpi, &tm); \
      } \
   } else { \
      r = (type)(*fctable[i].mpi_fptr)(a1, a2, a3, a4, a5, a6); \
      if (tdebug) \
         add_time(&fctable[i].tm_mpi, &tm); \
   } \
   if (fctable[i].post_fptr) \
      (*fctable[i].post_fptr)(a1, a2, a3, a4, a5, a6, &data); \
   IMPIE_TAIL(i, n)

#define IMPIE_CALL7(type, r, i, n, a1, a2, a3, a4, a5, a6, a7) \
   IMPIE_HEAD(i, n) \
   if (fctable[i].pre_fptr) { \
      if ((int)((*fctable[i].pre_fptr)(a1, a2, a3, a4, a5, a6, a7, &data)) == 0) { \
         if (tdebug) \
            add_time(&fctable[i].tm_pre, &tm); \
         r = (type)(*fctable[i].mpi_fptr)(a1, a2, a3, a4, a5, a6, a7); \
         if (tdebug) \
            add_time(&fctable[i].tm_mpi, &tm); \
      } \
   } else { \
      r = (type)(*fctable[i].mpi_fptr)(a1, a2, a3, a4, a5, a6, a7); \
      if (tdebug) \
         add_time(&fctable[i].tm_mpi, &tm); \
   } \
   if (fctable[i].post_fptr) \
      (*fctable[i].post_fptr)(a1, a2, a3, a4, a5, a6, a7, &data); \
   IMPIE_TAIL(i, n)

#define IMPIE_CALL8(type, r, i, n, a1, a2, a3, a4, a5, a6, a7, a8) \
   IMPIE_HEAD(i, n) \
   if (fctable[i].pre_fptr) { \
      if ((int)((*fctable[i].pre_fptr)(a1, a2, a3, a4, a5, a6, a7, a8, &data)) == 0) { \
         if (tdebug) \
            add_time(&fctable[i].tm_pre, &tm); \
         r = (type)(*fctable[i].mpi_fptr)(a1, a2, a3, a4, a5, a6, a7, a8); \
         if (tdebug) \
            add_time(&fctable[i].tm_mpi, &tm); \
      } \
   } else { \
      r = (type)(*fctable[i].mpi_fptr)(a1, a2, a3, a4, a5, a6, a7, a8); \
      if (tdebug) \
         add_time(&fctable[i].tm_mpi, &tm); \
   } \
   if (fctable[i].post_fptr) \
      (*fctable[i].post_fptr)(a1, a2, a3, a4, a5, a6, a7, a8, &data); \
   IMPIE_TAIL(i, n)

#define IMPIE_CALL9(type, r, i, n, a1, a2, a3, a4, a5, a6, a7, a8, a9) \
   IMPIE_HEAD(i, n) \
   if (fctable[i].pre_fptr) { \
      if ((int)((*fctable[i].pre_fptr)(a1, a2, a3, a4, a5, a6, a7, a8, a9, &data)) == 0) { \
         if (tdebug) \
            add_time(&fctable[i].tm_pre, &tm); \
         r = (type)(*fctable[i].mpi_fptr)(a1, a2, a3, a4, a5, a6, a7, a8, a9); \
         if (tdebug) \
            add_time(&fctable[i].tm_mpi, &tm); \
      } \
   } else { \
      r = (type)(*fctable[i].mpi_fptr)(a1, a2, a3, a4, a5, a6, a7, a8, a9); \
      if (tdebug) \
         add_time(&fctable[i].tm_mpi, &tm); \
   } \
   if (fctable[i].post_fptr) \
      (*fctable[i].post_fptr)(a1, a2, a3, a4, a5, a6, a7, a8, a9, &data); \
   IMPIE_TAIL(i, n)

#define IMPIE_CALL10(type, r, i, n, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) \
   IMPIE_HEAD(i, n) \
   if (fctable[i].pre_fptr) { \
      if ((int)((*fctable[i].pre_fptr)(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, &data)) == 0) { \
         if (tdebug) \
            add_time(&fctable[i].tm_pre, &tm); \
         r = (type)(*fctable[i].mpi_fptr)(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10); \
         if (tdebug) \
            add_time(&fctable[i].tm_mpi, &tm); \
      } \
   } else { \
      r = (type)(*fctable[i].mpi_fptr)(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10); \
      if (tdebug) \
         add_time(&fctable[i].tm_mpi, &tm); \
   } \
   if (fctable[i].post_fptr) \
      (*fctable[i].post_fptr)(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, &data); \
   IMPIE_TAIL(i, n)

#define IMPIE_CALL12(type, r, i, n, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12) \
   IMPIE_HEAD(i, n) \
   if (fctable[i].pre_fptr) { \
      if ((int)((*fctable[i].pre_fptr)(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, &data)) == 0) { \
         if (tdebug) \
            add_time(&fctable[i].tm_pre, &tm); \
         r = (type)(*fctable[i].mpi_fptr)(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12); \
         if (tdebug) \
            add_time(&fctable[i].tm_mpi, &tm); \
      } \
   } else { \
      r = (type)(*fctable[i].mpi_fptr)(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12); \
      if (tdebug) \
         add_time(&fctable[i].tm_mpi, &tm); \
   } \
   if (fctable[i].post_fptr) \
      (*fctable[i].post_fptr)(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, &data); \
   IMPIE_TAIL(i, n)

#endif


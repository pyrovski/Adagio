/******************************************************************************
 *
 *  File Name........: util.c
 *
 *  Description......:
 *
 *  Created by vin on 07/20/04
 *
 *  Revision History.:
 *
 *  $Log: util.c,v $
 *  Revision 1.1  2004/09/09 16:04:26  vin
 *  *** empty log message ***
 *
 *  Revision 1.2  2004/07/21 15:26:03  vin
 *  *** empty log message ***
 *
 *  Revision 1.1  2004/07/20 21:30:58  vin
 *  *** empty log message ***
 *
 *
 *  $Id: util.c,v 1.1 2004/09/09 16:04:26 vin Exp $
 *
 *****************************************************************************/

#include <stdlib.h>
#include <sys/time.h>
#include <stdio.h>
/*-----------------------------------------------------------------------------
 *
 * Name...........: now
 *
 * Description....: user-friendly interface to gettimeofday.  requires
 * that the variable "base" be initialized (by calling now())
 *
 * Input Param(s).: 
 *
 * Return Value(s): time in millisecs
 *
 */

long now()
{
  static long base=0;
  struct timeval tval;
  long usec;

  gettimeofday(&tval, 0);

  usec = tval.tv_usec + tval.tv_sec*1000000;
  if (base == 0) base = usec/1000;
  return usec/1000 - base;
} /*---------- End of now ---------------------------------------------------*/

/*-----------------------------------------------------------------------------
 *
 * Name...........: ckalloc
 *
 * Description....: 
 *
 * Input Param(s).: 
 *		unsigned int l -- 
 *
 * Return Value(s): 
 *
 */

void *ckalloc(unsigned int l)
{
  void *p = malloc(l);
 
  if ( p == NULL ) {
    printf("ckalloc: malloc failed");
    exit(-1);
  }
 
  return p;
} /*---------- End of ckalloc -----------------------------------------------*/
 

// macro for reading hw time stamp counter (cycle count)
#define rdtsc()		 				\
    ( {							\
	    register unsigned long long __res;	       	\
		__asm__ __volatile__(			\
			"rdtsc" : "=A"(__res)		\
		);					\
	    __res;					\
    } )

/*-----------------------------------------------------------------------------
 *
 * Name...........: readTsc
 *
 * Description....: Friendly front-end to rdtsc instr and macro.
 * Requires that tsc be initialized (by calling readTsc
 *
 * Input Param(s).: 
 *
 * Return Value(s): 
 *
 */

unsigned long long readTsc()

{
  static unsigned long long tsc;
  unsigned long long r, t;
  r = rdtsc();
  t = r - tsc;
  tsc = r;
  return t;
} /*---------- End of readTsc -----------------------------------------------*/

/*........................ end of util.c ....................................*/

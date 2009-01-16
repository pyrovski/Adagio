#
# Project makefile, the default 'make' will build everything necessary
#
# $Id: Makefile,v 1.4 2004/04/21 19:36:01 mefemal Exp $

CC=mpicc
MPI_CC=mpicc

# If your LAM install is non-standard, modify the following
#MPI_INC=-I/opt/lam-7.0.4/include -I/osr/pac/include
#MPI_LIB=-L/opt/lam-7.0.4/lib -L/osr/pac/lib -lagent
#MPI_INC=-I/osr/users/mefemal/mpich-1.2.5/include
#MPI_LIB=-L/osr/users/mefemal/mpich-1.2.5/lib

LIB=-L/osr/users/rountree/GreenMPI/local/lib -L/osr/pac/lib64

AGENT_LIB=-lsscaling -lnuma 
AGENT_INC=-I/osr/pac/include

EFLAGS=-Wall -g
#EFLAGS=-O2
CFLAGS=$(EFLAGS) -fPIC

ANDANTE_OBJS=gettimeofday_helpers.o meters.o cpuid.o affinity.o

# =========================================================================

all: libmpijack.so

libmpijack.so: impie.c impie.h
	$(MPI_CC) $(CFLAGS) $(AGENT_INC) $(ANDANTE_OBJS) -o libmpijack.so -shared impie.c $(LIB) -ldl $(AGENT_LIB) --verbose
	cp libmpijack.so $(HOME)/GreenMPI/local/lib
	
dist:
	chmod 777 libmpijack.so
	if [ `arch` = "x86_64" ] ; then cp libmpijack.so /osr/pac/lib64 ; else cp libmpijack.so /osr/pac/lib ; fi

again: clean all

hello: hello.c libmpijack.so
	$(MPI_CC) -DBLR_USE_JITTER $(EFLAGS) $(LIB) -lmpijack $(CFLAGS) $(MPI_INC) $(MPI_LIB) -o test/hello.jitter  hello.c
	$(MPI_CC)                  $(EFLAGS) $(LIB) -lmpijack $(CFLAGS) $(MPI_INC) $(MPI_LIB) -o test/hello.nosched hello.c

clean:
	rm -f *.o *.a *.so test/hello
	ln -s ~/GreenMPI/src/shim/gettimeofday_helpers.o
	ln -s ~/GreenMPI/src/shim/meters.o
	ln -s ~/GreenMPI/src/shim/cpuid.o
	ln -s ~/GreenMPI/src/shim/affinity.o



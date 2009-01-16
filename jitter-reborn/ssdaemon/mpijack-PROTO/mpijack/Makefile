#
# Project makefile, the default 'make' will build everything necessary
#
# $Id: Makefile,v 1.4 2004/04/21 19:36:01 

CC=gcc
MPI_CC=mpicc

EFLAGS=-O2
MPI_LIB=-L/osr/pac/lib 
MPI_INC=-I/osr/pac/include

CFLAGS=$(EFLAGS) $(DEBUG) -D_X86

# =========================================================================

all:	impie.c impie.h  mark.c mark.h util.c util.h info.c info.h pmc.c pmc.h gear.c gear.h
	$(CC) $(CFLAGS) $(MPI_INC) -fPIC -c mark.c
	$(CC) $(CFLAGS) $(MPI_INC) -fPIC -c util.c
	$(CC) $(CFLAGS) $(MPI_INC) -fPIC -c info.c
	$(CC) $(CFLAGS) $(MPI_INC)  -fPIC -c impie.c
	$(CC) $(CFLAGS) $(MPI_INC)  -fPIC -c pmc.c
	$(CC) $(CFLAGS) $(MPI_INC)  -fPIC -c gear.c
	$(CC) $(CFLAGS) -ldl -shared -o libmpijack.so impie.o  mark.o util.o info.o pmc.o gear.o -L/osr/pac/lib -lagent
	#ar ruv libimpie.a impie.o  mark.o util.o info.o pmc.o gear.o #/osr/pac/lib/libagent.a
	chmod 777 libmpijack.so
	cp libmpijack*.so /osr/pac/lib

test:
	$(MPI_CC) $(CFLAGS) $(MPI_INC) $(MPI_LIB) hello.c -o hello -lpthread

clean:
	rm -f *.o *.a *.so hello


# WARNING:  $(HOSTFILE) and ${MPI_INCLUDE_PATH} must be defined elsewhere.
# $(installDest) can optionally be set in the file installPath
#
# greenMPI mca parameters, remember that these can be comma-seperataed with no spaces,
#   e.g. -mca gmpi_algo fermata,jitter -mca gmpi_trace ts,file,line,fn 
# gmpi_algo:
# 	none fermata andante adagio allegro fixedfreq jitter miser clean fakejoules fakefreq
# gmpi_trace:
# 	none all ts file line fn comp comm rank pcontrol

-include installPath
installDest ?= $(HOME)/local

#GMPI_FLAGS=-mca gmpi_algo fermata -mca gmpi_trace all
GMPI_FLAGS= -mca gmpi_trace all
#MCA_REQUIRED_FLAGS=-mca btl self,tcp -mca mpi_paffinity_alone 1 -mca mpi_maffinity libnuma
MCA_REQUIRED_FLAGS=-mca btl self,tcp 
#NUMA_FLAGS=numactl --localalloc --physcpubind=1,2 --
NUMA_FLAGS=


export ADAGIO_FLAGS= -mca gmpi_algo adagio 
export ANDANTE_FLAGS= -mca gmpi_algo andante 
export FERMATA_FLAGS= -mca gmpi_algo fermata 
export NOSCHED_FLAGS= 
export MISER_FLAGS= -mca gmpi_algo miser
export BADNODE_FLAGS= -mca gmpi_badnode opt09,opt13
export NAS_BADNODE_FLAGS= -mca gmpi_badnode opt01,opt09,opt13
export NAS_EXTRA_FLAGS= -mca gmpi_mods bigcomm

# Compile environment

MPICC=mpicc
wrap=../wrap/wrap.py
ifneq ($(dbg),)
DBG=-D_DEBUG=$(dbg) -DBLR_USE_EAGER_LOGGING -g -pg
# -DVERBOSE
OPT_FLAGS = -O0
else
OPT_FLAGS = -O3
endif
CFLAGS=-Wall $(DBG) $(OPT_FLAGS)
LIBDIR=-L. -L$(HOME)/local/lib
INCDIR=-I$(HOME)/local/include
LIBS=-lc -lm -lnuma -lrt -Xlinker -rpath $(HOME)/local/lib -lunwind -lmd5 -lpapi -lmsr
GENERATED_SHIMFILES = shim_enumeration.h shim_functions.c

all: Makefile harness_pristine harness harness_static cpuidTest
	@echo Done
ft:
	$(MAKE) -C ../NPB3.3/NPB3.3-MPI/bin ft  "MPIRUN=$(NAS_MPIRUN)"
nas:
	$(MAKE) -C ../NPB3.3/NPB3.3-MPI/bin nas "MPIRUN=$(NAS_MPIRUN)"
miser:
	$(MAKE) -C ../NPB3.3/NPB3.3-MPI/bin miser_test "MPIRUN=$(NAS_MPIRUN)" 
umt:
	cd ../umt2k-1.2.2/bin; $(MAKE) umt "MPIRUN=$(MPIRUN)" 
umt_jitter:
	cd ../umt2k-1.2.2/bin; $(MAKE) jitter "MPIRUN=$(MPIRUN)" 
paradis:
	cd ../ParaDiS/blr; $(MAKE) paradis "MPIRUN=$(MPIRUN)"
paradis_jitter:
	cd ../ParaDiS/blr; $(MAKE) jitter "MPIRUN=$(MPIRUN)" 

# Test runs
spin: Makefile harness 
# Andante
	echo -n "Andante " >> spin.results
	$(MPIRUN) $(ANDANTE_FLAGS) $(NUMA_FLAGS) ./harness --test_spin >> spin.results 2>&1 
	cat runtime* > spin.andante
	rm -f runtime*
# Fermata
	echo -n "Fermata " >> spin.results
	$(MPIRUN) $(FERMATA_FLAGS) $(NUMA_FLAGS) ./harness --test_spin >> spin.results 2>&1
	cat runtime* > spin.fermata
	rm -f runtime*
# Adagio
	echo -n "Adagio  " >> spin.results
	$(MPIRUN) $(ADAGIO_FLAGS) $(NUMA_FLAGS) ./harness --test_spin >> spin.results 2>&1
	cat runtime* > spin.adagio
	rm -f runtime*
# Nosched
	echo -n "Nosched " >> spin.results
	$(MPIRUN) $(NOSCHED_FLAGS) $(NUMA_FLAGS) ./harness --test_spin >> spin.results 2>&1
	cat runtime* > spin.nosched
	rm -f runtime*

ping: Makefile harness
	$(MPIRUN) ./harness -v -h --test_ping 
	cat runtime* > ping.normal
	rm -f runtime*
	$(MPIRUN) -np 2 ./harness -v -h --test_ping 
	cat runtime* > ping.eager
	rm -f runtime*

hash: Makefile harness 
	$(MPIRUN) ./harness -v -h --test_hash
	cat runtime* > hash.runtime
	rm -f runtime*

sane: Makefile harness 
	$(MPIRUN) ./harness -v -h
	cat runtime* > sane.runtime
	rm -f runtime*

rank: Makefile harness 
	$(MPIRUN) ./harness -v -h --test_rank
	cat runtime* > rank.runtime
	rm -f runtime*

test: Makefile rank hash sane ping spin

objects = shim.o shim_functions.o wpapi.o shift.o meters.o log.o \
stacktrace.o gettimeofday_helpers.o cpuid.o shm.o

# Harness code.

cpuidTest: Makefile cpuidTest.c cpuid.o
	gcc -o $@ cpuidTest.c cpuid.o $(CFLAGS)

harness: Makefile harness.o libGreenMPI.so
	$(MPICC) $(CFLAGS) $(LIBDIR) $(INCDIR)  -o harness 	\
	harness.o -Xlinker -rpath $(HOME)/local/lib -lGreenMPI

harness_pristine: Makefile harness.o
	$(MPICC) $(CFLAGS) -o harness_pristine harness.o 

harness_static: Makefile harness.o $(objects)
	$(MPICC) $(CFLAGS) -o $@ harness.o $(objects) $(LIBDIR) $(LIBS)

harness.o: Makefile $(GENERATED_SHIMFILES) harness.c
	$(MPICC) $(CFLAGS) -c harness.c

cpuid.o: Makefile cpuid.c cpuid.h
	$(MPICC) $(CFLAGS) -c cpuid.c -fPIC

shm.o: Makefile shm.c shm.h
	$(MPICC) $(CFLAGS) $(INCDIR) -c shm.c -fPIC

clean:
	rm -f harness harness_pristine harness_static *.o $(GENERATED_SHIMFILES) *~ libGreenMPI.so

# Adagio libraries.

libGreenMPI.so: Makefile shim.o wpapi.o shift.o meters.o cpuid.o shm.o\
	log.o stacktrace.o gettimeofday_helpers.o \
	$(GENERATED_SHIMFILES)  
	$(MPICC) $(CFLAGS) $(LIBDIR) -shared -Wl,-soname,libGreenMPI.so \
		-o libGreenMPI.so 					\
		$(objects) $(LIBS)
install: libGreenMPI.so harness
	install -m 0744 libGreenMPI.so $(installDest)/lib/

shim.o: Makefile shim.c shim.h log.o stacktrace.o 			\
		gettimeofday_helpers.o wpapi.o shift.o  		\
		$(GENERATED_SHIMFILES) 
	$(MPICC) $(CFLAGS) -fPIC -c shim.c $(INCDIR) -DdetectCritical
	$(MPICC) $(CFLAGS) -fPIC -c shim_functions.c

log.o: Makefile log.c log.h
	$(MPICC) $(CFLAGS) $(INCDIR) -fPIC -c log.c

stacktrace.o: Makefile stacktrace.c stacktrace.h
	$(MPICC) $(CFLAGS) $(INCDIR) -fPIC -c stacktrace.c -I../../local/include

gettimeofday_helpers.o: Makefile gettimeofday_helpers.c gettimeofday_helpers.h
	$(MPICC) $(CFLAGS) $(INCDIR) -fPIC -c gettimeofday_helpers.c

wpapi.o: Makefile wpapi.c wpapi.h
	$(MPICC) $(CFLAGS) $(INCDIR) -fPIC -c wpapi.c

shift.o: Makefile shift.c shift.h 
	$(MPICC) $(CFLAGS) $(INCDIR) -fPIC -c shift.c

meters.o: Makefile meters.c meters.h gettimeofday_helpers.o 
	$(MPICC) $(CFLAGS) $(INCDIR) -fPIC -c meters.c

shim_functions.c: Makefile shim.w shim_h.w
	rm -f $@
	$(wrap) -o shim_functions.c shim.w
	chmod 440 $@

shim_enumeration.h: Makefile shim_h.w
	rm -f $@
	$(wrap) -o shim_enumeration.h shim_h.w
	chmod 440 $@

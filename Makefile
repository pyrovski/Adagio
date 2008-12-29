# WARNING:  $(HOSTFILE) and ${MPI_INCLUDE_PATH} must be defined elsewhere.
#
# greenMPI mca parameters, remember that these can be comma-seperataed with no spaces,
#   e.g. -mca gmpi_algo fermata,jitter -mca gmpi_trace ts,file,line,fn 
# gmpi_algo:
# 	none fermata andante adagio allegro fixedfreq jitter miser clean fakejoules fakefreq
# gmpi_trace:
# 	none all ts file line fn comp comm rank pcontrol
#GMPI_FLAGS=-mca gmpi_algo fermata -mca gmpi_trace all
GMPI_FLAGS= -mca gmpi_trace all
#MCA_REQUIRED_FLAGS=-mca btl self,tcp -mca mpi_paffinity_alone 1 -mca mpi_maffinity libnuma
MCA_REQUIRED_FLAGS=-mca btl self,tcp 
#NUMA_FLAGS=numactl --localalloc --physcpubind=1,2 --
NUMA_FLAGS=


ADAGIO_FLAGS= -mca gmpi_algo adagio 
ANDANTE_FLAGS= -mca gmpi_algo andante 
FERMATA_FLAGS= -mca gmpi_algo fermata 
NOSCHED_FLAGS= 


# Runtime environment
NP=32
MPIRUN=mpirun -np $(NP) -hostfile $(HOSTFILE) $(GMPI_FLAGS) $(MCA_REQUIRED_FLAGS) 

# Compile environment

MPICC=mpicc
CFLAGS=-O0 -Wall -g
LIBDIR=-L$(HOME)/GreenMPI/local/lib
INCDIR=-I$(HOME)/GreenMPI/local/include
LIBS=-lc -lm -lunwind -lmd5 -lpapi -lnuma
GENERATED_SHIMFILES = shim_enumeration.h shim_functions.c shim_parameters.h 	\
shim_selection.h  shim_str.h  shim_structs.h  shim_union.h			


all: Makefile harness_pristine harness
	echo Done
umt:
	cd $(HOME)/GreenMPI/src/umt2k-1.2.2/bin; $(MAKE) umt "MPIRUN=$(MPIRUN)" "ADAGIO_FLAGS=$(ADAGIO_FLAGS)" "ANDANTE_FLAGS=$(ANDANTE_FLAGS)" "FERMATA_FLAGS=$(FERMATA_FLAGS)" "NOSCHED_FLAGS=$(NOSCHED_FLAGS)"

paradis:
	cd $(HOME)/GreenMPI/src/ParaDiS/blr; $(MAKE) paradis "MPIRUN=$(MPIRUN)" "ADAGIO_FLAGS=$(ADAGIO_FLAGS)" "ANDANTE_FLAGS=$(ANDANTE_FLAGS)" "FERMATA_FLAGS=$(FERMATA_FLAGS)" "NOSCHED_FLAGS=$(NOSCHED_FLAGS)"

# Test runs
spin: Makefile harness 
	# Andante
	echo -n "Andante " >> spin.results
	$(MPIRUN) $(ANDANTE_FLAGS) $(NUMA_FLAGS) ./harness --test_spin >> spin.results 2>&1 
	cat runtime* > spin.andante
	rm -rf runtime*
	# Fermata
	echo -n "Fermata " >> spin.results
	$(MPIRUN) $(FERMATA_FLAGS) $(NUMA_FLAGS) ./harness --test_spin >> spin.results 2>&1
	cat runtime* > spin.fermata
	rm -rf runtime*
	# Adagio
	echo -n "Adagio  " >> spin.results
	$(MPIRUN) $(ADAGIO_FLAGS) $(NUMA_FLAGS) ./harness --test_spin >> spin.results 2>&1
	cat runtime* > spin.adagio
	rm -rf runtime*
	# Nosched
	echo -n "Nosched " >> spin.results
	$(MPIRUN) $(NOSCHED_FLAGS) $(NUMA_FLAGS) ./harness --test_spin >> spin.results 2>&1
	cat runtime* > spin.nosched
	rm -rf runtime*
ping: Makefile harness
	$(MPIRUN) -np $(NP) -hostfile $(HOSTFILE) $(MCA_REQUIRED_FLAGS) $(GMPI_FLAGS) \
		./harness -v -h --test_ping 
	cat runtime* > ping.normal
	rm -rf runtime*
	$(MPIRUN) -np 2 -hostfile $(HOSTFILE) $(MCA_REQUIRED_FLAGS) $(GMPI_FLAGS) \
		./harness -v -h --test_ping 
	cat runtime* > ping.eager
	rm -rf runtime*

hash: Makefile harness 
	$(MPIRUN) -np $(NP) -hostfile $(HOSTFILE) $(MCA_FLAGS) $(GMPI_FLAGS) \
		./harness -v -h --test_hash
	cat runtime* > hash.runtime
	rm -rf runtime*

sane: Makefile harness 
	$(MPIRUN) -np $(NP) -hostfile $(HOSTFILE) $(MCA_FLAGS) $(GMPI_FLAGS) \
		./harness -v -h
	cat runtime* > sane.runtime
	rm -rf runtime*

rank: Makefile harness 
	$(MPIRUN) -np $(NP) -hostfile $(HOSTFILE) $(MCA_FLAGS) $(GMPI_FLAGS) \
		./harness -v -h --test_rank
	cat runtime* > rank.runtime
	rm -rf runtime*

test: Makefile rank

# Harness code.

harness: Makefile harness.o GreenMPI
	$(MPICC) $(CFLAGS) $(LIBDIR) $(INCDIR)  -o harness 	\
	harness.o -lGreenMPI

harness_pristine: Makefile harness.o
	$(MPICC) $(CFLAGS) -o harness_pristine harness.o 

harness.o: Makefile $(GENERATED_SHIMFILES) harness.c
	$(MPICC) $(CFLAGS) -c harness.c

clean:
	rm -f harness *.o $(GENERATED_SHIMFILES)

# Adagio libraries.

GreenMPI: Makefile shim.o util.o wpapi.o shift.o cpuid.o meters.o\
	affinity.o $(GENERATED_SHIMFILES)  
	$(MPICC) $(CFLAGS) $(LIBDIR) -shared -Wl,-soname,libGreenMPI.so \
		-o libGreenMPI.so 					\
		shim.o shim_functions.o util.o wpapi.o shift.o cpuid.o	\
		meters.o affinity.o					\
		$(LIBS)
	cp libGreenMPI.so ${HOME}/GreenMPI/local/lib

shim.o: Makefile shim.c shim.h util.o wpapi.o shift.o cpuid.o 		\
		$(GENERATED_SHIMFILES) 
	$(MPICC) -fPIC -DBLR_DONOTUSEOPT13 -DBLR_USE_EAGER_LOGGING -c shim.c
	$(MPICC) -fPIC -c shim_functions.c

util.o: Makefile util.c util.h
	$(MPICC) $(CFLAGS) $(INCDIR) -fPIC -c util.c

wpapi.o: Makefile wpapi.c wpapi.h
	$(MPICC) $(CFLAGS) $(INCDIR) -fPIC -c wpapi.c

shift.o: Makefile shift.c shift.h cpuid.o
	$(MPICC) $(CFLAGS) $(INCDIR) -fPIC -c shift.c

cpuid.o: Makefile cpuid.c cpuid.h 
	$(MPICC) $(CFLAGS) $(INCDIR) -fPIC -c cpuid.c

meters.o: Makefile meters.c meters.h util.o 
	$(MPICC) $(CFLAGS) $(INCDIR) -fPIC -c meters.c

affinity.o: Makefile affinity.c affinity.h 
	$(MPICC) $(CFLAGS) $(INCDIR) -fPIC -c affinity.c


$(GENERATED_SHIMFILES): Makefile shim.py shim.sh
	echo $(SHELL)
	rm -f $(GENERATED_SHIMFILES)
	./shim.sh
	chmod 440 $(GENERATED_SHIMFILES)



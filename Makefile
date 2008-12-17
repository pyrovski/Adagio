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
ADAGIO_FLAGS= -mca gmpi_algo adagio 
MCA_REQUIRED_FLAGS=-mca btl self,tcp		# used on eponymous to shut up the stupid ubuntu dist errors.

# Runtime environment
MPIRUN=mpirun
NP=4

# Compile environment

MPICC=mpicc
CFLAGS=-O0 -Wall -g
LIBDIR=-L$(HOME)/GreenMPI/local/lib
INCDIR=-I$(HOME)/GreenMPI/local/include
LIBS=-lc -lm -lunwind -lmd5 -lpapi 
GENERATED_SHIMFILES = shim_enumeration.h shim_functions.c shim_parameters.h 	\
shim_selection.h  shim_str.h  shim_structs.h  shim_union.h			


all: Makefile harness_pristine harness
	echo Done


# Test runs
spin: Makefile harness harness_pristine
#	$(MPIRUN) -np 2 -hostfile $(HOSTFILE) $(MCA_REQUIRED_FLAGS) $(GMPI_FLAGS) \
#		./harness -v --test_spin 
#		cat runtime* > spin.nosched
#		rm -rf runtime*
	$(MPIRUN) -np 2 -hostfile $(HOSTFILE) $(MCA_REQUIRED_FLAGS) $(GMPI_FLAGS) \
		$(ADAGIO_FLAGS)	\
		./harness -v --test_spin 
		cat runtime* > spin.adagio
		rm -rf runtime*
ping: Makefile harness
	$(MPIRUN) -np 2 -hostfile $(HOSTFILE) $(MCA_REQUIRED_FLAGS) $(GMPI_FLAGS) \
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
	mv libGreenMPI.so ${HOME}/GreenMPI/local/lib

shim.o: Makefile shim.c shim.h util.o wpapi.o shift.o cpuid.o 		\
		$(GENERATED_SHIMFILES) 
	$(MPICC) -fPIC -DUSE_EAGER_LOGGING -c shim.c
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



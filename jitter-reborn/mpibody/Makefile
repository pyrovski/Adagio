CC = mpicc
MPIJACK =  -L/osr/pac/lib -lmpijack
OPT = -O2
LIBS = $(MPIJACK) -L~springer/lib -lmpi -lm
OBJS = mpibody.o SimpleGraphics.o

all : mpibody

again : clean all

.c.o:
	$(CC) $(OPT) -g -c $<

mpibody : $(OBJS)
	$(CC) $(OPT) -g $(LIBS) -o $@ $(OBJS)

#=======================================================================

clean : 
	rm mpibody $(OBJS)

#=======================================================================


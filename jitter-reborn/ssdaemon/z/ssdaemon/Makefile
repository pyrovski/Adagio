CC = gcc
OPT = -O2 -g
LIBS = -L/osr/pac/lib -lsscaling

all : libsscaling.so ssdaemon ssclient

again : clean all

.c.o:
	$(CC) $(OPT) -c $<

libsscaling.so : sscaling.c sscaling.h
	gcc $(OPT) -shared -o libsscaling.so sscaling.c
	cp libsscaling.so /osr/pac/lib

ssdaemon : common.o ssdaemon.o
	$(CC) $(OPT) $(LIBS) -o $@ common.o ssdaemon.o

ssclient: common.o ssclient.o
	$(CC) $(OPT) $(LIBS) -o $@ common.o ssclient.o

#=======================================================================

clean :
	rm libsscaling.so ssdaemon ssclient common.o ssdaemon.o ssclient.o

#=======================================================================


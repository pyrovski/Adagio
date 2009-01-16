CC = gcc
OPT = -O2 -g -fPIC 
LIBS = -L. -L/osr/pac/lib -L/osr/pac/lib64 -lsscaling

all : libsscaling.so ssdaemon ssclient

again : clean all

.c.o:
	$(CC) $(OPT) -c $<

libsscaling.so : sscaling.c sscaling.h common.o
	gcc $(OPT) -shared -o libsscaling.so sscaling.c common.o

ssdaemon : common.o ssdaemon.o
	$(CC) $(OPT) $(LIBS) -o $@ common.o ssdaemon.o

ssclient: common.o ssclient.o
	$(CC) $(OPT) $(LIBS) -o $@ common.o ssclient.o

# put all the compiled files where they need to be
dist:
	if [ `arch` = "x86_64" ] ; then cp ssdaemon       /osr/pac/bin64 ; else cp ssdaemon       /osr/pac/bin ; fi 
	if [ `arch` = "x86_64" ] ; then cp ssclient       /osr/pac/bin64 ; else cp ssclient       /osr/pac/bin ; fi 
	if [ `arch` = "x86_64" ] ; then cp libsscaling.so /osr/pac/lib64 ; else cp libsscaling.so /osr/pac/lib ; fi 
	cp sscaling.h /osr/pac/include

#=======================================================================

clean :
	rm libsscaling.so ssdaemon ssclient common.o ssdaemon.o ssclient.o

#=======================================================================


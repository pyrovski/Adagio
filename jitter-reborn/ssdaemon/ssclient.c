/**
 * Command-line client for the Simple Scaling Daemon (ssdaemon).
 * By Tyler Bletsch
 *
 * Includes some code based on the UDP server example from the Linux Gazette
 *   (http://www.linuxgazette.com/node/8758)
 */

#include "common.h"
#include "sscaling.h"

#define TITLE "Simple Scaling Daemon Client (ssclient) 1.00"

/**
 * Emit some help info to the console.  If errorMsg is non-NULL, it will be
 * displayed afterward.  Finally, the program will exit.
 *
 * @param progName Name of this program, i.e. argv[0]
 * @param errorMsg Optional error message to display after help info
 */
void usage(char* progName, char* errorMsg) {
	printf(
			TITLE "\n"
			"  By Tyler Bletsch\n"
			"\n"
			"Get or set the CPU gear on any machine running ssdaemon.\n"
			"\n"
			"Syntax:\n"
			"  %s [-m] [-v] [-P<port>] [-H<host>] [gear]\n"
			"\n"
			"Options:\n"
			"  -m  Machine readable; Output either ERROR or the resulting gear alone\n"
			"  -v  Verbose; output every step of the way.\n"
			"  -P  Port; set the UDP port to the given <port> (default is %d)\n"
			"  -H  Host; set the host to query to <host> (default is 'localhost')\n"
			"\n"
			"Parameters:\n"
			"  gear  Target gear (if unspecified, then we query the current gear).\n"
			"\n"
			,progName,SSD_DEFAULT_PORT);
	if (errorMsg) printf("ERROR: %s\n",errorMsg);
	exit(0);
}

/**
 * Entry point for command-line client.
 */
int main(int argc, char** argv) {
	// Options
	char bVerbose=0,bMachineReadable=0;

	// Program name (i.e. argv[0])
	char* progName;

	// Port number to connect to (set to default)
	unsigned short port = SSD_DEFAULT_PORT;

	// Host to connect to (set to default)
	char host[128] = "localhost";

	// CPU speed gear to be sent/received
	int gear=-1;
	
	progName = argv[0]; // Note our program name
	argc--; argv++; // ...and get rid of it

	// Get all cmdline args
	while (argc >= 1 && argv[0][0]=='-') {
		char opt=argv[0][1]; // First letter
		char* arg=argv[0]+2; // Rest of it comprises the option's "argument"
		
		switch (opt) {
			case 'v': bVerbose=1; break;
			case 'm': bMachineReadable=1; break;
			case 'H':
				strcpy(host,arg);
				if (strlen(host) == 0) usage(progName,"-H requires a hostname, e.g. '-Hpac00'");
				break;
			case 'P':
				port = atoi(arg);
				if (port <= 0) usage(progName,"-P requires a numeric port, e.g. '-P2828'");
				break;
			case 'h': usage(progName,NULL); // -h implicitly shows help message
			default: usage(progName, "Unrecognized option"); // Invalid argument, give the help message and exit
		}
		
		argc--; argv++; // Shift cmdline arguments to get rid of this option we just processed
	}


	if (argc==1) {
		if (sscanf(argv[0],"%d",&gear) != 1) usage(progName, "Invalid gear");
	} else if (argc>1) {
		usage(progName,"Too many arguments");
	}
	
	//if (!bMachineReadable) printf(TITLE "\n\n");

	int result;
	if (gear == -1) { // Query
		int freqKHz;
		result = ss_getGearRemote(host,port,&gear,&freqKHz,bVerbose?3:(bMachineReadable?0:1));
		if (result==0) {
			if (bMachineReadable) {
				printf("%d\n",gear);
			} else {
				printf("Gear %d (%d MHz)\n",gear,freqKHz/1000);
			}
		} else {
			printf("ERROR\n");
		}
	} else { // Command
		result = ss_setGearRemote(host,port,gear,bVerbose?3:(bMachineReadable?0:1));
		if (result==0) {
			if (bMachineReadable) {
				printf("%d\n",gear);
			} else {
				printf("Set to gear %d.\n",gear);
			}
		} else {
			printf("ERROR\n");
		}
	}
}

/**
 * Daemon implementation for the Simple Scaling Daemon (ssdaemon).
 * By Tyler Bletsch
 *
 * Includes some code based on the UDP server example from the Linux Gazette
 *   (http://www.linuxgazette.com/node/8758)
 */

#include "common.h"
#include "sscaling.h"

#define TITLE "Simple Scaling Daemon (ssdaemon) 1.00"

// If you have some crazy machine with 1000 gears, put 1000 here
#define MAX_NUM_GEARS 16 

// Filesystem hooks to CPU scaling system
#define FILE_FREQ_LIST "/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies"
#define FILE_FREQ_SET  "/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"

/// Array correlating gear numbers (e.g. 0..6) to speeds (e.g. 2000000,1800000,etc.)
int gearSpeeds[MAX_NUM_GEARS];

/// Number of available gears detected (number of elements in gearSpeeds[])
int numGears;

char bVerbose;

/**
 * Emit some help info to the console.  The program will then exit.
 *
 * @param progName Name of this program, i.e. argv[0]
 */
void usage(char* progName) {
	printf(
			TITLE "\n"
			"  By Tyler Bletsch\n"
			"\n"
			"Syntax:\n"
			"  %s [-d [-O]] [-v] [port]\n"
			"\n"
			"Options:\n"
			"  -d  Daemon mode; fork a daemon process to run in background\n"
			"  -O  When daemonizing, don't redirect stdout/stderr to null\n"
			"  -v  Verbose; output every step of the way.\n"
			"\n"
			"Parameters:\n"
			"  port  Override the default listening port (normally %d).\n"
			,progName,SSD_DEFAULT_PORT);
	exit(0);
}

/**
 * Read the available frequencies list and populate gearSpeeds[]
 *
 * @return Zero on success, -1 otherwise
 */
char readGears() {
	numGears=0;
	
	FILE* fp = fopen(FILE_FREQ_LIST,"r");
	if (!fp) return -1;

	while (fscanf(fp,"%d",&gearSpeeds[numGears])==1) { numGears++; }

	fclose(fp);

	if (numGears<=0) return -1;
	
	return 0;
}

/**
 * Make the current process a daemon by forking and detaching I/O.
 * This method is suitable to detach any process from the current
 * terminal and run it in the background.
 *
 * (Adapted from the metertools package)
 *
 * @param bOutputToNull Set to true to redirect stdout/stderr
 * 
 */
void daemonize(char bOutputToNull) {
	int fd;
	
	switch (fork()) {
		case -1:
			die("Unable to fork");
		case 0:
			break;
		default:
			exit(0);
	}
	
	if (setsid() == -1) {
		die("Unable to set process group");
	}
	
	if ((fd = open("/dev/null", O_RDWR, 0)) != -1) {
		dup2(fd, STDIN_FILENO);
		if (bOutputToNull) dup2(fd, STDOUT_FILENO);
		if (bOutputToNull) dup2(fd, STDERR_FILENO);
		if (fd > 2) close(fd);
	} else {
		die("Unable to detach file discriptors");
	}
}

/**
 * Sets the CPU gear to the one specified.
 *
 * @param gear Gear to switch to
 *
 * @return Zero on success, -1 otherwise
 */
char setSpeed(int gear) {
	FILE* fp = fopen(FILE_FREQ_SET,"w");
	if (!fp) return -1;

	if (bVerbose) printf("Setting to gear %d (%d MHz).\n",gear,gearSpeeds[gear]);
	
	fprintf(fp,"%d",gearSpeeds[gear]);

	fclose(fp);

	return 0;
}

/**
 * Reads the CPU gear and returns it.
 *
 * @return The current CPU gear, -1 on error
 */
int getSpeed() {
	int freqKHz,gear;
	
	FILE* fp = fopen(FILE_FREQ_SET,"r");
	if (!fp) return -1;

	if (fscanf(fp,"%d",&freqKHz) != 1) return -1;

	fclose(fp);

	for (gear=0; gear<numGears; gear++) {
		if (gearSpeeds[gear] == freqKHz) {
			return gear;
			if (bVerbose) printf("Reporting gear %d (%d MHz).\n",gear,freqKHz);
		}
	}

	// No such gear?
	return -1;
	
}

/**
 * Daemon entry point.
 */
int main(int argc, char** argv) {
	int sock; // Listening UDP socket

	// Options
	char bDaemon=0,bOutputToNull=1;
	
	bVerbose=0; // Set global verbosity option to false

	// Program name (i.e. argv[0])
	char* progName;

	// Port to listen to (set to default)
	unsigned short port = SSD_DEFAULT_PORT;
	
	progName = argv[0]; // Note our program name
	argc--; argv++; // ...and get rid of it

	// Get all cmdline args
	while (argc >= 1 && argv[0][0]=='-') {
		switch (argv[0][1]) {
			case 'v': bVerbose=1; break;
			case 'O': bOutputToNull=0; break;
			case 'd': bDaemon=1; break;
			default: usage(progName); // Invalid argument, give the help message and exit
		}
		
		argc--; argv++; // Shift cmdline arguments to get rid of this option we just processed
	}

	// Check for port number on command line
	if (argc == 1) {
		port = atoi(argv[0]);
		if (port == 0) {
			fprintf(stderr,"Invalid port number '%s'!\n",argv[0]);
			return EXIT_FAILURE;
		}
	} else if (argc > 1) {
		usage(progName);
	}

	// Emit banner with options listing
	printf(
			TITLE "\n"
			"\n"
			"Options:\n"
			"  Verbose?      : %s\n"
			"  Daemon?       : %s\n"
			"  OutputToNull? : %s  (daemon mode only)\n"
			"\n",
	tf(bVerbose),
	tf(bDaemon),
	tf(bOutputToNull));

	// Read the gears file
	if (readGears() != 0) {
		fprintf(stderr,"Unable to read frequencies list from '%s'.\n",FILE_FREQ_LIST);
		return EXIT_FAILURE;
	}

	// Emit the detected gears
	printf("Gears:\n");
	int i;
	for (i=0; i<numGears; i++) {
		printf("  [%d] %d MHz\n",i,gearSpeeds[i]/1000);
	}
	printf("\n");

	// Become a daemon if we should
	if (bDaemon) {
		printf("Forking daemon...\n");
		daemonize(bOutputToNull);
	}

	// Create UDP socket for listening
	sock = makeUDPSocket(port);

	printf("Up and running!\n");
	while (1) { // Server loop
		struct sockaddr_in addrOther;
		SSDMessage msgIn,msgOut;
		int result;

		// Get next request
		result = recvMessage(sock, &msgIn, &addrOther);
		if (result != 0) {
			printf("Error receiving message...\n");
			continue;
		}

		if (bVerbose) printf("Got message %s.\n",msgToString(&msgIn));

		// Handle it
		char bValid;
		switch (msgIn.type) {
			case SSD_SET_SPEED:
				// Determine if this is a valid gear
				bValid = (msgIn.gear >= 0 && msgIn.gear < numGears);

				// Prepare response
				if (bValid) {
					msgOut.type = SSD_ACK;
					msgOut.gear = msgIn.gear;
					msgOut.freqKHz = gearSpeeds[msgIn.gear];
				} else {
					msgOut.type = SSD_NAK;
					msgOut.gear = -1;
					msgOut.freqKHz = -1;
				}

				// Send response
				if (bVerbose) printf("Sending response %s.\n",msgToString(&msgOut));
				sendMessage(sock, &msgOut, &addrOther);

				// Actually set gear
				if (bValid) setSpeed(msgIn.gear);
				break;

			case SSD_GET_SPEED:
				// Prepare response & check for validity
				msgOut.gear = getSpeed();
				
				bValid = (msgOut.gear >= 0 && msgOut.gear < numGears);
				
				if (bValid) {
					msgOut.type = SSD_ACK;
					msgOut.freqKHz = gearSpeeds[msgOut.gear];
				} else { // Error, send NAK
					msgOut.type = SSD_NAK;
					msgOut.gear = -1;
					msgOut.freqKHz = -1;
				}

				// Send response
				if (bVerbose) printf("Sending response %s.\n",msgToString(&msgOut));
				sendMessage(sock, &msgOut, &addrOther);
				break;

			case SSD_ACK:
			case SSD_NAK:
				printf("Daemon got an ACK/NAK?!?\n");

				break;
			default:
				printf("Daemon got message with invalid type %s.\n",msgToString(&msgIn));

		}
		
	}

	// We never get here, but who cares
	return EXIT_SUCCESS;
}

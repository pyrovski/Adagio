/**
 * Shared library code for talking to the Simple Scaling Daemon (ssdaemon).
 * By Tyler Bletsch
 */

#include "common.h"
#include "sscaling.h"

int sock;

/**
 * Initialize socket at library load time.
 */
static void __attribute__((constructor)) initialize() {
	//printf("sscaling library initialized.\n");
	sock = makeUDPSocket(0);
}

/**
 * Close down socket at library unload time.
 */
static void __attribute__((destructor)) exterminate() {
	//printf("sscaling library unloading.\n");
	close(sock);
}

/**
 * Send a gear-change request to the local ssdaemon.
 *
 * @param gear Gear to switch to
 * @param verbosity 0=No output, 1=Errors to stderr, 2=Success to stdout,
 *                  3=Debug dump of message traffic
 *
 * @return Zero on success, -1 otherwise
 */
int ss_setGear(int gear, char verbosity) {
	return ss_setGearRemote("localhost",SSD_DEFAULT_PORT,gear,verbosity);
}

// Fortran wrapper
int ss_setgear_(int* gear, char* verbosity) {
	if (!gear || !verbosity) {
		fprintf(stderr,"ss_setgear_: Fortran wrapper got null pointer!\n");
		return -1;
	}
	return ss_setGear(*gear, *verbosity);
}

/**
 * Send a gear-change request to the ssdaemon on another machine.  The default
 * port for this service is SSD_DEFAULT_PORT, specified in sscaling.h.
 *
 * @param host Hostname of the machine needing the gear change
 * @param port Port number of the ssdaemon service (default SSD_DEFAULT_PORT)
 * @param gear Gear to switch to
 * @param verbosity 0=No output, 1=Errors to stderr, 2=Success to stdout
 *                  3=Debug dump of message traffic
 *
 * @return Zero on success, -1 otherwise
 */
int ss_setGearRemote(char* host, int port, int gear, char verbosity) {
	// Address of daemon
	struct sockaddr_in addrOther;

	// Message received and to be sent
	SSDMessage msgIn, msgOut;

	int result;

	// Look up target
	if (makeAddress(&addrOther, host, port) != 0) {
		if (verbosity>=1) fprintf(stderr,"Unable to resolve hostname '%s'.\n",host);
		return -1;
	}

	// Prepare request
	msgOut.type = SSD_SET_SPEED;
	msgOut.gear = gear;
	msgOut.freqKHz = -1;

	int try;
	for (try=0; try<RETRIES; try++) {
		// Send request
		if (verbosity>=3) printf("Sending request %s to '%s'.\n",msgToString(&msgOut),host);
		sendMessage(sock, &msgOut, &addrOther);
	
		// Try to get the result from it (wait at most TIMEOUT ms)
		result = recvMessageWithTimeout(sock, &msgIn, &addrOther, TIMEOUT);

		if (result == 0) { // We got a message
			
			if (verbosity>=3) printf("Got message %s.\n",msgToString(&msgIn));
			
			if (msgIn.type == SSD_ACK) { // And it's the right one!
				if (verbosity>=2) printf("Frequency is changing to gear %d (%d Mhz).\n",msgIn.gear,msgIn.freqKHz/1000);
				return 0;
			} else if (msgIn.type == SSD_NAK) { // But the server reports error
				if (verbosity>=1) fprintf(stderr,"Received a NAK indicating some daemon-side error.\n");
				return -1;
			} else { // But it's not an ACK/NAK?  Weird...
				if (verbosity>=1) fprintf(stderr,"Received a non-ACK/NAK in response?\n");
				return -1;
			}

			
		} else if (result == -1) { // Error getting message (worse than timing out)
			if (verbosity>=1) fprintf(stderr,"Error while receiving ACK!\n");
			return -1;
		}

		// We timed out, so let's just loop...
	}
	
	// We must have run out of retries!
	if (verbosity>=1) fprintf(stderr,"Timed out while waiting for response! (Is ssdaemon running on target?)\n");

	return -1;

}

// Fortran wrapper
int ss_setgearremote_(char* host, int* port, int* gear, char* verbosity) {
	if (!port || !gear || !verbosity) {
		fprintf(stderr,"ss_setgearremote_: Fortran wrapper got null pointer!\n");
		return -1;
	}
	return ss_setGearRemote(host, *port, *gear, *verbosity);
}

/**
 * Send a gear-query request to the ssdaemon on this machine.
 *
 * @param gear Pointer to int representing gear target is running in; ignored
 *             if null
 * @param freqKHz Pointer to int representing target's CPU speed in KHz;
 *                ignored if null
 * @param verbosity 0=No output, 1=Errors to stderr, 2=Success to stdout
 *                  3=Debug dump of message traffic
 *
 * @return Zero on success, -1 otherwise
 */
int ss_getGear(int* gear, int* freqKHz, char verbosity) {
	return ss_getGearRemote("localhost",SSD_DEFAULT_PORT,gear,freqKHz,verbosity);
}

// Fortran wrapper
int ss_getgear_(int* gear, int* freqKHz, char* verbosity) {
	if (!verbosity) {
		fprintf(stderr,"ss_getgear_: Fortran wrapper got null pointer!\n");
		return -1;
	}
	return ss_getGear(gear, freqKHz, *verbosity);
}

/**
 * Send a gear-query request to the ssdaemon on another machine.  The default
 * port for this service is SSD_DEFAULT_PORT, specified in sscaling.h.  
 * 
 *
 * @param host Hostname of the machine needing the gear change
 * @param port Port number of the ssdaemon service (default SSD_DEFAULT_PORT)
 * @param gear Pointer to int representing gear target is running in; ignored
 *             if null
 * @param freqKHz Pointer to int representing target's CPU speed in KHz;
 *                ignored if null
 * @param verbosity 0=No output, 1=Errors to stderr, 2=Success to stdout
 *                  3=Debug dump of message traffic
 *
 * @return Zero on success, -1 otherwise
 */
int ss_getGearRemote(char* host, int port, int* gear, int* freqKHz, char verbosity) {
	// Address of daemon
	struct sockaddr_in addrOther;

	// Message received and to be sent
	SSDMessage msgIn, msgOut;

	int result;

	// Look up target
	if (makeAddress(&addrOther, host, port) != 0) {
		if (verbosity>=1) fprintf(stderr,"Unable to resolve hostname '%s'.\n",host);
		return -1;
	}

	// Prepare request
	msgOut.type = SSD_GET_SPEED;
	msgOut.gear = -1;
	msgOut.freqKHz = -1;

	int try;
	for (try=0; try<RETRIES; try++) {
		
		// Send request
		if (verbosity>=3) printf("Sending request %s to '%s'.\n",msgToString(&msgOut),host);
		sendMessage(sock, &msgOut, &addrOther);

		// Try to get the result from it (wait at most TIMEOUT ms)
		result = recvMessageWithTimeout(sock, &msgIn, &addrOther, TIMEOUT);

		// We got a message
		if (result == 0) {
			if (verbosity>=3) printf("Got message %s.\n",msgToString(&msgIn));
			if (msgIn.type == SSD_ACK) { // And it's the right one!
				if (verbosity>=2) printf("Frequency reported is gear %d (%d Mhz).\n",msgIn.gear,msgIn.freqKHz/1000);
				if (gear)    (*gear) = msgIn.gear;
				if (freqKHz) (*freqKHz) = msgIn.freqKHz;
				return 0;
			} else { // But it's not an ACK?  Weird...
				if (verbosity>=1) fprintf(stderr,"Received a non-ACK in response?\n");
				return -1;
			}
		}
		
		// Error getting message (worse than timing out)
		if (result == -1) {
			if (verbosity>=1) fprintf(stderr,"Error while receiving ACK!\n");
			return -1;
		}

		// Simply loop on timeout...
	}
	
	// We must have run out of retries!
	if (verbosity>=1) fprintf(stderr,"Timed out while waiting for response! (Is ssdaemon running on target?)\n");

	return -1;

}

// Fortran wrapper
int ss_getgearremote_(char* host, int* port, int* gear, int* freqKHz, char* verbosity) {
	if (!port || !verbosity) {
		fprintf(stderr,"ss_getgearremote_: Fortran wrapper got null pointer!\n");
		return -1;
	}
	return ss_getGearRemote(host, *port, gear, freqKHz, *verbosity);
}


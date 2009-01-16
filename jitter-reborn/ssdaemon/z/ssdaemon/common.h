/**
 * Common definitions for the Simple Scaling Daemon (ssdaemon).
 * By Tyler Bletsch
 *
 * Includes some code based on the UDP server example from the Linux Gazette
 *   (http://www.linuxgazette.com/node/8758)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#ifndef COMMON_H
#define COMMON_H

/// Structure of our datagrams
typedef struct _SSDMessage {
	enum {
		SSD_ACK, // Acknowledgement.  We did what you asked and it was OK (gear and freqKHz are valid).
		SSD_NAK, // Negative acknowledgement.  We couldn't do what you asked (gear and freqKHz are -1).
		SSD_SET_SPEED, // Request to set the speed to the given gear (freqKHz ignored).
		SSD_GET_SPEED // Request to find out the current gear and frequency (given gear and freqKHz are -1).
	} type;
	int gear;
	int freqKHz;
} SSDMessage;

/**
 * Resolve to "TRUE" if given thing is true, else "FALSE"
 */
#define tf(b) ((b)?"TRUE":"FALSE")

/**
 * Complain with perror, then exit.
 *
 * @param msg Nature of the problem (don't include newline)
 */
void die(char* msg);

/**
 * Makes and binds a datagram socket.
 *
 * @param port Port to bind the socket to. 0 to leave it up to the OS.
 *
 * @return File descriptor representing the socket.
 */
int makeUDPSocket(unsigned short port);

/**
 * Sets up an internet address structure.
 *
 * @param addr Address to set up.
 * @param hostname Dotted decimal IP address or DNS name to set it as.
 * @param port Port number of this endpoint (0 for OS-created default)
 *
 * @note ip is there for clients wishing to set up the address of a machine
 * they want to connect to. Passing in NULL will use the local
 * address of the machine. This is useful for servers setting
 * themselves up.
 */
int makeAddress(struct sockaddr_in* addr, char* hostname, unsigned short port);

/**
 * Receive a SSDMessage from the given socket.
 *
 * @param sock Socket to receive from
 * @param msg Pointer to SSDMessage struct to write to
 * @param addrOther Address of peer noted here
 *
 * @return Zero on success, else -1.
 */
int recvMessage(int sock, SSDMessage* msg, struct sockaddr_in* addrOther);

/**
 * Send a SSDMessage from the given socket.
 *
 * @param sock Socket to use when send
 * @param msg Pointer to SSDMessage struct to send
 * @param addrOther Address of peer to send to
 *
 * @return Zero on success, else -1.
 */
int sendMessage(int sock, SSDMessage* msg, struct sockaddr_in* addrOther);

/**
 * Return a static string buffer describing the given message
 *
 * @param msg Message to describe
 *
 * @return Static string buffer describing it
 */
char* msgToString(SSDMessage* msg);

#endif // COMMON_H


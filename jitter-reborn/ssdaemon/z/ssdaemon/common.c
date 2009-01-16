/**
 * Common code for the Simple Scaling Daemon (ssdaemon).
 * By Tyler Bletsch
 * 
 * Includes some code based on the UDP server example from the Linux Gazette
 *   (http://www.linuxgazette.com/node/8758)
 */

#include "common.h"

/**
 * Complain with perror, then exit.
 *
 * @param msg Nature of the problem (don't include newline)
 */
void die(char* msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}


/**
 * Makes and binds a datagram socket.
 *
 * @param port Port to bind the socket to. 0 to leave it up to the OS.
 *
 * @return File descriptor representing the socket.
 */
int makeUDPSocket(unsigned short port) {
	struct sockaddr_in addr;
	int fd;

	fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd == -1) die("socket");

	if (port != 0) {
		makeAddress(&addr, NULL, port);
		if (bind(fd, (struct sockaddr*) &addr, sizeof addr) < 0) die("bind");
	}

	return fd;
}

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
int makeAddress(struct sockaddr_in* addr, char* hostname, unsigned short port) {
	struct hostent* h;

	memset(addr, 0, sizeof(struct sockaddr_in));
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);

	if (hostname == NULL) { // Local machine on IP
		addr->sin_addr.s_addr = htonl(INADDR_ANY);
	} else {
		// Get IP address (no check if input is IP address or DNS name).
		h = gethostbyname(hostname);
		if (h == NULL) return -1;

		memcpy(&addr->sin_addr.s_addr, h->h_addr_list[0], h->h_length);
	}

	return 0;
}

/**
 * Receive a SSDMessage from the given socket.
 *
 * @param sock Socket to receive from
 * @param msg Pointer to SSDMessage struct to write to
 * @param addrOther Address of peer noted here
 *
 * @return Zero on success, else -1.
 */
int recvMessage(int sock, SSDMessage* msg, struct sockaddr_in* addrOther) {
	int result;

	size_t addrLen = sizeof(struct sockaddr_in);
	result = recvfrom(sock, msg, sizeof(SSDMessage), 0, (struct sockaddr*)addrOther, &addrLen);

	if (result != sizeof(SSDMessage)) return -1;
	return 0;
}

/**
 * Receives a SSDMessage just like recvMessage, but waits for a given timeout
 * before giving up and returning -2.
 *
 * @param sock Socket to receive from
 * @param msg Pointer to SSDMessage struct to write to
 * @param addrOther Address of peer noted here
 * @param timeout Time to wait for a message (in milliseconds)
 *
 * @return Zero on success, -1 on error, -2 on timeout
 */
int recvMessageWithTimeout(int sock, SSDMessage* msg, struct sockaddr_in* addrOther, int timeout) {
	struct timeval timeval_timeout;
	fd_set handles;
	int result;

	FD_ZERO(&handles);
	FD_SET(sock, &handles);
	
	timeval_timeout.tv_sec  =  timeout / 1000;
	timeval_timeout.tv_usec = (timeout % 1000) * 1000;

	result = select(sock+1,&handles,NULL,NULL,&timeval_timeout);

	if (result<0) return -1;
	if (result==0) return -2;

	// Actually get it with the normal function
	return recvMessage(sock,msg,addrOther);

}

/**
 * Send a SSDMessage from the given socket.
 *
 * @param sock Socket to use when send
 * @param msg Pointer to SSDMessage struct to send
 * @param addrOther Address of peer to send to
 *
 * @return Zero on success, else -1.
 */
int sendMessage(int sock, SSDMessage* msg, struct sockaddr_in* addrOther) {
	int result;
	
	size_t addrLen = sizeof(struct sockaddr_in);
	result = sendto(sock, msg, sizeof(SSDMessage), 0, (struct sockaddr*)addrOther, addrLen);

	if (result != sizeof(SSDMessage)) return -1;
	return 0;
}

/**
 * Return a static string buffer describing the given message
 *
 * @param msg Message to describe
 *
 * @return Static string buffer describing it
 */
char* msgToString(SSDMessage* msg) {
	static char buf[256];
	static char typeBuf[64];

	switch (msg->type) {
		case SSD_ACK: strcpy(typeBuf,"SSD_ACK"); break;
		case SSD_SET_SPEED: strcpy(typeBuf,"SSD_SET_SPEED"); break;
		case SSD_NAK: strcpy(typeBuf,"SSD_NAK"); break;
		case SSD_GET_SPEED: strcpy(typeBuf,"SSD_GET_SPEED"); break;
		default: sprintf(typeBuf,"Unknown(%d)",msg->type); break;
	}
	
	sprintf(buf,"{Type=%s Gear=%d FreqKHz=%d}",typeBuf,msg->gear,msg->freqKHz);

	return buf;
}

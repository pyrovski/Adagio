/**
 * Shared library definitions for talking to the Simple Scaling Daemon (ssdaemon).
 * By Tyler Bletsch
 */

#ifndef SSCALING_H
#define SSCALING_H

#define SSD_DEFAULT_PORT 2828

// Timeout for waiting for an ACK from the ssdaemon
#define TIMEOUT 200 // ms

// Number of attempts to send the command before giving up
#define RETRIES 5


/**
 * Send a gear-change request to the local ssdaemon.
 *
 * @param gear Gear to switch to
 * @param verbosity 0=No output, 1=Errors to stderr, 2=Success to stdout
 *                  3=Debug dump of message traffic
 *
 * @return Zero on success, -1 otherwise
 */
int ss_setGear(int gear, char bVerbose);

/**
 * Send a gear-change request to the ssdaemon on another machine.  The default
 * port for this service is SSD_DEFAULT_PORT, specified in sscaling.h.
 *
 * @param gear Gear to switch to
 * @param verbosity 0=No output, 1=Errors to stderr, 2=Success to stdout
 *                  3=Debug dump of message traffic
 *
 * @return Zero on success, -1 otherwise
 */
int ss_setGearRemote(char* host, int port, int gear, char bVerbose);

/**
 * Send a gear-query request to the ssdaemon on this machine.
 *
 * @param gear Pointer to int representing gear target is running in
 * @param freqKHz Pointer to int representing target's CPU speed in KHz
 * @param verbosity 0=No output, 1=Errors to stderr, 2=Success to stdout
 *                  3=Debug dump of message traffic
 *
 * @return Zero on success, -1 otherwise
 */
int ss_getGear(int* gear, int* freqKHz, char verbosity);

/**
 * Send a gear-query request to the ssdaemon on another machine.  The default
 * port for this service is SSD_DEFAULT_PORT, specified in sscaling.h.
 *
 * @param host Hostname of the machine needing the gear change
 * @param port Port number of the ssdaemon service (default SSD_DEFAULT_PORT)
 * @param gear Pointer to int representing gear target is running in
 * @param freqKHz Pointer to int representing target's CPU speed in KHz
 * @param verbosity 0=No output, 1=Errors to stderr, 2=Success to stdout
 *                  3=Debug dump of message traffic
 *
 * @return Zero on success, -1 otherwise
 */
int ss_getGearRemote(char* host, int port, int* gear, int* freqKHz, char verbosity);

#endif // SSCALING_H

#ifndef __NET_H
#define __NET_H

/* Communicate on this port */
#define LOCAL_PORT 54321

/* Values we need to keep track of for clients */
double cumulative_energy;
double cumulative_interval;

/* Standard for messages */
struct net_msg {
  unsigned int cmd;
  unsigned int val;
  double val2;
};

void * request_handler(void *arg);

/* Commands */
#define CMD_GET_ENERGY 0
#define CMD_CLOSE      1
#define CMD_ENERGY     10
#define CMD_GET_AVE_P      11

#endif

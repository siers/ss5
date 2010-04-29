#ifndef GUARD_SRC_NETWORKING_H_
#define GUARD_SRC_NETWORKING_H_

#include <netinet/in.h>
#include <signal.h>
#include "defs.h"
//#define blen 8192
#define blen 8192 * 10

extern uint16_t port;

char resolvehost(struct sockaddr_in*, char*);
int myconnect_ip(uint32_t, uint16_t, int*);
int myconnect_domain(char*, uint16_t, int*, uint32_t*);
int create_socket(uint16_t alterport);
void* cat_socket(void* m);
void fuse_sockets(int, int, s_client*);
sig_atomic_t accept_loop(int fd);

#endif /* GUARD_SRC_NETWORKING_H_ */

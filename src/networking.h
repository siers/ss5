#ifndef GUARD_SRC_NETWORKING_H_
#define GUARD_SRC_NETWORKING_H_

#include <netinet/in.h>
#include <signal.h>
#include "defs.h"
#define BUF_SIZE 8192
#define TIMEOUT 500

extern uint16_t port;

char resolvehost(struct sockaddr_in*, char*);
int myconnect_ip(s_client*);
int myconnect_domain(s_client*);
int create_socket(uint16_t alterport);
void* cat_socket(void* m);
void fuse_sockets_select(int, int);
void fuse_sockets_poll(int, int);
sig_atomic_t accept_loop(int fd);

#endif /* GUARD_SRC_NETWORKING_H_ */

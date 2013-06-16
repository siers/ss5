#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "log.h"

#define VERSION "SS5, v1.1\n"
#define SOCKS_VER 5
#define PORT 1337
#define INVALID_ATYP 255

struct server {
  int fd;
  int port;
};

struct client {
    /* Client Details (about one that connected) */
    int fd; // Incoming socket descriptor.
    struct sockaddr_in addr;
      /* Protocolish stuff */
        char ver;
        char method;
        char cmd;
        char atyp;

    /* Connection Details (about one we made) */
    int sd; // Outgoing socket descriptor.
    char daddr[255]; // Buffer for recv'ing domain/ip.
    uint16_t dport;
    uint32_t ip;
    char buf[255];
};

extern int dead;
extern int port;
extern int loglvl;

/* server.c */

int server(struct server*, int);

/* proto.c */
void client_daemon(struct client*);

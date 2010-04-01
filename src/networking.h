#include <netinet/in.h>
#include <signal.h>
#define blen 8096

extern uint16_t port;

char resolvehost(struct sockaddr_in* buf, char* host);
int myconnect_ip(int ip, uint16_t port, int* buf);
int myconnect_domain(char* host, uint16_t port, int* buf, void* ip);
int create_socket(uint16_t alterport);
void smash_sockets(int sd1, int sd2, void*);
sig_atomic_t accept_loop(int fd);

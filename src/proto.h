#include <netinet/in.h>
#define INVALID_ATYP 255

char make_connection(char cmd, char atyp, char dstaddr[], uint16_t port,
        void* srcip, int* sock, void* ip);
char talk(int, void* srcip);
void talk_wrapper(int fd, struct sockaddr_in client_addr);

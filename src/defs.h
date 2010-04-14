#ifndef GUARD_SRC_STRUCTS_H_
#define GUARD_SRC_STRUCTS_H_
#include <stdint.h>
#include <netinet/in.h>

#define SOCKS_VER 5
#define INVALID_ATYP 255

typedef struct {
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
    char daddr[255]; // Could someone find out what does this?
    uint16_t dport;
    uint32_t ip;
    char buf[255];
} s_client; // SOCKS client.

#endif /* GUARD_SRC_STRUCTS_H_ */

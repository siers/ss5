#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "notify.h"
#include "proto.h"
#include "networking.h"
#include "defs.h"

extern int errno;

/*
 * {{{ Make_connection() return values:
 * + X'00' succeeded
 * + X'01' general SOCKS server failure
 * + X'02' connection not allowed by ruleset
 * + X'03' Network unreachable
 * + X'04' Host unreachable
 * + X'05' Connection refused
 * + X'06' TTL expired
 * + X'07' Command not supported
 * + X'08' Address type not supported
 * + X'09' to X'FF' unassigned
 * +------------------------------- }}}
 */

char
make_connection(s_client* client)
{ // {{{ Big-ass switch/if function.
    unsigned int destip = *((int*) client->daddr);
#ifdef SOCK_VERBOSE
    notify_making_connection(client);
#endif
    switch (client->cmd)
    {
        case 1: // CONNECT
            switch (client->atyp)
            {
                //char myconnect_ip(int ip, uint16_t port, int* buf)
                //char myconnect_domain(char* host, uint16_t port, int* buf)
                case 1: // IPv4
                    client->ip = destip;
                    if (!myconnect_ip(client->ip, client->dport,
                                &client->sd))
                        return 0;
                    break;
                case 3: // Domain name.
                    if (!myconnect_domain(client->daddr, client->dport,
                                &client->sd, &client->ip))
                        return 0;
                    break;
                case 4: // IPv6
                    return 8;
                    break;
            }
            switch (errno) {
                case ENETUNREACH:
                    return 3;
                case EHOSTUNREACH:
                    return 4;
                case ECONNREFUSED:
                    return 5;
                default:
                    return 1;
            }
            break;
        /* case 2: // BIND
            break;
        case 3: // UDP
            break; */
        default:
            return 7;
    }
    return 0; /// Big-ass function }}}
}

char
talk(s_client* client)
{
    /* {{{ Handshake */
    int ret; // For saving various return values.
    uint8_t buf[255], nmethods;
    // Client sends (VERSION, NMETHODS) -- we read it
    if ((ret = recv(client->fd, &buf, 2, 0)) != 2)
        return 1;
    client->ver = buf[0];
    nmethods = buf[1];
    // We read all the nMETHODS
    if (!recv(client->fd, &buf, nmethods, 0) == nmethods)
        return 2;
    // We choose the one we like -- \x00(no auth)
    int choice, c;
    for (choice = -1, c = 0;
            c < nmethods && (1 << 9 & choice) &&
              client->ver == SOCKS_VER;
            c++)
        switch (buf[c]) {
            case 0x00:
            //case 0x02: // in future, possibly
                choice = buf[c];
        }
    // We send the METHOD we chosed, and \xFF, if there weren't any we like
    buf[0] = SOCKS_VER;
    if (choice != -1)
        buf[1] = (char) choice;
    else
        buf[1] = 0xff;
    send(client->fd, buf, 2, 0);
    if (choice == -1)
        return 3;
    client->method = buf[1];
#ifdef SOCK_VERBOSE
    snprintf((char*) buf, 255, ": method %i chosed.", client->method);
    notify_custom(&client->addr.sin_addr.s_addr, (char*) buf);
#endif
    /* Handshake }}} */

    /* {{{ Request
     * Client is now sending request — VER, CMD, RESRVD.(\0), ATYP
     * (Full request is VER, CMD, RSV, ATYP, DST.ADDR, DST.PORT) */
    if ((ret = recv(client->fd, buf, 4, 0)) != 4)
        return 4;
    client->cmd = buf[1];
    client->atyp = buf[3];
    // Receiving DST.ADDR
    switch (client->atyp) {
        case 0x01: // IPv4
            if (recv(client->fd, client->daddr, 4, 0) != 4)
                return 6;
            break;
        case 0x03: // Domain name
            // Receiving length of the incoming string.
            if (recv(client->fd, buf, 1, 0) != 1)
                return 7;
            char tmplen = buf[0];
            memset(client->daddr, 0, 255);
            if (recv(client->fd, client->daddr, tmplen, 0) != tmplen)
                return 8;
            break;
        /*case 0x04: // IPv6
            if (recv(fd, buf, 4, 0) != 4)
                return 9;
            break; */
        default:
            client->cmd = INVALID_ATYP; // Invalid command.
    }
    // Receiving DST.PORT from request.
    if (recv(client->fd, &client->dport, 2, 0) != 2)
        return 5;
    client->dport = ntohs(client->dport);
    // Request is filled }}}

    /* {{{ Reply
     * Making a connection(we need to do it before a reply can be sent) */
    ret = make_connection(client);
    /* Reply: VER, REPLY, RSV, ATYP, BIND.VAR, BIND.PORT */
    buf[0] = SOCKS_VER;
    buf[1] = ret;
    buf[2] = 0;
    buf[3] = client->atyp;
    *((uint32_t*) &(buf[4])) = client->ip; // This writes in buf[4 — 7].
    *((uint16_t*) &(buf[8])) = htons(client->dport);
    send(client->fd, buf, 10, 0);
    /* Reply }}} */

    /* {{{ Crap */
#ifdef SOCK_VERBOSE
    snprintf((char*) buf, 255, ": reply sent with reply code %i.", ret);
    notify_custom(&client->addr.sin_addr.s_addr, (char*) buf);
#endif
    if (ret != 0)
        return 10;
#ifdef SOCK_VERBOSE
    notify_custom(&client->addr.sin_addr.s_addr, " is fused.");
#endif
    fuse_sockets(client->fd, client->sd, client);
#ifdef debug
    printf("return from fuse_sockets in talk()\n");
#endif
    shutdown(client->fd, SHUT_RDWR);
    shutdown(client->sd, SHUT_RDWR);
    close(client->sd);
    /* Crap }}} */

    return 0;
}

void
talk_wrapper(s_client client)
{
    // TODO: s/fork/pthreads/
    if (fork() != 0)
        return;
    int ret = talk(&client);
#ifdef SOCK_VERBOSE
    notify_disconnected(&client.addr.sin_addr.s_addr, ret, errno);
#endif
    close(client.fd);
    exit(0);
}

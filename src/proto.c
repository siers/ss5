#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "notify.h"
#include "proto.h"
#include "networking.h"

extern int errno;

/*
 * Make_connection() return values:
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
 * +------------------------------- -  -   -
 */

char make_connection(char cmd, char atyp, char dstaddr[], uint16_t port,
        void* srcip, int* sock, void* ip)
{
    unsigned int destip = *((int*) dstaddr);
#ifdef SOCK_VERBOSE
    notify_making_connection(&destip, atyp, srcip);
#endif
    switch (cmd)
    {
        case 1: // CONNECT
            switch (atyp)
            {
                //char myconnect_ip(int ip, uint16_t port, int* buf)
                //char myconnect_domain(char* host, uint16_t port, int* buf)
                case 1: // IPv4
                    *((uint32_t*) ip) = destip;
                    if (!myconnect_ip(destip, ntohs(port), sock))
                        return 0;
                    break;
                case 3: // Domain name.
                    if (!myconnect_domain(dstaddr, ntohs(port), sock, ip))
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
    return 0;
}

char talk(int fd, void* srcip)
{
    int ret; // For saving various return values.
    uint8_t buf[255], nmethods;
    // Client sends (VERSION, NMETHODS)
    if ((ret = recv(fd, &buf, 2, 0)) != 2)
        return 1;
    nmethods = buf[1];
    // We read all the nMETHODS
    if (!(buf[0] == 5 && recv(fd, &buf, nmethods, 0) == nmethods))
        return 2;
    // We choose the one we like -- \x00(no auth)
    int choice, c;
    for (choice = -1, c = 0; c < nmethods && (1 << 9 & choice); c++)
        switch (buf[c]) {
            case 0x00:
            //case 0x02: // in future, possibly
                choice = buf[c];
        }
    // We send the METHOD we chosed, and \xFF, if there weren't any we like
    buf[0] = 5;
    if (choice != -1)
        buf[1] = (char) choice;
    else
        buf[1] = 0xff;
    send(fd, buf, 2, 0);
    if (choice == -1)
        return 3;

    /* Client is now sending request — VER, CMD, RESRVD.(\0), ATYP
       (Full request is VER, CMD, RSV, ATYP, DST.ADDR, DST.PORT) */
    if ((ret = recv(fd, buf, 4, 0)) != 4)
        return 4;
    uint8_t cmd = buf[1], atyp = buf[3];
    uint8_t buf2[255];
    // Receiving DST.ADDR
    switch (atyp) {
        case 0x01: // IPv4
            if (recv(fd, buf, 4, 0) != 4)
                return 6;
            memcpy(buf2, buf, 4);
            break;
        case 0x03: // Domain name
            if (recv(fd, buf, 1, 0) != 1)
                return 7;
            char tmplen = buf[0];
            memset(buf, 0, 255);
            if (recv(fd, buf, tmplen, 0) != tmplen)
                return 8;
            memcpy(buf2, buf, 4);
            break;
        /*case 0x04: // IPv6
            if (recv(fd, buf, 4, 0) != 4)
                return 9;
            memcpy(buf, buf2, 4);
            break; */
        default:
            cmd = INVALID_ATYP; // Invalid command.
    }
    // Receiving DST.PORT from request.
    if (recv(fd, buf, 2, 0) != 2)
        return 5;
    // Request is filled

    // Making a connection(we need to do it before a reply can be sent)
    int sd; // sd -- socket descriptor
    uint32_t ip;
    //char make_connection(char cmd, char atyp, char dstaddr[], uint16_t port,
    //void* srcip, int* sock, void* ip)
    ret = make_connection(cmd, atyp, (char*) buf2, *((uint16_t*) buf), srcip,
            &sd, &ip);
    buf[0] = 0x05;
    buf[1] = ret;
    buf[2] = 0;
    buf[3] = atyp;
    *((uint32_t*) &(buf[4])) = ip; // This writes in buf[4 — 7].
    send(fd, buf, 10, 0);
    buf[0] = 'a';
    buf[1] = 'b';
    send(sd, buf, 2, 0);

#ifdef SOCK_VERBOSE
    snprintf((char*) buf2, 255, ": reply sent with reply code %i", ret);
    notify_custom(srcip, (char*) buf2);
#endif
    if (ret != 0)
        return 10;
#ifdef SOCK_VERBOSE
    notify_custom(srcip, " is smashed.");
#endif
    smash_sockets(fd, sd, srcip);
    //smash_sockets(sd, sd, srcip);
    close(sd);

    return 0;
}

void talk_wrapper(int fd, struct sockaddr_in client_addr)
{
    int ret = talk(fd, &client_addr.sin_addr.s_addr);
#ifdef SOCK_VERBOSE
    notify_disconnected(&client_addr.sin_addr.s_addr, ret, errno);
#endif
    close(fd);
}

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h> /* For sig_atomic_t. */
#include <sys/select.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>
#include <poll.h>
#include "proto.h"
#include "networking.h"
#include "notify.h"
#include "defs.h"

#ifdef debug
#include <stdio.h>
#include <errno.h>
extern int errno;
#endif

extern sig_atomic_t sigcaught;

/* TODO: Use getaddrinfo(); !!! */
char
resolvehost(struct sockaddr_in* buf, char* host)
{
    struct hostent *hp;
    if ((hp = gethostbyname(host)) == 0) {
        return h_errno;
    }
    memset(&(*buf), 0, sizeof(struct sockaddr_in));
    buf->sin_family = AF_INET;
    buf->sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
    return 0;
}

int
myconnect_ip(s_client* client)
{
    if ((client->sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        return -1;
    }
    struct sockaddr_in cli; // -ent
    bzero(&cli, sizeof(cli));
    cli.sin_family = AF_INET;
    cli.sin_addr.s_addr = client->ip;
    cli.sin_port = htons(client->dport);
    if (connect(client->sd,(struct sockaddr *) &cli, sizeof(cli)) == -1) {
        close(client->sd);
        return -1;
    }
    return 0;
}

int
myconnect_domain(s_client* client)
{
    struct addrinfo *first, *iter, hints;
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags = 0;

    getaddrinfo(client->daddr, NULL, &hints, &first);
    for (iter = first; iter->ai_next != NULL; iter = iter->ai_next)
    {
        if ((client->sd = socket(iter->ai_family, iter->ai_socktype,
                        iter->ai_protocol)) == -1)
            continue;
        if (connect(client->sd, iter->ai_addr, iter->ai_addrlen) != -1)
            break;
        close(client->sd);
    }

    // !! Won't work w/ IPv6.
    memcpy(&client->ip, &((struct sockaddr_in*) iter->ai_addr)->sin_addr,
            sizeof(((struct sockaddr_in*) iter->ai_addr)->sin_addr));
    freeaddrinfo(first);

    if (iter == NULL)
        return -1;
    return 0;
}

int
create_socket(uint16_t alterport)
{
    int sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        return -1;
    struct sockaddr_in srv;
    bzero(&srv, sizeof(srv));
    srv.sin_family = AF_INET;
    if (alterport == 0)
        srv.sin_port = htons(port); // Global port variable.
    else
        srv.sin_port = htons(alterport); // Alternative port.
#ifdef SOCK_VERBOSE
    notify_port_opened(ntohs(srv.sin_port));
#endif
    if (bind(sock, (struct sockaddr*) &srv, sizeof(srv)) || listen(sock, 5))
        return -1;
    char True = 1; // I am being the captain obvious! Weeee...
    // This sockopt allows other sockets to bind() to this port.
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &True, sizeof(True));
    return sock;
}

void
fuse_sockets_select(int fd, int sd, s_client* client)
{
    fd_set fdset, copy;
    struct timeval tv;
    int ret;
    char buf[BUF_SIZE];

    FD_ZERO(&copy);
    FD_SET(fd, &copy);
    FD_SET(sd, &copy);

    while (1)
    {
        memcpy(&fdset, &copy, sizeof fdset);
        tv.tv_sec = 1; tv.tv_usec = 0;
        switch (select(sd+1, &fdset, NULL, NULL, &tv)) {
            case -1: break;
            case 0: continue;
        }
        if (FD_ISSET(fd, &fdset)){
            if (!(ret = recv(fd, buf, BUF_SIZE, 0)))
                break;
            send(sd, buf, ret, 0);
        }
        if (FD_ISSET(sd, &fdset)){
            if (!(ret = recv(sd, buf, BUF_SIZE, 0)))
                break;
            send(fd, buf, ret, 0);
        }
    }
}

void
fuse_sockets_poll(int fd, int sd, s_client* client)
{
    struct pollfd fds[2];
    //char *buf = malloc(BUF_SIZE);
    char buf[BUF_SIZE];
    unsigned char counter;
    int ret;
    fds[0].fd = fd;
    fds[1].fd = sd;
    fds[0].events = POLLIN | POLLNVAL;
    fds[1].events = POLLIN | POLLNVAL;

    while (1) {
        // TIMEOUT's a constant in networking.h
        switch(poll(fds, 2, TIMEOUT)) {
            case -1: return;
            case 0: continue;
        }
        for (counter = 0; counter < 2; counter++) {
            if (fds[counter].revents & POLLNVAL)
                return;
            if (fds[counter].revents & POLLIN) {
                if (1 > (ret = recv(fds[counter].fd, buf, BUF_SIZE - 1, 0))) {
                    return;
                }
                send(fds[counter ^ 1].fd, buf, ret, 0);
            }
        }
    }
}

int
select_wrapper(int fd, int tv_sec)
{
    // Note: (p)select returns 0 on timeout or error(like signal),
    // but modifies fdset on success.
    fd_set fdset;
    struct timeval timeout = {.tv_sec = tv_sec, .tv_usec = 0};
    FD_ZERO(&fdset);
    FD_SET(fd, &fdset);
    return select(fd + 1, &fdset, NULL, NULL, &timeout);
}

sig_atomic_t
accept_loop(int fd)
{
    s_client client;
    client.fd = -1;
    int ret;
    unsigned int addrlen = sizeof(client.addr);
    while (!sigcaught)
    {
#ifdef debug
        printf("Cycle in accept_loop\n");
#endif
        // -1 if ESOMETHING, 0 on timeout, >0 if success.
        if ((ret = select_wrapper(fd, 5)) <= 0)
            continue;
        if ((client.fd = accept(fd, (struct sockaddr*) &client.addr,
                        &addrlen)) == (-1)) {
#ifdef SOCK_VERBOSE
            complain(errno);
#endif
            continue;
        }
#ifdef SOCK_VERBOSE
        notify_connected(&client.addr.sin_addr.s_addr);
#endif
        talk_wrapper(client); // This shouldn't block. (fork or thread)
    }
    return sigcaught;
}

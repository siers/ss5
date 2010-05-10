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
#include "proto.h"
#include "networking.h"
#include "notify.h"
#include "defs.h"

#ifdef debug
#include <stdio.h>
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
myconnect_ip(uint32_t ip, uint16_t port, int* buf)
{
    int sd;
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        return -1;
    }
    struct sockaddr_in cli; // -ent
    bzero(&cli, sizeof(cli));
    cli.sin_family = AF_INET;
    cli.sin_addr.s_addr = ip;
    cli.sin_port = htons(port);
    if (connect(sd,(struct sockaddr *) &cli, sizeof(cli)) == -1) {
        close(sd);
        return -1;
    }
    *buf = sd;
    return 0;
}

int
myconnect_domain(char* host, uint16_t port, int* buf, uint32_t* ip)
{
    int sd;
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        return -1;
    }
    struct sockaddr_in hostaddr;
    if (resolvehost(&hostaddr, host) != 0) {
        close(sd);
        return -2;
    }
    if (connect(sd,(struct sockaddr *) &hostaddr, sizeof(hostaddr)) == -1) {
        close(sd);
        return -1;
    }
    *buf = sd;
    *ip = hostaddr.sin_addr.s_addr;
    return sd;
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
    return sock;
}

void*
cat_socket(void* mem)
{
    thread_arg* arg = (thread_arg*) mem;
    char buf[8192];
    int ret;
    while ((ret = recv(arg->src, buf, 8192, 0)) > 0) {
        send(arg->dest, buf, ret, 0);
    }
#ifdef debug
    char shenanigans[50];
    snprintf(shenanigans, 50, ": exiting thread %i, recv retval = %i.",
            arg->num, ret);
    notify_custom(NULL, shenanigans);
#endif
    pthread_exit(NULL);
    close(arg->src);
    close(arg->dest);
}

void
fuse_sockets(int sd1, int sd2, s_client* client)
{
    pthread_t c, s;
    thread_arg ca = {.src  = dup(client->fd),
        .dest = dup(client->sd), .num = 1};
    thread_arg sa = {.dest = dup(client->fd),
        .src  = dup(client->sd), .num = 1};
    pthread_create(&c, NULL, &cat_socket, &ca);
    pthread_create(&s, NULL, &cat_socket, &sa);

    fd_set fdset;
    do {
        FD_ZERO(&fdset);
        FD_SET(sd1, &fdset);
        FD_SET(sd2, &fdset);
        sleep(1);
    } while (!select(sd2 + 1, &fdset, NULL, NULL, NULL) < 0);
#ifdef debug
    notify_custom(&client->addr.sin_addr.s_addr, ": escape from select, "
            "killing threads.");
#endif
    pthread_cancel(c);
    pthread_cancel(s);
    shutdown(client->fd, SHUT_RDWR);
    shutdown(client->sd, SHUT_RDWR);
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
        // -1 if ESOMETHING, 0 on timeout, >0 if success.
        if ((ret = select_wrapper(fd, 1)) <= 0)
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

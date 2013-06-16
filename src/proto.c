#include "everything.h"

extern int errno;

static int
connect_address(struct client* client)
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

static int
connect_domain(struct client* client)
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

static char
make_connection(struct client* client)
{

    unsigned int destip = *((int*) client->daddr);

    switch (client->cmd)
    {
        case 1: // CONNECT
            switch (client->atyp)
            {
                case 1: // IPv4
                    client->ip = destip;
                    if (!connect_address(client))
                        return 0;
                    break;
                case 3: // Domain name.
                    if (!connect_domain(client))
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

static char
talk(struct client* client)
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

    if (ret != 0)
        return 10;
    else
        return 0;
}

static void
cat(int fd, int sd)
{
    struct pollfd fds[2];
    char buf[BUFSIZ];
    unsigned char counter;
    int ret;
    fds[0].fd = fd;
    fds[1].fd = sd;
    fds[0].events = POLLIN | POLLNVAL;
    fds[1].events = POLLIN | POLLNVAL;

    while (1) {
        // TIMEOUT's a constant in networking.h
        switch(poll(fds, 2, 5000)) {
            case -1: return;
            case 0: continue;
        }

        for (counter = 0; counter < 2; counter++) {
            if (fds[counter].revents & POLLNVAL)
                return;
            if (fds[counter].revents & POLLIN) {
                if (1 > (ret = recv(fds[counter].fd, buf, BUFSIZ - 1, 0))) {
                    return;
                }
                send(fds[counter ^ 1].fd, buf, ret, 0);
            }
        }
    }
}

static void
client_use(struct client* client)
{
    cat(client->fd, client->sd);
    shutdown(client->fd, SHUT_RDWR);
    shutdown(client->sd, SHUT_RDWR);
    close(client->sd);
}

void
client_daemon(struct client* client)
{
    if (fork() != 0)
        return;

    int ret;

    if ((ret = talk(client)) == 0) {
        client_use(client);
    }

    close(client->fd);
    exit(ret);
}

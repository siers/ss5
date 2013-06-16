#include "everything.h"

static int
select_with_seconds(int fd, int tv_sec)
{
    // Note: (p)select returns 0 on timeout or error(like signal),
    // but modifies fdset on success.

    fd_set fdset;
    struct timeval timeout = {.tv_sec = tv_sec, .tv_usec = 0};

    FD_ZERO(&fdset);
    FD_SET(fd, &fdset);

    return select(fd + 1, &fdset, NULL, NULL, &timeout);
}

static int
init(struct server* server, int port)
{
    server->port = port;

    if ((server->fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        return -1;

    struct sockaddr_in srv;
    bzero(&srv, sizeof(srv));
    srv.sin_port   = server->port;
    srv.sin_family = AF_INET;

    if (bind(server->fd, (struct sockaddr*) &srv, sizeof(srv)) || listen(server->fd, 5))
        return -1;

    char True = 1;
    setsockopt(server->fd, SOL_SOCKET, SO_REUSEADDR, &True, sizeof(True));

    return 0;
}

static int
shut(struct server* server)
{
    return close(server->fd);
}

int
server(struct server* server, int port)
{
    struct client client = {.fd = -1};
    unsigned int alen = sizeof(client.addr);
    int ret;

    init(server, port);

    while (!dead)
    {
        // TODO: wait(2) client processes.

        // -1 if ESOMETHING, 0 on timeout, >0 if success.
        if ((ret = select_with_seconds(server->fd, 5)) <= 0)
            continue;

        client.fd = accept(server->fd, (struct sockaddr*) &client.addr, &alen);
        if (client.fd == -1) {
            continue;
        }

        client_daemon(&client); // This shouldn't block. (fork or thread)
    }

    shut(server);

    return dead;
}

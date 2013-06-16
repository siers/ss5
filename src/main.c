#include "everything.h"

extern int errno;
extern char* optarg;

int sock;
int dead    = 0;
int loglvl  = -1;

void
interrupt(int sig)
{
    if (dead == sig) {
        exit(1);
    }
    dead = sig;
}

int main(int argc, char** argv)
{
    int opt, port = PORT;
    struct server s;

    while ((opt = getopt(argc, argv, "vp:")) != -1) {
        switch (opt)
        {
            case 'v':
                printf(VERSION);
                exit(1);
            case 'p':
                port = atoi(optarg);
                break;
            default:
                assert_fatal(0, "Usage: %s [-v] [-p <port>]\n", argv[0]);
        }
    }

    signal(SIGINT, interrupt);

    return server(&s, port);
}

#ifdef SOCK_VERBOSE
#include <arpa/inet.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include "notify.h"
#include "defs.h"

// error_table[talk(..)-1]
static char* error_table[] = {
    "Auth error",
    "Insufficient input from socket #1 || Wrong version.",
    "No acceptable methods",
    "Insufficient input from socket #2",
    "Insufficient input from socket #3(Port in request)",
    "Insufficient input from socket #4",
    "Insufficient input from socket #5",
    "Insufficient input from socket #6",
    "You shouldn't see this - reserved for IPv6",
    "Failed to establish connection",
};

char ipstr[255], timestr[255], ipstr2[255];
char nullstr[] = "0.0.0.0"; // for debug
time_t t;
struct tm* tmp;
// So that I wouldn't have to recreate them every time I call a function.

void create_timestr()
{
    t = time(NULL);
    tmp = localtime(&t);
    strftime(timestr, 255, timeformat, tmp);
}

void notify_making_connection(const s_client* client)
{
    char* ptr; // Store ptr to string to print as the target.
    switch (client->atyp)
    {
        case 1: /* IPv4 */
            inet_ntop(AF_INET, client->daddr, ipstr, 255);
            ptr = ipstr;
            break;
        case 3: /* Domain name */
            ptr = (char*) client->daddr;
        default:
            ptr = NULL;
            return;
    }
    create_timestr();
    inet_ntop(AF_INET, &client->addr.sin_addr.s_addr, ipstr2, 255);
    if (ptr == NULL) { // a.k.a. unknown atyp
        printf("[%s] %s requested connection with weird ATYP: %i.\n",
                timestr, ipstr2, client->atyp);
    }
    printf("[%s] %s requested connection to %s:%i. (ATYP: %i)\n",
            timestr, ipstr2, ptr, client->dport, client->atyp);
}

void notify_custom(void* in, char* text)
{
    create_timestr();
    if (in) // In case I pass NULL when debugging.
        inet_ntop(AF_INET, in, ipstr, 255);
    char tmp[255] = "[%s] %s";
    strncat(tmp, text, 255);
    printf(tmp, timestr, ipstr);
    printf("\n");
}

void notify_connected(void* in)
{
    create_timestr();
    inet_ntop(AF_INET, in, ipstr, 255);
    printf("[%s] %s connected.\n", timestr, ipstr);
}

void notify_disconnected(void* in, int ret, int errno_)
{
    create_timestr();
    inet_ntop(AF_INET, in, ipstr, 255);
    if (ret)
        printf("[%s] %s disconnected. (%s;#%i: %s)\n", timestr, ipstr,
                error_table[ret-1], errno_, strerror(errno_));
    else
        printf("[%s] %s disconnected. (Success)\n", timestr, ipstr);
}

void notify_port_opened(uint16_t port)
{
    create_timestr();
    printf("[%s] Socket bound to port %i.\n", timestr, port);
}

void notify_port_closed()
{
    create_timestr();
    printf("[%s] Socket closed.\n", timestr);
}

int complain(int n)
{
    create_timestr();
    printf("[%s] Socket error: %s.\n", timestr, strerror(n));
    return 1;
}
#endif

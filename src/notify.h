#ifndef GUARD_SRC_NOTIFY_H_
#define GUARD_SRC_NOTIFY_H_
#ifdef SOCK_VERBOSE

#include "defs.h"
#define timeformat "%H:%M:%S|%d.%m.%y."

void notify_making_connection(const s_client*);
void notify_connected(void*);
void notify_custom(void*, char*);
void notify_disconnected(void* in, int ret, int errno_);
void notify_port_opened(uint16_t port);
void notify_port_closed();
int complain(int n);

#endif
#endif /* GUARD_SRC_NOTIFY_H_ */

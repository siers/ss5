#ifdef SOCK_VERBOSE
#define timeformat[] "%H:%M:%S|%d.%m.%y.";
void notify_making_connection(void* in, char atyp, void* src);
void notify_connected(void*);
void notify_custom(void*, char*);
void notify_disconnected(void* in, int ret, int errno_);
void notify_port_opened(uint16_t port);
void notify_port_closed();
int complain(int n);
#endif

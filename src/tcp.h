#ifndef __TCP_H__
#define __TCP_H__

#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#define TCP_OK 0
#define TCP_ERR -1

#define TCP_CONN_BACKLOG 1024

int tcp_create_listener(char *ip, int port, int nonblock);
int tcp_accept(int sd, char *remote_ip, size_t remote_iplen, int *remote_port,
		int nonblock, int *tryagain);
ssize_t tcp_read(int sd, char *buf, size_t count, int *tryagain);
ssize_t tcp_write(int sd, const char *buf, size_t count, int *tryagain);
int tcp_connect(char *ipaddr, int port, int nonblock);
int tcp_close(int sd);

int tcp_set_nonblocking(int sd);
int tcp_set_reuseaddr(int sd);
int tcp_set_keepalive(int sd, int interval);
int tcp_set_nodelay(int sd, int flag);
int tcp_set_writebuffersize(int sd, int size);


#endif

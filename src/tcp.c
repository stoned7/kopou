#include <stdio.h>
#include "tcp.h"

int tcp_create_listener(char* ip, int port, int nonblock)
{
	int sd, r;
	socklen_t len;

	sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd == TCP_ERR)
		return sd;

	r = tcp_set_reuseaddr(sd);
	if (r == TCP_ERR)
		goto err;

	if (nonblock) {
		r = tcp_set_nonblocking(sd);
		if (r == TCP_ERR)
			goto err;
	}

	struct sockaddr_in saddr;
	len = sizeof(saddr);
	memset(&saddr, 0, len);
	saddr.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &saddr.sin_addr);
	saddr.sin_port = htons(port);

	r = bind(sd, (struct sockaddr*)&saddr, len);
	if (r == TCP_ERR)
		goto err;

	r = listen(sd, TCP_CONN_BACKLOG);
	if (r == TCP_ERR)
		goto err;

	return sd;
err:
	close(sd);
	return TCP_ERR;
}

int tcp_accept(int sd, char *remote_ip, size_t remote_iplen,
		int *remote_port, int nonblock, int *tryagain)
{
	int rd, r;
	struct sockaddr_in raddr;
	socklen_t len;

	*tryagain = 0;
	len = sizeof(raddr);
	memset(&raddr, 0, len);

	rd = accept(sd, (struct sockaddr*)&raddr, &len);

	if (rd == TCP_ERR) {
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			*tryagain = 1;
		return TCP_ERR;
	}

	if (nonblock) {
		r = tcp_set_nonblocking(rd);
		if (r == TCP_ERR) {
			close(rd);
			return TCP_ERR;
		}
	}

	if (remote_ip)
		inet_ntop(AF_INET, (void*)&(raddr.sin_addr),
				remote_ip, remote_iplen);
	if (remote_port)
		*remote_port = ntohs(raddr.sin_port);

	return rd;
}

int tcp_set_reuseaddr(int sd)
{
	int opt = 1;
	return setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

int tcp_set_keepalive(int sd, int interval)
{
	int r, val = 1;
	r = setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val));
	if (r == TCP_ERR)
		return TCP_ERR;

	val = interval;
	r = setsockopt(sd, IPPROTO_TCP, TCP_KEEPIDLE, &val, sizeof(val));
	if (r == TCP_ERR)
		return TCP_ERR;

	val = interval/3;
	if (val == 0)
		val = 1;
	r = setsockopt(sd, IPPROTO_TCP, TCP_KEEPINTVL, &val, sizeof(val));
	if (r = TCP_ERR)
		return TCP_ERR;

	val = 3;
	r = setsockopt(sd, IPPROTO_TCP, TCP_KEEPCNT, &val, sizeof(val));
	if (r == TCP_ERR)
		return TCP_ERR;

	return TCP_OK;
}

int tcp_set_nodelay(int sd, int flag)
{
	return setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
}

int tcp_set_writebuffersize(int sd, int size)
{
	return setsockopt(sd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
}

int tcp_set_nonblocking(int sd)
{
        return fcntl(sd, F_SETFL, fcntl(sd, F_GETFD, 0) | O_NONBLOCK);
}


ssize_t tcp_read(int sd, char *buf, size_t count, int *tryagain)
{
	ssize_t nread, tread = 0;
	*tryagain = 0;
	while (tread != count) {
		nread = read(sd, buf, count - tread);
		if (nread == 0)
			return TCP_ERR; /* not interested for dead client */
		if (nread == TCP_ERR) {
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
				*tryagain = 1;
				return tread;
			}
			return TCP_ERR;
		}
		tread += nread;
		buf += nread;
	}
	return tread;
}

ssize_t tcp_write(int sd, const char *buf, size_t count, int *tryagain)
{
	ssize_t nwritten, twritten = 0;
	*tryagain = 0;
	while (twritten != count) {
		nwritten = write(sd, buf, count - twritten);
		if (nwritten == 0)
			return twritten;
		if (nwritten == TCP_ERR) {
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
				*tryagain = 1;
				return twritten;
			}
			return TCP_ERR;
		}
		twritten += nwritten;
		buf += nwritten;
	}
	return twritten;
}

int tcp_connect(char *ipaddr, int port, int nonblock)
{
	int cd, r;
	struct sockaddr_in raddr;
	socklen_t len;

	cd = socket(AF_INET, SOCK_STREAM, 0);
	if (cd == TCP_ERR)
		return cd;

	r = tcp_set_reuseaddr(cd);
	if (r == TCP_ERR)
		goto err;

	if (nonblock) {
		r = tcp_set_nonblocking(cd);
		if (r == TCP_ERR)
			goto err;
	}

	len = sizeof(raddr);
	memset(&raddr, 0, len);
	raddr.sin_family = AF_INET;
	raddr.sin_port = htons(port);
	inet_pton(AF_INET, ipaddr, &raddr.sin_addr);

	r = connect(cd, (struct sockaddr*)&raddr, len);
	if (r == TCP_ERR) {
		if (errno == EINPROGRESS && nonblock)
			return cd;
		else
			goto err;
	}

	return cd;
err:
	close(cd);
	return TCP_ERR;
}

int tcp_close(int sd)
{
	return close(sd);
}

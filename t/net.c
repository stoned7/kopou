
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>

#include "../src/tcp.h"

int main(int argc, char **argv)
{
	int server;
	int efd;
	struct epoll_event ev, events[10];


	server = tcp_create_listener(7878, 1024, 0);
	if (server == TCP_ERR) {
		printf("server creation fail\n");
		return EXIT_FAILURE;
	}

	efd = epoll_create(10);
	if (efd == -1) {
		perror("epoll_create");
		exit(EXIT_FAILURE);
	}
	ev.events = EPOLLIN;
	ev.data.fd = server;
	epoll_ctl(efd, EPOLL_CTL_ADD, server, &ev);
	printf("server listening ...\n");

	int etfd;
	int n;
	long ecounter = 0;
	while(1) {
		etfd = epoll_wait(efd, events, 10, -1);
		ecounter++;
		if (etfd == -1) {
			printf("ecounter:%ld\n", ecounter);
			perror("epoll_wait");
			exit(EXIT_FAILURE);
		}

		for (n = 0; n < etfd; ++n) {
			if ((events[n].events & EPOLLERR) ||
				(events[n].events & EPOLLHUP))
				 //&& ((!events[n].events & EPOLLIN)
				 //|| (!events[n].events & EPOLLOUT))
				{
					printf("ecounter:%ld\n", ecounter);
					perror("epoll error");
					close (events[n].data.fd);
					continue;
				}

			if (events[n].data.fd == server) {
				printf("ecounter:%ld\n", ecounter);
				char ip[64];
				int port;
				int tryagain;
				int cli = tcp_accept(server, ip, 64, &port, 1,
						&tryagain);
				if (cli == TCP_ERR) {
					printf("tryagain: %d\n", tryagain);
					printf("accept(): %s\n", strerror(errno));
					continue;
				}
				//tcp_set_keepalive(cli, 10);
				printf("-----> Client: %s:%d\n", ip, port);
				ev.events = EPOLLIN;
				ev.data.fd = cli;
				epoll_ctl(efd, EPOLL_CTL_ADD, cli, &ev);
			}
			else {
				if (events[n].events & EPOLLIN) {
					printf("ecounter:%ld\n", ecounter);
					printf("%d ready for read\n", events[n].data.fd);

					int cli = events[n].data.fd;
					int tryagain;
					char buf[16];
					int isreadonce = 0;
					ssize_t nread;
					memset(buf, '\0', 16);
					nread = tcp_read(cli, buf, 16, &tryagain);
					if (nread > 0) {
						printf("Read: %zd:%d:%s\n", nread,tryagain, buf);
						ev.events = events[n].events;
						ev.events |= EPOLLOUT;
						ev.data.fd = events[n].data.fd;
						int rct = epoll_ctl(efd, EPOLL_CTL_MOD, ev.data.fd, &ev);
						if (rct == -1)
							perror("r:epoll_ctl_mod");
					} else {
						if (tryagain) {
							printf("completed reading: %zd, tryagain:%d\n",
							nread, tryagain);
							printf("read(): %s\n", strerror(errno));
							break;
						} else {
							printf("client down: %zd\n", nread);
							printf("read(): %s\n", strerror(errno));
							tcp_close(cli);
							break;
						}
					}
				}

				if (events[n].events & EPOLLOUT) {
					printf("ecounter:%ld\n", ecounter);
					printf("%d ready for write\n",events[n].data.fd);
					int tryagain;
					char *buf = "helloresponse";
					ssize_t written = tcp_write(events[n].data.fd,  buf, 13, &tryagain);
					printf("written: %zd, tryagain: %d\n", written, tryagain);
					ev.events = events[n].events;
					ev.events = EPOLLIN;
					ev.data.fd = events[n].data.fd;
					int rct = epoll_ctl(efd, EPOLL_CTL_MOD, ev.data.fd, &ev);
					if (rct == -1)
						perror("w:epoll_ctl_mod");

				}
			}
		}
	}

	return EXIT_SUCCESS;
}


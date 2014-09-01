#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/tcp.h"

int main()
{
	char *ip = "127.0.0.1";
	int port = 7878;

	int server;
	server = tcp_connect(ip, port, 0);
	if (server == TCP_ERR) {
		printf("connect(): %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	printf("connected to server ...\n");

	int tryagain;
	char *buf = "~3\r\n#4$SINS\r\n#8$sibsagar\r\n#21$Hey, I am from Assam.\r\n";
	size_t len = strlen(buf);
	ssize_t written = tcp_write(server,  buf, len, &tryagain);
	printf("written: %zd, tryagain: %d\n", written, tryagain);

	buf = "~2\r\n#4$SGET\r\n#10$abcdefghij\r\n";
	len = strlen(buf);
	written = tcp_write(server,  buf, len, &tryagain);
	printf("written: %zd, tryagain: %d\n", written, tryagain);

	while(1) {
	}

	printf("exiting client\n");
	return EXIT_SUCCESS;
}

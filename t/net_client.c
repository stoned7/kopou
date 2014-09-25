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

	char *buf = "GET /package/detail/37?abc=xyz&123 HTTP/1.1\r\nHost: dev-packagedeployment.tavisca.com\r\nConnection:keep-alive\r\nCache-Control: max-age=0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\nUser-Agent: Mozilla/5.0 (Windows NT 6.2; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/37.0.2062.103 Safari/537.36\r\nAccept-Encoding: gzip,deflate,sdch\r\nAccept-Language: en-US,en;q=0.8\r\nCookie: ASP.NET_SessionId=t3lve43o5fzii2xwji53anuc\r\nContent-Length: 67890\r\nContent-Type: application/json, abcd\r\nKeep-Alive: 300\r\n\r\n";
	size_t len = strlen(buf);
	ssize_t written = tcp_write(server,  buf, len, &tryagain);
	printf("written: %zd, tryagain: %d\n", written, tryagain);

	/*
	buf = "~2\r\n#4$SGET\r\n#10$abcdefghij\r\n";
	len = strlen(buf);
	written = tcp_write(server,  buf, len, &tryagain);
	printf("written: %zd, tryagain: %d\n", written, tryagain);
	*/

	while(1) {
	}

	printf("exiting client\n");
	return EXIT_SUCCESS;
}

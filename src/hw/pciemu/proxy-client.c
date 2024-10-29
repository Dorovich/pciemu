/* proxy-client.c - External access to a running PCIEMU device
 *
 * Author: Dorovich (David Cañadas López)
 *
 */

#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "proxy.h"

#define CHECK(cond, str) { \
	if (cond) { \
		perror(str); \
		exit(EXIT_FAILURE); \
	} \
}

int main (int argc, char *argv[])
{
	int srv, loop, ret, msg_size;
	char *msg;
	struct sockaddr_in addr;
	struct hostent *h;

	h = gethostbyname(PCIEMU_PROXY_HOST);
	CHECK(h == NULL, "gethostname");
	
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PCIEMU_PROXY_PORT);
	addr.sin_addr.s_addr = *(in_addr_t *)h->h_addr_list[0];

	srv = socket(AF_INET, SOCK_STREAM, 0);
	CHECK(srv < 0, "socket");

	ret = connect(srv, (struct sockaddr *)&addr, sizeof(addr));
	CHECK(ret < 0, "connect");

	msg_size = 1024;
	msg = (char *)malloc(msg_size * sizeof(char));
	loop = 1;
	while (loop) {
		ret = getline(&msg, &n, stdin);
		CHECK(ret < 0, "getline");
		ret = send(srv, msg, strlen(msg), 0);
		CHECK(ret < 0, "send");
		/* TODO acabar el bucle */
	}

	return 0;

}

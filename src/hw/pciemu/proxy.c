/* proxy.c - External access to PCIEMU device memory
 *
 * Author: Dorovich (David Cañadas López)
 *
 */

#include "pciemu.h"
#include "proxy.h"
#include "qemu/main-loop.h"
#include "sysemu/sysemu.h"
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* -----------------------------------------------------------------------------
 *  Private
 * -----------------------------------------------------------------------------
 */

void qmp_system_reset(void *reason); /* forward declaration */

static QEMUBH *pciemu_proxy_reset_bh;

static void pciemu_proxy_bh_handler (void *opaque)
{
	qmp_system_reset(NULL); /* ver qemu/ui/gtk.c, línea 1313 */
}

static void *pciemu_proxy_server_routine (void *opaque)
{
	int con, loop, ret;
	size_t msg_size;
	socklen_t len;
	char *msg;
	struct sockaddr_in src;

	PCIEMUDevice *dev = opaque;
	len = sizeof(src);
	msg_size = PCIEMU_PROXY_BUFF;
	msg = (char *)malloc(msg_size*sizeof(char));

	while (1) {
		con = accept(dev->proxy.sockd, (struct sockaddr *)&src, &len);
		if (con < 0) {
			perror("accept");
			goto server_accept_err;
		}

		/* char msg[1024]; */
		bzero(msg, PCIEMU_PROXY_BUFF);
		loop = 1;
		while (loop) {
			ret = recv(con, msg, msg_size*sizeof(char), 0);
			if (ret < 0) {
				perror("recv");
				goto server_msg_err;
			}
			/* check command */
			if (!strncmp(msg, "ping\n", ret)) {
				ret = sprintf(msg, "pong\n");
			}
			else if (!strncmp(msg, "reset\n", ret)) {
				qemu_bh_schedule(pciemu_proxy_reset_bh);
				ret = sprintf(msg, "resetting\n");
			}
			else if (!strncmp(msg, "quit\n", ret)) {
				loop = 0;
				ret = sprintf(msg, "bye\n");
			}
			else {
				ret = sprintf(msg, "?\n");
			}
			ret = send(con, msg, ret, 0);
			if (ret < 0) {
				perror("send");
				goto server_msg_err;
			}
		}
		
		close(con);
	}	

server_msg_err:
	if (msg)
		free(msg);
	if (con)
		close(con);
	if (dev->proxy.sockd)
		close(dev->proxy.sockd);
server_accept_err:
	pthread_exit(NULL);
}

/* -----------------------------------------------------------------------------
 *  Public
 * -----------------------------------------------------------------------------
 */

void pciemu_proxy_reset(PCIEMUDevice *dev)
{
	return;
}

void pciemu_proxy_init(PCIEMUDevice *dev, Error **errp)
{
	int ret;
	struct hostent *h;

	pciemu_proxy_reset_bh = qemu_bh_new(pciemu_proxy_bh_handler, NULL);

	/* Inicializar socket */

	h = gethostbyname(PCIEMU_PROXY_HOST);
	if (h == NULL) {
		herror("gethostbyname");
		return;
	}

	dev->proxy.sockd = socket(AF_INET, SOCK_STREAM, 0);
	if (dev->proxy.sockd < 0) {
		perror("socket");
		return;
	}

	bzero(&dev->proxy.addr, sizeof(dev->proxy.addr));
	dev->proxy.addr.sin_family = AF_INET;
	dev->proxy.addr.sin_port = htons(PCIEMU_PROXY_PORT);
	dev->proxy.addr.sin_addr.s_addr = *(in_addr_t *)h->h_addr_list[0];

	ret = bind(dev->proxy.sockd, (struct sockaddr *)&dev->proxy.addr,
			sizeof(dev->proxy.addr));
	if (ret < 0) {
		perror("bind");
		return;
	}

	ret = listen(dev->proxy.sockd, PCIEMU_PROXY_MAXQ);
	if (ret < 0) {
		perror("listen");
		return;
	}

	/* Inicializar thread */

	ret = pthread_create(&dev->proxy.proxy_thread, NULL,
			pciemu_proxy_server_routine, dev);
	if (ret < 0) {
		perror("pthread_create");
		return;
	}
	
	ret = pthread_detach(dev->proxy.proxy_thread);
	if (ret < 0) {
		perror("pthread_detach");
		return;
	}
}

void pciemu_proxy_fini(PCIEMUDevice *dev)
{
	return;
}

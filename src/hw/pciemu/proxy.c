/* proxy.c - External access to PCIEMU device memory
 *
 * Author: Dorovich (David Cañadas López)
 *
 */

/* #include <fcntl.h> */
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
/* #include <sys/socket.h> */
#include <unistd.h>
#include "proxy.h"
#include "pciemu.h"
#include "sysemu/sysemu.h"
#include "qemu/main-loop.h"

/* -----------------------------------------------------------------------------
 *  Private
 * -----------------------------------------------------------------------------
 */

static QEMUBH *pciemu_proxy_reset_bh;

static void pciemu_proxy_bh_handler (void *opaque)
{
	qmp_system_reset(NULL); /* ver qemu/ui/gtk.c, línea 1313 */
}

void *pciemu_proxy_socket_routine (void *opaque)
{
	int con, len, loop, ret;
	struct sockaddr_in src;

	PCIEMUDevice *dev = opaque;

	len = sizeof(src);
	while (1) {
		con = accept(dev->proxy.sockd, (struct sockaddr *)&src, &len);
		if (con < 0) {
			perror("accept");
			goto socket_routine_accept_err;
		}

		char msg[1024];
		loop = 1;
		while (loop) {
			ret = recv(con, msg, sizeof(msg), 0);
			if (ret < 0) {
				perror("recv");
				goto socket_routine_msg_err;
			}
			if (!strcmp(msg, "reset")) {
				qemu_bh_schedule(pciemu_proxy_reset_bh);
			}
			else if (!strcmp(msg, "quit")) {
				loop = 0;
			}
			else {
				/* echo message */
				ret = send(con, msg, strlen(msg), 0);
				if (ret < 0) {
					perror("send");
					goto socket_routine_msg_err;
				}
			}
		}
		
		close(con);
	}	

socket_routine_msg_err:
	if (con) close(con);
socket_routine_accept_err:
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

	pciemu_proxy_reset_bh = qemu_bh_new(pciemu_proxy_bh_handler, NULL);

	ret = pthread_create(&dev->proxy.proxy_thread, NULL,
			pciemu_proxy_socket_routine, dev);
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

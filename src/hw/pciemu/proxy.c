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

int pciemu_proxy_handle_client(int con, const char *cmd_buf)
{
	int ret, loop;

	bzero(cmd_buf, PCIEMU_PROXY_BUFF);
	loop = 1;
	while (loop) {
		ret = recv(con, cmd_buf, PCIEMU_PROXY_BUFF*sizeof(char), 0);
		if (ret < 0) {
			perror("recv");
			goto server_msg_err;
		}
		if (!strncmp(cmd_buf, "ping\n", ret)) {
			ret = sprintf(cmd_buf, "pong\n");
		}
		else if (!strncmp(cmd_buf, "reset\n", ret)) {
			qemu_bh_schedule(pciemu_proxy_reset_bh);
			ret = sprintf(cmd_buf, "resetting\n");
		}
		else if (!strncmp(cmd_buf, "quit\n", ret)) {
			loop = 0;
			ret = sprintf(cmd_buf, "bye\n");
		}
		else {
			ret = sprintf(cmd_buf, "?\n");
		}
		ret = send(con, cmd_buf, ret, 0);
		if (ret < 0) {
			perror("send");
			goto server_msg_err;
		}
	}
	close(con);

	return 0;
server_msg_err:
	return -1;
}

static void *pciemu_proxy_server_routine (void *opaque)
{
	int con, ret;
	socklen_t len;
	char *msg;
	struct sockaddr_in src;

	PCIEMUDevice *dev = opaque;
	len = sizeof(src);
	msg = malloc(PCIEMU_PROXY_BUFF*sizeof(char));

	while (1) {
		con = accept(dev->proxy.sockd, (struct sockaddr *)&src, &len);
		if (con < 0) {
			perror("accept");
			goto server_accept_err;
		}
		ret = pciemu_proxy_handle_client(con, msg);
		if (ret < 0)
			goto server_handle_err;
	}

server_handle_err:
	if (msg)
		free(msg);
	if (con)
		close(con);
	if (dev->proxy.sockd)
		close(dev->proxy.sockd);
server_accept_err:
	pthread_exit(NULL);
}

static void *pciemu_proxy_client_routine (void *opaque)
{
	int ret;
	size_t msg_size;
	char *msg;

	PCIEMUDevice *dev = opaque;
	msg_size = PCIEMU_PROXY_BUFF*sizeof(char);
	msg = malloc(msg_size);

	ret = connect(dev->proxy.sockd, (struct sockaddr *)&dev->proxy.addr,
		sizeof(dev->proxy.addr));
	if (ret < 0) {
		perror("connect");
		goto client_connect_err;
	}

	printf("Connected to PCIEMU proxy!\n");

	/* ping - pong */

	ret = sprintf(msg, "ping\n");
	ret = send(dev->proxy.sockd, msg, ret, 0);
	if (ret < 0) {
		perror("send");
		goto client_handle_err;
	}

	ret = recv(dev->proxy.sockd, msg, msg_size, 0);
	if (ret < 0) {
		perror("recv");
		goto client_handle_err;
	}

	ret = write(1, msg, ret);
	if (ret < 0) {
		perror("write");
		goto client_write_err;

	/* quit - bye */

	ret = sprintf(msg, "quit\n");
	ret = send(dev->proxy.sockd, msg, ret, 0);
	if (ret < 0) {
		perror("send");
		goto client_handle_err;
	}

	ret = recv(dev->proxy.sockd, msg, msg_size, 0);
	if (ret < 0) {
		perror("recv");
		goto client_handle_err;
	}

	ret = write(1, msg, ret);
	if (ret < 0) {
		perror("write");
		goto client_write_err;
	}
}

client_handle_err:
client_write_err:
	if (msg)
		free(msg);
	if (dev->proxy.sockd)
		close(dev->proxy.sockd);
client_connect_err:
	pthread_exit(NULL);
}

void pciemu_proxy_init_server(PCIEMUDevice *dev)
{
	int ret;

	/* Configurar socket */

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

void pciemu_proxy_init_client(PCIEMUDevice *dev)
{
	int ret;

	/* Configurar socket (nada a hacer) */

	/* Inicializar thread */

	ret = pthread_create(&dev->proxy.proxy_thread, NULL,
			pciemu_proxy_client_routine, dev);
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


/* -----------------------------------------------------------------------------
 *  Public
 * -----------------------------------------------------------------------------
 */

bool pciemu_proxy_get_mode(Object *obj, Error **errp)
{
	PCIEMUDevice *dev = PCIEMU(obj);
	return dev->proxy.server_mode;
}

void pciemu_proxy_set_mode(Object *obj, bool mode, Error **errp)
{
	PCIEMUDevice *dev = PCIEMU(obj);
	dev->proxy.server_mode = mode;
}

void pciemu_proxy_reset(PCIEMUDevice *dev)
{
	return;
}

void pciemu_proxy_init(PCIEMUDevice *dev, Error **errp)
{
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

	/*
	 * Hay un nuevo booleano, "server_mode", en PCIEMUDevice.
	 * Se puede pasar su valor inicial por la línea de comandos
	 * con "-device pciemu,server_mode=x" (ver pciemu_instance_init,
	 * en pciemu.c)
	 */

	if (dev->proxy.server_mode)
		pciemu_proxy_init_server(dev);
	else
		pciemu_proxy_init_client(dev);

}

void pciemu_proxy_fini(PCIEMUDevice *dev)
{
	return;
}

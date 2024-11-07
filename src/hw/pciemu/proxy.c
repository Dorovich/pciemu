/* proxy.c - External access to PCIEMU device memory
 *
 * Author: Dorovich (David Cañadas López)
 *
 */

#include "pciemu.h"
#include "proxy.h"
#include "qemu/main-loop.h"
#include "sysemu/sysemu.h"
#include "irq.h"
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <unistd.h>

/* -----------------------------------------------------------------------------
 *  Private
 * -----------------------------------------------------------------------------
 */

void qmp_system_reset(void *reason); /* forward declaration */

static QEMUBH *pciemu_reset_bh, *pciemu_sync_bh;

static void pciemu_proxy_reset_bh_handler (void *opaque)
{
	qmp_system_reset(NULL); /* ver qemu/ui/gtk.c, línea 1313 */
}

static void pciemu_proxy_sync_bh_handler (void *opaque)
{
	PCIEMUDevice *dev = opaque;

	memcpy(dev->dma.buff, dev->proxy.tmp_buff, sizeof(dev->dma.buff));
	free(dev->proxy.tmp_buff);
}

int pciemu_proxy_recv_buffer(PCIEMUDevice *dev, int client)
{
	int ret;
	size_t size;

	size = sizeof(dev->dma.buff);
	dev->proxy.tmp_buff = malloc(size);
	ret = recv(client, dev->proxy.tmp_buff, size, 0);

	return ret;
}

int pciemu_proxy_send_buffer(PCIEMUDevice *dev, int client)
{
	return send(client, dev->dma.buff, sizeof(dev->dma.buff), 0);
}

int pciemu_proxy_request(int con, ProxyRequest req)
{
	int ret;

	ret = 0;
	if (req != PCIEMU_REQ_NONE)
		ret = send(con, &req, sizeof(req), 0);

	return ret;
}

int pciemu_proxy_wait_reply(int con, ProxyRequest rep)
{
	int ret;
	ProxyRequest req;

	ret = recv(con, &req, sizeof(req), 0);
	if (req != rep)
		ret = -1;

	return ret;
}

int pciemu_proxy_handle_req(PCIEMUDevice *dev, int con, ProxyRequest req)
{
	int ret, ret_handle;

	ret_handle = PCIEMU_HANDLE_SUCCESS;
	switch (req) {
	case PCIEMU_REQ_PING:
		ret = pciemu_proxy_request(con, PCIEMU_REQ_PONG);
		if (ret < 0)
			ret_handle = PCIEMU_HANDLE_FAILURE;
		break;
	case PCIEMU_REQ_RESET:
		qemu_bh_schedule(pciemu_reset_bh);
		ret = pciemu_proxy_request(con, PCIEMU_REQ_ACK);
		if (ret < 0)
			ret_handle = PCIEMU_HANDLE_FAILURE;
		break;
	case PCIEMU_REQ_QUIT:
		ret = pciemu_proxy_request(con, PCIEMU_REQ_ACK);
		if (ret < 0)
			ret_handle = PCIEMU_HANDLE_FAILURE;
		else
			ret_handle = PCIEMU_HANDLE_FINISH;
		break;
	case PCIEMU_REQ_INTA:
		pciemu_irq_raise(dev, 0);
		ret = pciemu_proxy_request(con, PCIEMU_REQ_ACK);
		if (ret < 0)
			ret_handle = PCIEMU_HANDLE_FAILURE;
		break;
	case PCIEMU_REQ_SYNC:
		ret = pciemu_proxy_recv_buffer(dev, con);
		if (ret < 0) {
			ret_handle = PCIEMU_HANDLE_FAILURE;
			break;
		}
		qemu_bh_schedule(pciemu_sync_bh);
		ret = pciemu_proxy_request(con, PCIEMU_REQ_ACK);
		if (ret < 0)
			ret_handle = PCIEMU_HANDLE_FAILURE;
		break;
	case PCIEMU_REQ_SYNCME:
		ret = pciemu_proxy_request(con, PCIEMU_REQ_SYNC);
		if (ret < 0) {
			ret_handle = PCIEMU_HANDLE_FAILURE;
			break;
		}
		ret = pciemu_proxy_send_buffer(dev, con);
		if (ret < 0)
			ret_handle = PCIEMU_HANDLE_FAILURE;
		break;
	case PCIEMU_REQ_ACK:
	case PCIEMU_REQ_PONG:
	case PCIEMU_REQ_NONE:
		break;
	default:
		ret_handle = PCIEMU_HANDLE_FAILURE;
		break;
	}

	return ret_handle;
}


int pciemu_proxy_issue_req(PCIEMUDevice *dev, int con, ProxyRequest req)
{
	int ret, ret_handle;

	ret_handle = PCIEMU_HANDLE_SUCCESS;
	switch (req) {
	case PCIEMU_REQ_PING:
		ret = pciemu_proxy_request(con, req);
		if (ret < 0) {
			ret_handle = PCIEMU_HANDLE_FAILURE;
			break;
		}
		ret = pciemu_proxy_wait_reply(con, PCIEMU_REQ_PONG);
		if (ret < 0)
			ret_handle = PCIEMU_HANDLE_FAILURE;
		break;
	case PCIEMU_REQ_RESET:
	case PCIEMU_REQ_INTA:
		ret = pciemu_proxy_request(con, req);
		if (ret < 0) {
			ret_handle = PCIEMU_HANDLE_FAILURE;
			break;
		}
		ret = pciemu_proxy_wait_reply(con, PCIEMU_REQ_ACK);
		if (ret < 0)
			ret_handle = PCIEMU_HANDLE_FAILURE;
		break;
	case PCIEMU_REQ_QUIT:
		ret = pciemu_proxy_request(con, req);
		if (ret < 0) {
			ret_handle = PCIEMU_HANDLE_FAILURE;
			break;
		}
		ret = pciemu_proxy_wait_reply(con, PCIEMU_REQ_ACK);
		if (ret < 0)
			ret_handle = PCIEMU_HANDLE_FAILURE;
		else
			ret_handle = PCIEMU_HANDLE_FINISH;
		break;
	case PCIEMU_REQ_SYNC:
		ret = pciemu_proxy_request(con, req);
		if (ret < 0) {
			ret_handle = PCIEMU_HANDLE_FAILURE;
			break;
		}
		ret = pciemu_proxy_send_buffer(dev, con);
		if (ret < 0) {
			ret_handle = PCIEMU_HANDLE_FAILURE;
			break;
		}
		ret = pciemu_proxy_wait_reply(con, PCIEMU_REQ_ACK);
		if (ret < 0)
			ret_handle = PCIEMU_HANDLE_FAILURE;
		break;
	case PCIEMU_REQ_SYNCME:
		ret = pciemu_proxy_request(con, req);
		if (ret < 0)
			ret_handle = PCIEMU_HANDLE_FAILURE;
		break;
	case PCIEMU_REQ_ACK:
	case PCIEMU_REQ_PONG:
	case PCIEMU_REQ_NONE:
		break;
	default:
		ret_handle = PCIEMU_HANDLE_FAILURE;
		break;
	}

	return ret_handle;
}

int pciemu_proxy_handle_connection(PCIEMUDevice *dev, int con)
{
	int ret, rret;
	ProxyRequest req;
	fd_set fds;
	struct timeval timeout;

	/* Comprobar peticiones */

	FD_ZERO(&fds);

	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	ret = PCIEMU_HANDLE_SUCCESS;
	do {
		FD_SET(con, &fds);
		rret = select(con+1, &fds, NULL, NULL, &timeout);
		if (rret && FD_ISSET(con, &fds)) {
			recv(con, &req, sizeof(req), MSG_WAITALL);
			printf("Debug: Found a request on socket (%X)\n", req);
			ret = pciemu_proxy_handle_req(dev, con, req);
		}
		else if (!TAILQ_EMPTY(&dev->proxy.req_head)) {
			req = pciemu_proxy_pop_req(dev);
			printf("Debug: Popped a request on queue (%X)\n", req);
			ret = pciemu_proxy_issue_req(dev, con, req);
		}
	} while (ret == PCIEMU_HANDLE_SUCCESS);

	printf("Debug: Closing connection\n");

	if (con)
		close(con);

	return ret;
}

static void *pciemu_proxy_server_routine (void *opaque)
{
	int con, ret;
	socklen_t len;
	struct sockaddr_in src;

	PCIEMUDevice *dev = opaque;
	len = sizeof(src);

	do {
		con = accept(dev->proxy.sockd, (struct sockaddr *)&src, &len);
		if (con < 0) {
			perror("accept");
			goto server_accept_err;
		}
		ret = pciemu_proxy_handle_connection(dev, con);
	} while (ret != PCIEMU_HANDLE_FAILURE);

	if (con)
		close(con);
	if (dev->proxy.sockd)
		close(dev->proxy.sockd);
server_accept_err:
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

static void *pciemu_proxy_client_routine (void *opaque)
{
	int ret;

	PCIEMUDevice *dev = opaque;

	ret = connect(dev->proxy.sockd, (struct sockaddr *)&dev->proxy.addr,
		sizeof(dev->proxy.addr));
	if (ret < 0) {
		perror("connect");
		goto client_connect_err;
	}
	printf("Debug: Connected to PCIEMU proxy\n");
	pciemu_proxy_handle_connection(dev, dev->proxy.sockd);

client_connect_err:
	pthread_exit(NULL);
}

void pciemu_proxy_init_client(PCIEMUDevice *dev)
{
	int ret;

	/* Peticiones de muestra */

	pciemu_proxy_push_req(dev, PCIEMU_REQ_PING);
	pciemu_proxy_push_req(dev, PCIEMU_REQ_PING);
	pciemu_proxy_push_req(dev, PCIEMU_REQ_PING);
	pciemu_proxy_push_req(dev, PCIEMU_REQ_QUIT);

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

int pciemu_proxy_push_req(PCIEMUDevice *dev, ProxyRequest req)
{
	struct pciemu_proxy_req_entry *entry;

	entry = malloc(sizeof(struct pciemu_proxy_req_entry));
	if (!entry)
		return EXIT_FAILURE;

	entry->req = req;
	TAILQ_INSERT_TAIL(&dev->proxy.req_head, entry, entries);
	return EXIT_SUCCESS;
}

ProxyRequest pciemu_proxy_pop_req(PCIEMUDevice *dev)
{
	struct pciemu_proxy_req_entry *entry;
	ProxyRequest req;

	entry = TAILQ_FIRST(&dev->proxy.req_head);
	if (!entry)
		return PCIEMU_REQ_NONE;

	req = entry->req;
	TAILQ_REMOVE(&dev->proxy.req_head, entry, entries);
	free(entry);

	return req;
}

void pciemu_proxy_reset(PCIEMUDevice *dev)
{
	return;
}

void pciemu_proxy_init(PCIEMUDevice *dev, Error **errp)
{
	struct hostent *h;

	pciemu_reset_bh = qemu_bh_new(pciemu_proxy_reset_bh_handler, NULL);
	pciemu_sync_bh = qemu_bh_new(pciemu_proxy_sync_bh_handler, NULL);

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

	TAILQ_INIT(&dev->proxy.req_head);

	/*
	 * Hay un nuevo booleano, "server_mode", en PCIEMUDevice.
	 * Se puede pasar su valor inicial por la línea de comandos
	 * con "-device pciemu,server_mode=[on|off]" (ver pciemu_instance_init,
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

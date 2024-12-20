/* proxy.c - External access to PCIEMU device memory
 *
 * Author: Dorovich (David Cañadas López)
 *
 */

#include "irq.h"
#include "dma.h"
#include "pciemu.h"
#include "pciemu_hw.h"
#include "proxy.h"
#include "qemu/main-loop.h"
#include "sysemu/sysemu.h"
#include <linux/futex.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>

/* -----------------------------------------------------------------------------
 *  Private
 * -----------------------------------------------------------------------------
 */

void qmp_system_reset(void *reason); /* forward declaration */

static QEMUBH *pciemu_reset_bh, *pciemu_sync_bh;

static void pciemu_proxy_reset_bh_handler(void *opaque)
{
	qmp_system_reset(NULL); /* ver qemu/ui/gtk.c, línea 1313 */
}

static void pciemu_proxy_sync_bh_handler(void *opaque)
{
	PCIEMUDevice *dev = opaque;
	DMAEngine *dma = &dev->dma;

	if (!dev->proxy.tmp_conf || !dev->proxy.tmp_buff)
		return;

	memcpy(&dma->config, dev->proxy.tmp_conf, sizeof(dma->config));
	free(dev->proxy.tmp_conf);

	memcpy(dma->buff, dev->proxy.tmp_buff, dma->config.txdesc.len);
	free(dev->proxy.tmp_buff);
}

/* No GLIBC definition for futex(2) */
static int futex(uint32_t *uaddr, int futex_op, uint32_t val,
		const struct timespec *timeout, uint32_t *uaddr2, uint32_t val3)
{
	return syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
}

void pciemu_proxy_ftx_wait(uint32_t *ftx)
{
	int ret;
	const uint32_t one = 1;

	if (*ftx == 0 && atomic_load(ftx) == 0)
		return;

	while(1) {
		if (atomic_compare_exchange_strong(ftx, &one, 0))
			break;

		ret = futex(ftx, FUTEX_WAIT, 0, NULL, NULL, 0);
		if (ret < 0 && ret != EAGAIN) {
			perror("futex wait");
		}
	}
}

void pciemu_proxy_ftx_post(uint32_t *ftx)
{
	int ret;
	const uint32_t zero = 0;

	if (*ftx == 1 && atomic_load(ftx) == 1)
		return;

	if (atomic_compare_exchange_strong(ftx, &zero, 1)) {
		ret = futex(ftx, FUTEX_WAKE, 1, NULL, NULL, 0);
		if (ret < 0) {
			perror("futex wake");
		}
	}

}

int pciemu_proxy_issue_sync(PCIEMUDevice *dev, int con)
{
	int ret;
	dma_size_t len;
	DMAConfig *config;

	if (dev->dma.buff == NULL)
		return PCIEMU_HANDLE_FAILURE;

	len = dev->dma.config.txdesc.len;
	ret = send(con, &len, sizeof(len));
	if (ret < 0)
		return PCIEMU_HANDLE_FAILURE;

	ret = send(con, dev->dma.buff, sizeof(uint8_t)*len);
	if (ret < 0)
		return PCIEMU_HANDLE_FAILURE;

	ret = pciemu_proxy_wait_reply(con, PCIEMU_REQ_ACK);
	if (ret < 0)
		return PCIEMU_HANDLE_FAILURE;

	free(dev->dma.buff);
	dev->dma.buff = NULL;

	return PCIEMU_HANDLE_SUCCESS;
}

int pciemu_proxy_handle_sync(PCIEMUDevice *dev, int con)
{
	dma_size_t len;

	len = 0;
	ret = recv(con, &len, sizeof(len), 0);
	if (ret < 0)
		return PCIEMU_HANDLE_FAILURE;

	dev->dma.buff = malloc(sizeof(uint8_t)*len);
	ret = recv(con, dev->dma.buff, sizeof(uint8_t)*len, 0);
	if (ret < 0)
		return PCIEMU_HANDLE_FAILURE;

	/* qemu_bh_schedule(pciemu_sync_bh); */

	ret = pciemu_proxy_request(con, PCIEMU_REQ_ACK);
	if (ret < 0)
		return PCIEMU_HANDLE_FAILURE;

	return PCIEMU_HANDLE_SUCCESS;
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
	size_t len;

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
		pciemu_irq_raise(dev, PCIEMU_HW_IRQ_FINI);
		ret = pciemu_proxy_request(con, PCIEMU_REQ_ACK);
		if (ret < 0)
			ret_handle = PCIEMU_HANDLE_FAILURE;
		break;
	case PCIEMU_REQ_SYNC:
		ret_handle = pciemu_proxy_handle_sync(dev, con);
		break;
	default:
		ret_handle = PCIEMU_HANDLE_FAILURE;
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
		ret_handle = pciemu_proxy_issue_sync(dev, con);
		break;
	default:
		ret_handle = PCIEMU_HANDLE_FAILURE;
	}

	return ret_handle;
}

int pciemu_proxy_handle_connection(PCIEMUDevice *dev, int con)
{
	int ret, rret;
	ProxyRequest req;
	fd_set fds;
	struct timeval timeout;

	FD_ZERO(&fds);
	bzero(&timeout, sizeof(timeout));
	ret = PCIEMU_HANDLE_SUCCESS;

	/* Comprobar peticiones */

	do {
		FD_SET(con, &fds);
		rret = select(con+1, &fds, NULL, NULL, &timeout);
		if (rret && FD_ISSET(con, &fds)) {
			recv(con, &req, sizeof(req), MSG_WAITALL);
			printf("Debug: Found a request on socket (%X)...", req);
			ret = pciemu_proxy_handle_req(dev, con, req);
			printf(" handled!\n");
		}
		else if (!TAILQ_EMPTY(&dev->proxy.req_head)) {
			req = pciemu_proxy_pop_req(dev);
			printf("Debug: Popped a request on queue (%X)...", req);
			ret = pciemu_proxy_issue_req(dev, con, req);
			printf(" issued!\n");
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
		printf("\nDebug: New client connected\n");
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
	printf("\nDebug: Connected to PCIEMU proxy server\n");
	pciemu_proxy_handle_connection(dev, dev->proxy.sockd);

client_connect_err:
	pthread_exit(NULL);
}

void pciemu_proxy_init_client(PCIEMUDevice *dev)
{
	int ret;

	/* Peticiones de muestra */

	pciemu_proxy_push_req(dev, PCIEMU_REQ_PING);

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
	pciemu_proxy_ftx_wait(&dev->proxy.req_push_ftx);
	TAILQ_INSERT_TAIL(&dev->proxy.req_head, entry, entries);
	pciemu_proxy_ftx_post(&dev->proxy.req_pop_ftx);
	return EXIT_SUCCESS;
}

ProxyRequest pciemu_proxy_pop_req(PCIEMUDevice *dev)
{
	struct pciemu_proxy_req_entry *entry;
	ProxyRequest req;

	pciemu_proxy_ftx_wait(&dev->proxy.req_pop_ftx);
	entry = TAILQ_FIRST(&dev->proxy.req_head);
	if (!entry) {
		pciemu_proxy_ftx_post(&dev->proxy.req_push_ftx);
		return PCIEMU_REQ_NONE;
	}

	TAILQ_REMOVE(&dev->proxy.req_head, entry, entries);
	pciemu_proxy_ftx_post(&dev->proxy.req_push_ftx);
	req = entry->req;
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
	pciemu_sync_bh = qemu_bh_new(pciemu_proxy_sync_bh_handler, dev);

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
	dev->proxy.addr.sin_port = htons(dev->proxy.port);
	dev->proxy.addr.sin_addr.s_addr = *(in_addr_t *)h->h_addr_list[0];

	dev->proxy.tmp_buff = NULL;
	TAILQ_INIT(&dev->proxy.req_head);
	dev->proxy.req_push_ftx = 1;
	dev->proxy.req_pop_ftx = 0;

	if (dev->proxy.server_mode)
		pciemu_proxy_init_server(dev);
	else
		pciemu_proxy_init_client(dev);

}

void pciemu_proxy_fini(PCIEMUDevice *dev)
{
	return;
}

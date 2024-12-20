/* proxy.h - External access to PCIEMU device memory
 *
 * Author: Dorovich (David Cañadas López)
 *
 */
#ifndef PCIEMU_PROXY_H
#define PCIEMU_PROXY_H

#include <pthread.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include "qemu/typedefs.h"
#include "qapi/qmp/qbool.h"

#define PCIEMU_PROXY_HOST "localhost"
#define PCIEMU_PROXY_PORT 8987
#define PCIEMU_PROXY_MAXQ 10
#define PCIEMU_PROXY_BUFF 1024

#define PCIEMU_REQ_NONE 0x00
#define PCIEMU_REQ_ACK 0x01
#define PCIEMU_REQ_PING 0x02
#define PCIEMU_REQ_PONG 0x03
#define PCIEMU_REQ_RESET 0x04
#define PCIEMU_REQ_QUIT 0x05
#define PCIEMU_REQ_INTA 0x06
#define PCIEMU_REQ_SYNC 0x07 /* this <- other */
#define PCIEMU_REQ_SYNCME 0x08 /* this -> other */
#define PCIEMU_REQ_RING 0x09

#define PCIEMU_HANDLE_FAILURE -1
#define PCIEMU_HANDLE_SUCCESS 0
#define PCIEMU_HANDLE_FINISH 1

#define PCIEMU_RECP_SELF 0 /* recipient = self */
#define PCIEMU_RECP_OTHER 1 /* recipient = other end */

/* Forward declaration */
typedef struct PCIEMUDevice PCIEMUDevice;

typedef unsigned int ProxyRequest;

struct pciemu_proxy_req_entry {
	ProxyRequest req;
	TAILQ_ENTRY(pciemu_proxy_req_entry) entries;
};

TAILQ_HEAD(pciemu_proxy_req_head, pciemu_proxy_req_entry);

struct pciemu_proxy {
	pthread_t proxy_thread;
	int sockd;
	bool server_mode;
	void *tmp_conf, *tmp_buff;
	uint16_t port;
	uint32_t req_push_ftx, req_pop_ftx;
	struct sockaddr_in addr;
	struct pciemu_proxy_req_head req_head;
};

typedef struct pciemu_proxy PCIEMUProxy;

void pciemu_proxy_reset(PCIEMUDevice *dev);
void pciemu_proxy_init(PCIEMUDevice *dev, Error **errp);
void pciemu_proxy_fini(PCIEMUDevice *dev);

bool pciemu_proxy_get_mode(Object *obj, Error **errp);
void pciemu_proxy_set_mode(Object *obj, bool mode, Error **errp);

int pciemu_proxy_push_req(PCIEMUDevice *dev, ProxyRequest req);
ProxyRequest pciemu_proxy_pop_req(PCIEMUDevice *dev);

#endif /* PCIEMU_PROXY_H */

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
#define PCIEMU_REQ_WHAT 0x06
#define PCIEMU_REQ_INTA 0x07
#define PCIEMU_REQ_SYNC 0x08 /* this <- other */
#define PCIEMU_REQ_SYNCME 0x09 /* this -> other */

#define PCIEMU_HANDLE_FAILURE -1
#define PCIEMU_HANDLE_SUCCESS 0
#define PCIEMU_HANDLE_FINISH 1

/* Forward declaration */
typedef struct PCIEMUDevice PCIEMUDevice;

typedef unsigned int ProxyRequest;

struct pciemu_proxy_req_entry {
	ProxyRequest req;
	TAILQ_ENTRY(pciemu_proxy_req_entry) entries;
};

struct pciemu_proxy {
	pthread_t proxy_thread;
	struct sockaddr_in addr;
	int sockd;
	bool server_mode;
	uint8_t *tmp_buff;
	TAILQ_HEAD(tailhead, pciemu_proxy_req_entry) req_head;
};

typedef struct pciemu_proxy PCIEMUProxy;

void pciemu_proxy_reset(PCIEMUDevice *dev);
void pciemu_proxy_init(PCIEMUDevice *dev, Error **errp);
void pciemu_proxy_fini(PCIEMUDevice *dev);

bool pciemu_proxy_get_mode(Object *obj, Error **errp);
void pciemu_proxy_set_mode(Object *obj, bool mode, Error **errp);

void pciemu_proxy_push_req(PCIEMUDevice *dev, ProxyRequest req);
ProxyRequest pciemu_proxy_pop_req(PCIEMUDevice *dev);

#endif /* PCIEMU_PROXY_H */

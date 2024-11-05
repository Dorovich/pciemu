/* proxy.h - External access to PCIEMU device memory
 *
 * Author: Dorovich (David Cañadas López)
 *
 */
#ifndef PCIEMU_PROXY_H
#define PCIEMU_PROXY_H

#include <pthread.h>
#include <sys/socket.h>
#include "qemu/typedefs.h"
#include "qapi/qmp/qbool.h"

#define PCIEMU_PROXY_HOST "localhost"
#define PCIEMU_PROXY_PORT 8182
#define PCIEMU_PROXY_MAXQ 10
#define PCIEMU_PROXY_BUFF 1024

typedef unsigned int ProxyRequest;

#define PCIEMU_REQ_ACK 0x01
#define PCIEMU_REQ_PING 0x02
#define PCIEMU_REQ_PONG 0x03
#define PCIEMU_REQ_RESET 0x04
#define PCIEMU_REQ_QUIT 0x05
#define PCIEMU_REQ_WHAT 0x06

/* Forward declaration */
typedef struct PCIEMUDevice PCIEMUDevice;

struct pciemu_proxy {
	pthread_t proxy_thread;
	struct sockaddr_in addr;
	int sockd;
	bool server_mode;
};

void pciemu_proxy_reset(PCIEMUDevice *dev);

void pciemu_proxy_init(PCIEMUDevice *dev, Error **errp);

void pciemu_proxy_fini(PCIEMUDevice *dev);

bool pciemu_proxy_get_mode(Object *obj, Error **errp);

void pciemu_proxy_set_mode(Object *obj, bool mode, Error **errp);

#endif /* PCIEMU_PROXY_H */

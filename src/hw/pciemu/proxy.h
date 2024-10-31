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

#define PCIEMU_PROXY_HOST "localhost"
#define PCIEMU_PROXY_PORT 8182
#define PCIEMU_PROXY_MAXQ 10
#define PCIEMU_PROXY_BUFF 1024

/* Forward declaration */
typedef struct PCIEMUDevice PCIEMUDevice;

struct pciemu_proxy {
	pthread_t proxy_thread;
	struct sockaddr_in addr;
	int sockd;
	int server_role;
};

void pciemu_proxy_reset(PCIEMUDevice *dev);

void pciemu_proxy_init(PCIEMUDevice *dev, Error **errp);

void pciemu_proxy_fini(PCIEMUDevice *dev);

#endif /* PCIEMU_PROXY_H */

/* proxy.h - External access to PCIEMU device memory
 *
 * Author: Dorovich (David Cañadas López)
 *
 */
#ifndef PCIEMU_PROXY_H
#define PCIEMU_PROXY_H

#include <pthread.h>
#include "qemu/typedefs.h"

/* Forward declaration */
typedef struct PCIEMUDevice PCIEMUDevice;

struct pciemu_proxy {
	pthread_t proxy_thread;
};

void pciemu_proxy_reset(PCIEMUDevice *dev);

void pciemu_proxy_init(PCIEMUDevice *dev, Error **errp);

void pciemu_proxy_fini(PCIEMUDevice *dev);

#endif /* PCIEMU_PROXY_H */

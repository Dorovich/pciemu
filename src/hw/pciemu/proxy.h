/* proxy.h - External acess to PCIEMU device memory
 *
 * Author: Dorovich (David Cañadas López)
 *
 */

#ifndef PCIEMU_PROXY_H
#define PCIEMU_PROXY_H

#include "pciemu.h"

/* Forward declaration */
typedef struct PCIEMUDevice PCIEMUDevice;

void pciemu_proxy_reset(PCIEMUDevice *dev);

void pciemu_proxy_init(PCIEMUDevice *dev, Error **errp);

void pciemu_proxy_fini(PCIEMUDevice *dev);

#endif /* PCIEMU_PROXY_H */

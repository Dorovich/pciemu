/* proxy.c - External acess to PCIEMU device memory
 *
 * Author: Dorovich (David Cañadas López)
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include "proxy.h"

/* TODO info:
 * https://systemprogrammingatntu.github.io/mp2/unix_socket.html
 *
 * */

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
	/* comprobar que el socket no exista */
	/* ... */

	dev->pciemu_proxy_socket = socket(AF_LOCAL, SOCK_STREAM, 0);
}

void pciemu_proxy_fini(PCIEMUDevice *dev)
{
	return;
}

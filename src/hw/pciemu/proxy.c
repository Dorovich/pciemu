/* proxy.c - External acess to PCIEMU device memory
 *
 * Author: Dorovich (David Cañadas López)
 *
 */

#include <unistd.h>
#include <fcntl.h>
#include "proxy.h"

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
	int fd = open("qemu-test.txt", O_CREAT|O_TRUNC|O_RDWR, 0666);
	const char *msg = "¡Hola desde QEMU!\n";
	write(fd, msg, sizeof(msg));
	close(fd);
}

void pciemu_proxy_fini(PCIEMUDevice *dev)
{
	return;
}

/* proxy.c - External access to PCIEMU device memory
 *
 * Author: Dorovich (David Cañadas López)
 *
 */

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "proxy.h"
#include "pciemu.h"
#include "sysemu/sysemu.h"
#include "qemu/main-loop.h"

/* -----------------------------------------------------------------------------
 *  Private
 * -----------------------------------------------------------------------------
 */

static QEMUBH *pciemu_proxy_reset_bh;

static void pciemu_proxy_bh_handler (void *opaque)
{
	qmp_system_reset(NULL); /* ver qemu/ui/gtk.c, línea 1313 */
}

void *pciemu_proxy_socket_routine (void *opaque)
{
	/* PCIEMUDevice *dev = opaque; */

	sleep(40);
	qemu_bh_schedule(pciemu_proxy_reset_bh);

	pthread_exit(NULL);
}

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
	int ret;

	pciemu_proxy_reset_bh = qemu_bh_new(pciemu_proxy_bh_handler, NULL);

	ret = pthread_create(&dev->proxy.proxy_thread, NULL,
			pciemu_proxy_socket_routine, dev);
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

void pciemu_proxy_fini(PCIEMUDevice *dev)
{
	return;
}

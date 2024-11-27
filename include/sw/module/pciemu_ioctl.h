/* pciemu_ioctl.h - IOCTL definitions
 *
 * Copyright (c) 2023 Luiz Henrique Suraty Filho <luiz-dev@suraty.com>
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#ifndef _PCIEMU_IOCTL_H_
#define _PCIEMU_IOCTL_H_

struct pciemu_tx {
	unsigned long __user addr; /* input/output data adress */
	unsigned long len; /* length of input data, in bytes */
}

#define PCIEMU_IOCTL_MAGIC 0xE1

/*
 * Start working on a memory transaction described by pciemu_tx
 */
#define PCIEMU_TX_WORK _IOW(PCIEMU_IOCTL_MAGIC, 1, struct pciemu_tx *)

/*
 * Passively wait for a previous transaction to end and get results size
 */
#define PCIEMU_TX_WAIT _IOR(PCIEMU_IOCTL_MAGIC, 2, size_t)

#endif /* _PCIEMU_IOCTL_H_ */

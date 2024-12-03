/* dma.h - Direct Memory Access (DMA) operations
 *
 * Copyright (c) 2023 Luiz Henrique Suraty Filho <luiz-dev@suraty.com>
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#ifndef PCIEMU_DMA_H
#define PCIEMU_DMA_H

#include "qemu/osdep.h"
#include "hw/pci/pci.h"
#include "pciemu_hw.h"

#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL << (n)) - 1))

/* forward declaration (defined in pciemu.h) to avoid circular reference */
typedef struct PCIEMUDevice PCIEMUDevice;

/* dma command */
/* typedef uint64_t dma_cmd_t; */

/* dma mode */
typedef uint64_t dma_mode_t;

/* dma size */
typedef dma_addr_t dma_size_t;

/* dma mask */
typedef uint64_t dma_mask_t;

/* transfer descriptor */
typedef struct DMATransferDesc {
	size_t npages;
	size_t offset;
	dma_size_t len;
	dma_addr_t handles[PCIEMU_HW_BAR0_DMA_WORK_AREA_SIZE];
} DMATransferDesc;

/* configuration of the DMA engine pre-execution */
typedef struct DMAConfig {
	DMATransferDesc txdesc;
	dma_mode_t mode;
	dma_mask_t mask;
} DMAConfig;

/* status of the DMA engine */
typedef enum DMAStatus {
	DMA_STATUS_IDLE,
	DMA_STATUS_EXECUTING,
	DMA_STATUS_OFF,
} DMAStatus;

typedef struct DMAEngine {
	DMAConfig config;
	DMAStatus status;
	uint8_t buff[PCIEMU_HW_DMA_AREA_SIZE];
} DMAEngine;

void pciemu_dma_config_txdesc_npages(PCIEMUDevice *dev, size_t npages);

void pciemu_dma_config_txdesc_addr(PCIEMUDevice *dev, dma_addr_t addr);

/* void pciemu_dma_config_txdesc_src(PCIEMUDevice *dev, dma_addr_t src); */

/* void pciemu_dma_config_txdesc_dst(PCIEMUDevice *dev, dma_addr_t dst); */

void pciemu_dma_config_txdesc_len(PCIEMUDevice *dev, dma_size_t size);

/* void pciemu_dma_config_cmd(PCIEMUDevice *dev, dma_cmd_t cmd); */

void pciemu_dma_config_mode(PCIEMUDevice *dev, dma_mode_t mode);

void pciemu_dma_doorbell_ring(PCIEMUDevice *dev);

void pciemu_dma_reset(PCIEMUDevice *dev);

void pciemu_dma_init(PCIEMUDevice *dev, Error **errp);

void pciemu_dma_fini(PCIEMUDevice *dev);

#endif /* PCIEMU_DMA_H */

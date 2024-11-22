/* pciemu_dma.c - pciemu virtual device DMA operations
 *
 * These are functions that map BARs inside the kernel module and
 * access them directly from the kernel module.
 *
 * Copyright (c) 2023 Luiz Henrique Suraty Filho <luiz-dev@suraty.com>
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#include <linux/dma-mapping.h>
#include "pciemu_module.h"
#include "hw/pciemu_hw.h"

static void pciemu_dma_struct_init(struct pciemu_dma *dma, size_t ofs,
				size_t len, enum dma_data_direction drctn)
{
	dma->offset = ofs;
	dma->len = len;
	dma->direction = drctn;
}

int pciemu_dma_config_npages(struct pciemu_dev *pciemu_dev, size_t npages)
{
	void __iomem *mmio = pciemu_dev->bar.mmio;
	iowrite32((u32)len, mmio + PCIEMU_HW_BAR0_DMA_CFG_TXDESC_NPAGES);
}

int pciemu_dma_work(struct pciemu_dev *pciemu_dev, struct page **pages,
		size_t npages, size_t ofs, size_t len)
{
	struct pci_dev *pdev;
	struct pciemu_dma *dma;
	void __iomem *mmio;
	size_t len_page;

	pdev = pciemu_dev->pdev;
	dma = &pciemu_dev->dma;
	mmio = pciemu_dev->bar.mmio;
	dma->offset = ofs;
	dma->len = len;
	dma->direction = DMA_TO_DEVICE;
	dma->dma_handles = kmalloc(npages*sizeof(struct page *), GPF_KERNEL);

	len_page = (len+ofs) > PAGE_SIZE ? len/PAGE_SIZE : len;
	dma->dma_handles[0] = dma_map_page(&pdev->dev, page,
			dma->offset, dma->len, dma->direction);
	for (int i=1; i<npages; ++i) {
		len_page = 
		dma->dma_handles[i] = dma_map_page(&pdev->dev, pages[i], 0,
				dma->len, dma->direction);
		if (dma_mapping_error(&pdev->dev, dma->dma_handle[i]))
			return -ENOMEM;
	}

	iowrite32((u32)dma->dma_handle[0],
		mmio + PCIEMU_HW_BAR0_DMA_CFG_TXDESC_SRC);
	iowrite32(PCIEMU_HW_DMA_AREA_START,
		mmio + PCIEMU_HW_BAR0_DMA_CFG_TXDESC_DST);
	iowrite32(dma->len,
		mmio + PCIEMU_HW_BAR0_DMA_CFG_TXDESC_LEN);
	iowrite32(PCIEMU_HW_DMA_DIRECTION_TO_DEVICE,
		mmio + PCIEMU_HW_BAR0_DMA_CFG_CMD);
	iowrite32(1, mmio + PCIEMU_HW_BAR0_DMA_DOORBELL_RING);
}

int pciemu_dma_wait(struct pciemu_dev *pciemu_dev)
{
	/* TODO */
	return 0;
}

/* int pciemu_dma_from_host_to_device(struct pciemu_dev *pciemu_dev, */
/* 				struct page *page, size_t ofs, size_t len) */
/* { */
/* 	struct pci_dev *pdev = pciemu_dev->pdev; */
/* 	void __iomem *mmio = pciemu_dev->bar.mmio; */
/* 	pciemu_dma_struct_init(&pciemu_dev->dma, ofs, len, DMA_TO_DEVICE); */
/* 	pciemu_dev->dma.dma_handle = dma_map_page(&pdev->dev, page, */
/* 			pciemu_dev->dma.offset, pciemu_dev->dma.len, */
/* 			pciemu_dev->dma.direction); */
/* 	if (dma_mapping_error(&(pdev->dev), pciemu_dev->dma.dma_handle)) */
/* 		return -ENOMEM; */
/* 	dev_dbg(&pdev->dev, "dma_handle_from = %llx\n", */
/* 		(unsigned long long)pciemu_dev->dma.dma_handle); */
/* 	dev_dbg(&pdev->dev, "cmd = %x\n", PCIEMU_HW_DMA_DIRECTION_TO_DEVICE); */
/* 	iowrite32((u32)pciemu_dev->dma.dma_handle, */
/* 		mmio + PCIEMU_HW_BAR0_DMA_CFG_TXDESC_SRC); */
/* 	iowrite32(PCIEMU_HW_DMA_AREA_START, */
/* 		mmio + PCIEMU_HW_BAR0_DMA_CFG_TXDESC_DST); */
/* 	iowrite32(pciemu_dev->dma.len, */
/* 		mmio + PCIEMU_HW_BAR0_DMA_CFG_TXDESC_LEN); */
/* 	iowrite32(PCIEMU_HW_DMA_DIRECTION_TO_DEVICE, */
/* 		mmio + PCIEMU_HW_BAR0_DMA_CFG_CMD); */
/* 	iowrite32(1, mmio + PCIEMU_HW_BAR0_DMA_DOORBELL_RING); */
/* 	dev_dbg(&pdev->dev, "done host->device...\n"); */
/* 	return 0; */
/* } */

/* int pciemu_dma_from_device_to_host(struct pciemu_dev *pciemu_dev, */
/* 				struct page *page, size_t ofs, size_t len) */
/* { */
/* 	struct pci_dev *pdev = pciemu_dev->pdev; */
/* 	void __iomem *mmio = pciemu_dev->bar.mmio; */
/* 	pciemu_dma_struct_init(&pciemu_dev->dma, ofs, len, DMA_FROM_DEVICE); */
/* 	pciemu_dev->dma.dma_handle = dma_map_page(&pdev->dev, page, */
/* 			pciemu_dev->dma.offset, pciemu_dev->dma.len, */
/* 			pciemu_dev->dma.direction); */
/* 	if (dma_mapping_error(&(pdev->dev), pciemu_dev->dma.dma_handle)) */
/* 		return -ENOMEM; */
/* 	dev_dbg(&(pdev->dev), "dma_handle_to = %llx\n", */
/* 		(unsigned long long)pciemu_dev->dma.dma_handle); */
/* 	dev_dbg(&(pdev->dev), "cmd = %x\n", */
/* 		PCIEMU_HW_DMA_DIRECTION_FROM_DEVICE); */
/* 	iowrite32(PCIEMU_HW_DMA_AREA_START, */
/* 		mmio + PCIEMU_HW_BAR0_DMA_CFG_TXDESC_SRC); */
/* 	iowrite32((u32)pciemu_dev->dma.dma_handle, */
/* 		mmio + PCIEMU_HW_BAR0_DMA_CFG_TXDESC_DST); */
/* 	iowrite32(pciemu_dev->dma.len, */
/* 		mmio + PCIEMU_HW_BAR0_DMA_CFG_TXDESC_LEN); */
/* 	iowrite32(PCIEMU_HW_DMA_DIRECTION_FROM_DEVICE, */
/* 		mmio + PCIEMU_HW_BAR0_DMA_CFG_CMD); */
/* 	iowrite32(1, mmio + PCIEMU_HW_BAR0_DMA_DOORBELL_RING); */
/* 	dev_dbg(&(pdev->dev), "done device->host...\n\n"); */
/* 	return 0; */
/* } */

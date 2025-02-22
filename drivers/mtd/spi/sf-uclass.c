// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2014 Google, Inc
 */

#define LOG_CATEGORY UCLASS_SPI_FLASH

#include <common.h>
#include <dm.h>
#include <log.h>
#include <malloc.h>
#include <spi.h>
#include <spi_flash.h>
#include <asm/global_data.h>
#include <dm/device-internal.h>
#include "sf_internal.h"
#include <blk.h>

DECLARE_GLOBAL_DATA_PTR;

int spi_flash_read_dm(struct udevice *dev, u32 offset, size_t len, void *buf)
{
	return log_ret(sf_get_ops(dev)->read(dev, offset, len, buf));
}

int spi_flash_write_dm(struct udevice *dev, u32 offset, size_t len,
		       const void *buf)
{
	return log_ret(sf_get_ops(dev)->write(dev, offset, len, buf));
}

int spi_flash_erase_dm(struct udevice *dev, u32 offset, size_t len)
{
	return log_ret(sf_get_ops(dev)->erase(dev, offset, len));
}

int spl_flash_get_sw_write_prot(struct udevice *dev)
{
	struct dm_spi_flash_ops *ops = sf_get_ops(dev);

	if (!ops->get_sw_write_prot)
		return -ENOSYS;
	return log_ret(ops->get_sw_write_prot(dev));
}

/*
 * TODO(sjg@chromium.org): This is an old-style function. We should remove
 * it when all SPI flash drivers use dm
 */
struct spi_flash *spi_flash_probe(unsigned int busnum, unsigned int cs,
				  unsigned int max_hz, unsigned int spi_mode)
{
	struct spi_slave *slave;
	struct udevice *bus;
	char *str;

#if defined(CONFIG_SPL_BUILD) && CONFIG_IS_ENABLED(USE_TINY_PRINTF)
	str = "spi_flash";
#else
	char name[30];

	snprintf(name, sizeof(name), "spi_flash@%d:%d", busnum, cs);
	str = strdup(name);
#endif

	if (_spi_get_bus_and_cs(busnum, cs, max_hz, spi_mode,
				"jedec_spi_nor", str, &bus, &slave))
		return NULL;

	return dev_get_uclass_priv(slave->dev);
}

int spi_flash_probe_bus_cs(unsigned int busnum, unsigned int cs,
			   struct udevice **devp)
{
	struct spi_slave *slave;
	struct udevice *bus;
	int ret;

	ret = spi_get_bus_and_cs(busnum, cs, &bus, &slave);
	if (ret)
		return ret;

	*devp = slave->dev;
	return 0;
}

static int spi_flash_post_bind(struct udevice *dev)
{
#if defined(CONFIG_NEEDS_MANUAL_RELOC)
	struct dm_spi_flash_ops *ops = sf_get_ops(dev);
	static int reloc_done;

	if (!reloc_done) {
		if (ops->read)
			ops->read += gd->reloc_off;
		if (ops->write)
			ops->write += gd->reloc_off;
		if (ops->erase)
			ops->erase += gd->reloc_off;

		reloc_done++;
	}
#endif
	return 0;
}

#ifdef CONFIG_SPINOR_BLOCK_SUPPORT
int spacemit_spinor_post_probe(struct udevice *dev)
{
	struct blk_desc *bdesc;
	struct udevice *bdev;
	int ret;
	struct udevice *parent_dev = dev->parent;

	// Create the block device interface for the SPI NOR device with the same parent as dev
	ret = blk_create_devicef(parent_dev, "nor_blk", "blk", IF_TYPE_NOR,
							 dev_seq(dev), SPI_NOR_BLOCK_SIZE, 0, &bdev);
	if (ret) {
		pr_err("Cannot create block device\n");
		return ret;
	}

	// Obtain the block device descriptor
	bdesc = dev_get_uclass_plat(bdev);
	if (!bdesc) {
		pr_err("Failed to get block device descriptor\n");
		return -ENODEV;
	}

	// Initialize block device descriptor
	bdesc->if_type = IF_TYPE_NOR;
	bdesc->removable = 0;

	dev_set_priv(bdev, dev);
	return 0;
}
#endif /* CONFIG_SPINOR_BLOCK_SUPPORT */

UCLASS_DRIVER(spi_flash) = {
	.id		= UCLASS_SPI_FLASH,
	.name		= "spi_flash",
	.post_bind	= spi_flash_post_bind,
#ifdef CONFIG_SPINOR_BLOCK_SUPPORT
	.post_probe	= spacemit_spinor_post_probe,
#endif /* CONFIG_SPINOR_BLOCK_SUPPORT */
	.per_device_auto	= sizeof(struct spi_nor),
};

/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <zephyr/device.h>
#include <zephyr/pm/device.h>
#include <zephyr/drivers/spi.h>

#include <sid_pal_serial_bus_ifc.h>
#include <sid_pal_gpio_ifc.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sid_spi_bus, CONFIG_SPI_BUS_LOG_LEVEL);

#define SPI_OPTIONS (uint16_t)(SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_MASTER | SPI_FULL_DUPLEX)
#define SPI_FREQUENCY_DEFAULT (DT_FREQ_M(8))

struct bus_serial_ctx_t {
	const struct sid_pal_serial_bus_iface *iface;
	const struct device *device;
	struct spi_config cfg;
};

static sid_error_t bus_serial_spi_xfer(const struct sid_pal_serial_bus_iface *iface,
				       const struct sid_pal_serial_bus_client *client,
				       uint8_t *tx,
				       uint8_t *rx,
				       size_t xfer_size);
static sid_error_t bus_serial_spi_destroy(const struct sid_pal_serial_bus_iface *iface);

static struct bus_serial_ctx_t bus_serial_ctx;

static const struct sid_pal_serial_bus_iface bus_ops = {
	.xfer = bus_serial_spi_xfer,
	.destroy = bus_serial_spi_destroy,
};

static sid_error_t bus_serial_spi_xfer(const struct sid_pal_serial_bus_iface *iface,
				       const struct sid_pal_serial_bus_client *client,
				       uint8_t *tx,
				       uint8_t *rx,
				       size_t xfer_size)
{
	LOG_DBG("%s(%p, %p, %p, %p, %d)", __func__, iface, client, tx, rx, xfer_size);
	if (iface != bus_serial_ctx.iface || (!tx && !rx) || !xfer_size || !client) {
		return SID_ERROR_INVALID_ARGS;
	}

	struct spi_buf tx_buff[] = {
		{
			.buf = tx,
			.len = xfer_size,
		},
	};

	struct spi_buf_set tx_set = {
		.buffers = tx_buff,
		.count = 1
	};

	struct spi_buf rx_buff[] = {
		{
			.buf = rx,
			.len = xfer_size,
		},
	};

	struct spi_buf_set rx_set = {
		.buffers =  rx_buff,
		.count = 1
	};

	sid_pal_gpio_write(client->client_selector, 0);

	int err = spi_transceive(bus_serial_ctx.device, &bus_serial_ctx.cfg, ((tx) ? &tx_set : NULL),
				 ((rx) ? &rx_set : NULL));

	sid_pal_gpio_write(client->client_selector, 1);

	sid_error_t ret = SID_ERROR_NONE;
	if (err < 0) {
		LOG_ERR("spi_transceive failed with error %d", err);
		ret = SID_ERROR_GENERIC;
	}

	return ret;
}

static sid_error_t bus_serial_spi_destroy(const struct sid_pal_serial_bus_iface *iface)
{
	LOG_DBG("%s(%p)", __func__, iface);
	if (!iface) {
		return SID_ERROR_INVALID_ARGS;
	}
	return SID_ERROR_NONE;
}

sid_error_t sid_pal_serial_bus_nordic_spi_create(const struct sid_pal_serial_bus_iface **iface, const void *cfg)
{
	ARG_UNUSED(cfg);
	if (!iface) {
		return SID_ERROR_INVALID_ARGS;
	}

	const struct device *spi_device = DEVICE_DT_GET(DT_NODELABEL(sid_semtech));

	if (!device_is_ready(spi_device)) {
		LOG_ERR("SPI device not ready");
		return SID_ERROR_IO_ERROR;
	}

	*iface = &bus_ops;
	bus_serial_ctx = (struct bus_serial_ctx_t){
		.device = spi_device,
		.cfg = {
			.frequency = DT_PROP_OR(DT_NODELABEL(sid_semtech), clock_frequency, SPI_FREQUENCY_DEFAULT),
			.operation = SPI_OPTIONS,
			.cs = NULL,
		},
		.iface = *iface,
	};

	LOG_DBG("SPI device configuration:\r\n"	\
		"frequency \t= %dHz\r\n"	\
		"device flags \t= 0x%X\r\n",
		bus_serial_ctx.cfg.frequency,
		bus_serial_ctx.cfg.operation);

	return SID_ERROR_NONE;
}

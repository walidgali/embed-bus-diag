#define _POSIX_C_SOURCE 200809L

#include "hal.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int linux_spi_open(hal_bus_t *bus, const char *device, uint8_t default_addr) {
    if (!device || device[0] == '\0') {
        return -EINVAL;
    }
    int fd = open(device, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        return -errno;
    }

    uint8_t mode = SPI_MODE_0;
    uint8_t bits = 8;
    uint32_t speed = 1000000u;

    (void)ioctl(fd, SPI_IOC_WR_MODE, &mode);
    (void)ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    (void)ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);

    bus->fd = fd;
    bus->current_addr = default_addr;
    return 0;
}

static int linux_spi_read_reg(hal_bus_t *bus, uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) {
    (void)addr;
    if (!bus || bus->fd < 0 || !buf || len == 0 || len > 256) {
        return -EINVAL;
    }

    uint8_t tx_cmd = (uint8_t)(reg | 0x80u);
    uint8_t dummy_tx[256];
    memset(dummy_tx, 0x00u, len);

    struct spi_ioc_transfer tr[2];
    memset(tr, 0, sizeof(tr));

    tr[0].tx_buf = (unsigned long)&tx_cmd;
    tr[0].rx_buf = 0;
    tr[0].len = 1;
    tr[0].speed_hz = 1000000u;
    tr[0].bits_per_word = 8;

    tr[1].tx_buf = (unsigned long)dummy_tx;
    tr[1].rx_buf = (unsigned long)buf;
    tr[1].len = (uint32_t)len;
    tr[1].speed_hz = 1000000u;
    tr[1].bits_per_word = 8;

    if (ioctl(bus->fd, SPI_IOC_MESSAGE(2), tr) < 0) {
        return -errno;
    }
    return 0;
}

static int linux_spi_write_reg(hal_bus_t *bus, uint8_t addr, uint8_t reg, const uint8_t *buf, size_t len) {
    (void)addr;
    if (!bus || bus->fd < 0 || !buf || len == 0 || len > 255) {
        return -EINVAL;
    }

    uint8_t frame[256];
    frame[0] = (uint8_t)(reg & 0x7Fu);
    memcpy(&frame[1], buf, len);

    struct spi_ioc_transfer tr;
    memset(&tr, 0, sizeof(tr));
    tr.tx_buf = (unsigned long)frame;
    tr.rx_buf = 0;
    tr.len = (uint32_t)(len + 1u);
    tr.speed_hz = 1000000u;
    tr.bits_per_word = 8;

    if (ioctl(bus->fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        return -errno;
    }
    return 0;
}

static int linux_spi_probe_addr(hal_bus_t *bus, uint8_t addr) {
    uint8_t probe_byte = 0;
    return linux_spi_read_reg(bus, addr, 0x00u, &probe_byte, 1);
}

static void linux_spi_close(hal_bus_t *bus) {
    if (bus && bus->fd >= 0) {
        close(bus->fd);
        bus->fd = -1;
    }
}

const hal_ops_t g_hal_spi_ops = {
    .backend_name = "spi",
    .open = linux_spi_open,
    .read_reg = linux_spi_read_reg,
    .write_reg = linux_spi_write_reg,
    .probe_addr = linux_spi_probe_addr,
    .close = linux_spi_close
};

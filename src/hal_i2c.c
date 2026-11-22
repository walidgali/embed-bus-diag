#define _POSIX_C_SOURCE 200809L

#include "hal.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int linux_i2c_open(hal_bus_t *bus, const char *device, uint8_t default_addr) {
    if (!device || device[0] == '\0') {
        return -EINVAL;
    }
    int fd = open(device, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        return -errno;
    }
    bus->fd = fd;
    bus->current_addr = default_addr;
    return 0;
}

static int linux_i2c_read_reg(hal_bus_t *bus, uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) {
    if (bus->fd < 0 || len > 256) {
        return -EINVAL;
    }
    struct i2c_msg msgs[2] = {
        { .addr = addr, .flags = 0, .len = 1, .buf = &reg },
        { .addr = addr, .flags = I2C_M_RD, .len = (uint16_t)len, .buf = buf }
    };
    struct i2c_rdwr_ioctl_data ioctl_data = {
        .msgs = msgs,
        .nmsgs = 2
    };
    if (ioctl(bus->fd, I2C_RDWR, &ioctl_data) < 0) {
        return -errno;
    }
    return 0;
}

static int linux_i2c_write_reg(hal_bus_t *bus, uint8_t addr, uint8_t reg, const uint8_t *buf, size_t len) {
    if (bus->fd < 0 || len > 255) {
        return -EINVAL;
    }
    uint8_t tmp[256];
    tmp[0] = reg;
    for (size_t i = 0; i < len; i++) {
        tmp[i + 1] = buf[i];
    }
    struct i2c_msg msg = {
        .addr = addr,
        .flags = 0,
        .len = (uint16_t)(len + 1),
        .buf = tmp
    };
    struct i2c_rdwr_ioctl_data ioctl_data = {
        .msgs = &msg,
        .nmsgs = 1
    };
    if (ioctl(bus->fd, I2C_RDWR, &ioctl_data) < 0) {
        return -errno;
    }
    return 0;
}

static int linux_i2c_probe_addr(hal_bus_t *bus, uint8_t addr) {
    if (bus->fd < 0) {
        return -EBADF;
    }
    uint8_t dummy = 0;
    struct i2c_msg msg = {
        .addr = addr,
        .flags = I2C_M_RD,
        .len = 1,
        .buf = &dummy
    };
    struct i2c_rdwr_ioctl_data ioctl_data = {
        .msgs = &msg,
        .nmsgs = 1
    };
    if (ioctl(bus->fd, I2C_RDWR, &ioctl_data) < 0) {
        return -errno;
    }
    return 0;
}

static void linux_i2c_close(hal_bus_t *bus) {
    if (bus && bus->fd >= 0) {
        close(bus->fd);
        bus->fd = -1;
    }
}

const hal_ops_t g_hal_i2c_ops = {
    .backend_name = "linux-i2c",
    .open = linux_i2c_open,
    .read_reg = linux_i2c_read_reg,
    .write_reg = linux_i2c_write_reg,
    .probe_addr = linux_i2c_probe_addr,
    .close = linux_i2c_close
};

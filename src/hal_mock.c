#include "hal.h"

#include <errno.h>
#include <string.h>

typedef struct {
    uint8_t active_addr;
    uint8_t regs[256];
    uint32_t step;
    int is_open;
} mock_bus_state_t;

static mock_bus_state_t g_mock_bus;

int hal_open(hal_bus_t *bus, const hal_ops_t *ops, const char *device, uint8_t default_addr) {
    if (!bus || !ops || !ops->open) {
        return -EINVAL;
    }
    memset(bus, 0, sizeof(*bus));
    bus->fd = -1;
    bus->current_addr = default_addr;
    bus->ops = ops;
    return ops->open(bus, device, default_addr);
}

int hal_read_reg(hal_bus_t *bus, uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) {
    if (!bus || !bus->ops || !bus->ops->read_reg || !buf || len == 0) {
        return -EINVAL;
    }
    return bus->ops->read_reg(bus, addr, reg, buf, len);
}

int hal_write_reg(hal_bus_t *bus, uint8_t addr, uint8_t reg, const uint8_t *buf, size_t len) {
    if (!bus || !bus->ops || !bus->ops->write_reg || !buf || len == 0) {
        return -EINVAL;
    }
    return bus->ops->write_reg(bus, addr, reg, buf, len);
}

int hal_probe_addr(hal_bus_t *bus, uint8_t addr) {
    if (!bus || !bus->ops || !bus->ops->probe_addr) {
        return -EINVAL;
    }
    return bus->ops->probe_addr(bus, addr);
}

void hal_close(hal_bus_t *bus) {
    if (bus && bus->ops && bus->ops->close) {
        bus->ops->close(bus);
    }
}

static int mock_open(hal_bus_t *bus, const char *device, uint8_t default_addr) {
    (void)device;
    memset(&g_mock_bus, 0, sizeof(g_mock_bus));
    g_mock_bus.active_addr = default_addr ? default_addr : 0x48u;
    g_mock_bus.is_open = 1;
    g_mock_bus.regs[0x00] = 0x19u;
    g_mock_bus.regs[0x01] = 0x00u;

    bus->fd = 100;
    bus->priv = &g_mock_bus;
    return 0;
}

static int mock_read_reg(hal_bus_t *bus, uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) {
    mock_bus_state_t *st = (mock_bus_state_t *)bus->priv;
    if (!st || !st->is_open) {
        return -EBADF;
    }
    if (addr != st->active_addr) {
        return -ENXIO;
    }
    if ((size_t)reg + len > sizeof(st->regs)) {
        return -EOVERFLOW;
    }

    /* محاكاة تغير طفيف واقعي في القراءة مع كل عينة */
    uint16_t base = (uint16_t)(((uint16_t)st->regs[reg] << 8) | st->regs[reg + 1]);
    uint16_t delta = (uint16_t)((st->step % 5u) << 4);
    uint16_t sim_word = (uint16_t)(base + delta);
    st->step++;

    if (len == 2) {
        buf[0] = (uint8_t)((sim_word >> 8) & 0xFFu);
        buf[1] = (uint8_t)(sim_word & 0xFFu);
    } else {
        memcpy(buf, &st->regs[reg], len);
    }
    return 0;
}

static int mock_write_reg(hal_bus_t *bus, uint8_t addr, uint8_t reg, const uint8_t *buf, size_t len) {
    mock_bus_state_t *st = (mock_bus_state_t *)bus->priv;
    if (!st || !st->is_open) {
        return -EBADF;
    }
    if (addr != st->active_addr) {
        return -ENXIO;
    }
    if ((size_t)reg + len > sizeof(st->regs)) {
        return -EOVERFLOW;
    }
    memcpy(&st->regs[reg], buf, len);
    return 0;
}

static int mock_probe_addr(hal_bus_t *bus, uint8_t addr) {
    mock_bus_state_t *st = (mock_bus_state_t *)bus->priv;
    if (!st || !st->is_open) {
        return -EBADF;
    }
    return (addr == st->active_addr) ? 0 : -ENXIO;
}

static void mock_close(hal_bus_t *bus) {
    mock_bus_state_t *st = (mock_bus_state_t *)bus->priv;
    if (st) {
        st->is_open = 0;
    }
    bus->fd = -1;
    bus->priv = NULL;
}

const hal_ops_t g_hal_mock_ops = {
    .backend_name = "mock",
    .open = mock_open,
    .read_reg = mock_read_reg,
    .write_reg = mock_write_reg,
    .probe_addr = mock_probe_addr,
    .close = mock_close
};

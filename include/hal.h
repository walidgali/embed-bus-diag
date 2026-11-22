#ifndef EMBED_HAL_H
#define EMBED_HAL_H

#include <stddef.h>
#include <stdint.h>

typedef struct hal_bus hal_bus_t;

typedef struct {
    const char *backend_name;
    int (*open)(hal_bus_t *bus, const char *device, uint8_t default_addr);
    int (*read_reg)(hal_bus_t *bus, uint8_t addr, uint8_t reg, uint8_t *buf, size_t len);
    int (*write_reg)(hal_bus_t *bus, uint8_t addr, uint8_t reg, const uint8_t *buf, size_t len);
    int (*probe_addr)(hal_bus_t *bus, uint8_t addr);
    void (*close)(hal_bus_t *bus);
} hal_ops_t;

struct hal_bus {
    const hal_ops_t *ops;
    int fd;
    uint8_t current_addr;
    void *priv;
};

int hal_open(hal_bus_t *bus, const hal_ops_t *ops, const char *device, uint8_t default_addr);
int hal_read_reg(hal_bus_t *bus, uint8_t addr, uint8_t reg, uint8_t *buf, size_t len);
int hal_write_reg(hal_bus_t *bus, uint8_t addr, uint8_t reg, const uint8_t *buf, size_t len);
int hal_probe_addr(hal_bus_t *bus, uint8_t addr);
void hal_close(hal_bus_t *bus);

extern const hal_ops_t g_hal_mock_ops;
extern const hal_ops_t g_hal_i2c_ops;

#endif /* EMBED_HAL_H */

extern const hal_ops_t g_hal_spi_ops;

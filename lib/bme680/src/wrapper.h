#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

extern bool auto_pull_up;
extern bool auto_pull_down;
extern bool gpio_isr_service_installed;

uint32_t sdk_system_get_time();

esp_err_t gpio_set_interrupt(
    gpio_num_t gpio, gpio_int_type_t type, gpio_isr_t handler
);

void gpio_enable(gpio_num_t gpio, const gpio_mode_t mode);

void i2c_init(int bus, gpio_num_t sda, gpio_num_t scl, uint32_t freq);

int i2c_slave_write(
    uint8_t bus, uint8_t addr, const uint8_t *reg, uint8_t *data, uint32_t len
);

int i2c_slave_read(
    uint8_t bus, uint8_t addr, const uint8_t *reg, uint8_t *data, uint32_t len
);

bool spi_bus_init(
    spi_host_device_t host, uint8_t sclk, uint8_t miso, uint8_t mosi
);

bool spi_device_init(uint8_t bus, uint8_t cs);

size_t spi_transfer_pf(
    uint8_t bus, uint8_t cs, const uint8_t *mosi, uint8_t *miso, uint16_t len
);

#ifdef __cplusplus
}
#endif

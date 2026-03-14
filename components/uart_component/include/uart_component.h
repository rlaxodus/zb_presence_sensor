#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/uart.h"

typedef struct {
    float val;
    bool status;
    uint32_t timestamp;
} uart_data_t;

esp_err_t uart_component_init(uart_port_t port, QueueHandle_t output_queue, int tx_io, int rx_io);
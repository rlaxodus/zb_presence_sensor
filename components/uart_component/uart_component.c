#include "uart_component.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/task.h"

#define BUF_SIZE 1024

// TODO: rename this tag when copying to a new sensor component
static const char *TAG = "UART_COMP";

//Stores the queues for UART(hardware) and OUTPUT(main)
typedef struct {
    QueueHandle_t uart_q;
    QueueHandle_t out_q;
    uart_port_t uart_num;
} task_params_t;

static task_params_t s_params;
static uart_data_t internal_data;
static SemaphoreHandle_t data_mutex = NULL;

/** FUNCTIONS */

static void uart_component_task(void *pvParameters) {
    task_params_t *params = (task_params_t *)pvParameters;
    uart_event_t event;
    uint8_t data[BUF_SIZE];

    for (;;) {
        // Blocks until a UART event (like DATA) occurs
        // Receive from UART queue
        if (xQueueReceive(params->uart_q, &event, portMAX_DELAY)) {
            switch (event.type) {
                case UART_DATA: {
                    int len = uart_read_bytes(params->uart_num, data, event.size, pdMS_TO_TICKS(100));

                    // --- START OF CUSTOM PARSER ---
                    // TODO: replace this block with the target sensor's protocol
                    if (len >= 6 && data[0] == 0xF4) { // Example: LD2412 frame header
                        uart_data_t report = {
                            .status = (data[4] > 0),
                            .val    = (float)data[5],
                            .timestamp = xTaskGetTickCount()
                        };
                        
                        //Save internally
                        xSemaphoreTake(data_mutex, portMAX_DELAY);
                        internal_data = report;
                        xSemaphoreGive(data_mutex);

                        //Send to OUTPUT queue
                        xQueueSend(params->out_q, &report, 0);
                    }
                    // --- END OF CUSTOM PARSER ---
                    break;
                }

                case UART_FIFO_OVF:
                    ESP_LOGW(TAG, "UART FIFO Overflow");
                    uart_flush_input(params->uart_num);
                    break;

                default:
                    break;
            }
        }
    }
}

esp_err_t uart_component_init(uart_port_t uart_num, QueueHandle_t output_queue, int tx_io, int rx_io) {
    data_mutex = xSemaphoreCreateMutex();

    const uart_config_t uart_config = {
        .baud_rate  = 115200, // TODO: set baud rate for target sensor
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    //Save OUTPUT queue to send to main
    s_params.out_q = output_queue;
    s_params.uart_num  = uart_num;

    ESP_ERROR_CHECK(uart_driver_install(uart_num, BUF_SIZE * 2, 0, 20, &s_params.uart_q, 0));
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(uart_num, tx_io, rx_io, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    xTaskCreate(uart_component_task, "uart_comp_task", 4096, &s_params, 5, NULL);

    return ESP_OK;
}

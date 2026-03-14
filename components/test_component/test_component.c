#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_random.h"
#include "esp_log.h"
#include "test_component.h"
#include <math.h>

static const char *TAG = "TEST_COMPONENT";

static QueueHandle_t s_queue = NULL;
static test_data_t   internal_data;
static SemaphoreHandle_t data_mutex = NULL;

static float    report_threshold  = 1.0f;
static uint32_t report_interval_ms = 500;

void test_comp_set_config(float threshold, uint32_t interval_ms) {
    report_threshold   = threshold;
    report_interval_ms = interval_ms;
}

static void component_task(void *pvParameters) {
    float    last_reported_val = 0.0f;
    TickType_t last_report_time = 0;
    bool     presence_state    = false;

    while (1) {
        float current_val = 50.0f + (esp_random() % 450);

        bool big_enough  = (fabs(current_val - last_reported_val) >= report_threshold);
        bool long_enough = (xTaskGetTickCount() - last_report_time >= pdMS_TO_TICKS(report_interval_ms));

        if (big_enough && long_enough) {
            presence_state = !presence_state;

            test_data_t fresh_data = {
                .value     = current_val,
                .presence  = presence_state,
                .timestamp = xTaskGetTickCount(),
            };

            // Update polled snapshot
            xSemaphoreTake(data_mutex, portMAX_DELAY);
            internal_data = fresh_data;
            xSemaphoreGive(data_mutex);

            // Post to main's queue — non-blocking, drop if full
            if (s_queue != NULL) {
                xQueueSend(s_queue, &fresh_data, 0);
            }

            last_reported_val = current_val;
            last_report_time  = xTaskGetTickCount();
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

esp_err_t test_comp_init(QueueHandle_t queue) {
    s_queue    = queue;
    data_mutex = xSemaphoreCreateMutex();
    xTaskCreate(component_task, "test_comp_task", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "Initialized");
    return ESP_OK;
}

void test_comp_get_latest(test_data_t *out_data) {
    if (out_data == NULL) return;
    if (xSemaphoreTake(data_mutex, portMAX_DELAY)) {
        *out_data = internal_data;
        xSemaphoreGive(data_mutex);
    }
}

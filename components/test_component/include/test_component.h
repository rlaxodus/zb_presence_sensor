#ifndef TEST_COMPONENT_H
#define TEST_COMPONENT_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Data posted to the queue on each meaningful state change.
 * Cast the void* from xQueueReceive to (test_data_t *).
 */
typedef struct {
    float    value;      // Simulated distance in cm
    bool     presence;   // True if something is detected
    uint32_t timestamp;  // Tick count at time of reading
} test_data_t;

/**
 * @brief Initialize the component and start the background task.
 * @param queue Queue created by the caller. The component posts
 *              test_data_t items to it on meaningful state changes.
 * @return ESP_OK on success.
 */
esp_err_t test_comp_init(QueueHandle_t queue);

/**
 * @brief Read the most recently produced data (poll model).
 * @param out_data Pointer where data will be copied.
 */
void test_comp_get_latest(test_data_t *out_data);

/**
 * @brief Configure reporting behaviour.
 * @param threshold   Minimum change in value to trigger a report.
 * @param interval_ms Minimum time (ms) between consecutive reports.
 */
void test_comp_set_config(float threshold, uint32_t interval_ms);

#endif /* TEST_COMPONENT_H */

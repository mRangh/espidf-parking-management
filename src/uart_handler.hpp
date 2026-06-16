#ifndef UART_HANDLER_HPP
#define UART_HANDLER_HPP

#ifdef __cplusplus
#include <cstdio>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "gate_ctrl.hpp"

#define UART_COMM_NUM UART_NUM_0
#define UART_BUF_SIZE (1024)

static const char *UART_TAG = "UART_COMM";

static void uart_rx_task(void *pvParameters){

    Gate* gate_inst = static_cast<Gate*>(pvParameters);
    
    if (gate_inst == nullptr){
        ESP_LOGE(UART_TAG, "[UART_TASK]: Invalid gate instance\n");
        vTaskDelete(NULL);
        return;
    }

    uint8_t* data = (uint8_t*) malloc(UART_BUF_SIZE);
    while (1) {
        int len = uart_read_bytes(UART_COMM_NUM, data, UART_BUF_SIZE - 1, pdMS_TO_TICKS(20));

        if (len > 0) {
            data[len] = '\0';
            
            while (len > 0 && (data[len - 1] == '\n' || data[len - 1] == '\r')) {
                data[--len] = '\0';
            }

            if (strcmp((char *)data, "RELEASE_GATE") == 0) {
                ESP_LOGI(UART_TAG, "[UART_TASK]: RELEASE_GATE received from Python.");
                gate_inst->release();
            }
        }
    }

    free(data);
    vTaskDelete(NULL);
}

inline void init_uart_communication(Gate& gate_to_control, int core_id) {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_param_config(UART_COMM_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_driver_install(UART_COMM_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0));

    xTaskCreatePinnedToCore(
        uart_rx_task,
        "uart_rx_task",
        3072,
        &gate_to_control,
        5,
        NULL,
        core_id
    );

    ESP_LOGI(UART_TAG, "[UART_COMM]: Successful init on Core %d.", core_id);
}

#endif
#endif
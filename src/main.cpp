/*
 * ============================================================================
 * @file        main.cpp
 *
 * @author      Marco Antônio Ranghetti
 * @github      github.com/mRangh
 * @email       marcoantonioranghetti@gmail.com
 * @academic    d2026008956@unifei.edu.br
 *
 * @version     1.0.0
 * @date        2026-06-22
 * @license     Apache License 2.0
 * ============================================================================
 */


#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp32_hal.hpp>
#include "gate_ctrl.hpp"
#include "uart_handler.hpp"

// =============================================================================
// HARDWARE MAP
// =============================================================================
Servo  servo{19, LEDC_CHANNEL_0};
Switch loop_sensor{12};
Switch paper_sensor{16};
Switch limit_90_sensor{27};
Switch limit_0_sensor{14};
Output led_r{18};
Output led_g{21};
Ultrasonic pass_sensor{13, 34};

// =============================================================================
// GATE CONFIGURATION
// =============================================================================
Gate gate({
    .mode     = Gate::EXIT_VALIDATING,
    .servo    = servo,
    .degree90 = limit_90_sensor,
    .degree0  = limit_0_sensor,
    .loop     = loop_sensor,
    .pass     = pass_sensor,
    .paper    = paper_sensor,
    .led_r    = led_r,
    .led_g    = led_g
});

// Testing button for python return simulation
Button  btn_py_sim(4, GPIO_PULLDOWN_ONLY);

// =============================================================================
// MAIN SYSTEM INIT AND HANDLING
// =============================================================================
extern "C" void app_main(void) {
    printf("[System]: Initializing gate.\n");
    gate.begin(1);
    init_uart_communication(gate, 0);
    printf("[System]: System ready.\n");

    while(true){
        if (gate.current_state == Gate::PROCESSING && !gate.process_done) {
            if (btn_py_sim.read()) {
                printf("[WORKBENCH TEST]: Button pressed. Simulating 'TICKET PRINTED' or 'QRCODE READED' signal.\n");
                gate.release();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
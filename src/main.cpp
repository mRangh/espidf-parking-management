#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hardware_drivers.hpp"
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
Output led_r{25};
Output led_g{26};
Ultrassonic pass_sensor{13, 34};

// =============================================================================
// GATE CONFIGURATION
// =============================================================================
Gate gate({
    .mode     = Gate::ENTRY_PRINTING,
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
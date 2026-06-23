/*
 * ============================================================================
 * @file        gate_ctrl.hpp
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

#ifndef GATE_CTRL_HPP
#define GATE_CTRL_HPP
#ifdef __cplusplus
#include <atomic>
#include <esp32_hal.hpp>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Gate controller state machine for entry and exit handling.
// Controls the servo, status LEDs, and monitors limit switches and loop sensors.
class Gate {
    
    public:
    
        std::atomic<bool> process_done;
    // Gate operation mode: entry requires ticket printing, exit requires QR validation.
        enum Mode {
            ENTRY_PRINTING,
            EXIT_VALIDATING
        };

        // Gate state machine phases.
        enum State {
            LOCKED,
            PROCESSING,
            OPENING,
            OPENED,
            CLOSING,
            TIMEOUT_ERR,
            SECURITY_BREACH
        };

        struct Config {
            Mode mode;
            Servo& servo;
            DigitalInput& degree90;
            DigitalInput& degree0;
            DigitalInput& loop;
            Ultrasonic& pass;
            DigitalInput& paper;
            Output& led_r;
            Output& led_g;
        };

        std::atomic<State> current_state;
        Mode gate_mode;

        // Constructor receives all hardware references used by the gate.
        Gate(const Config& c)
        : gate_mode(c.mode), servo(c.servo), loop_sensor(c.loop), pass_sensor(c.pass), paper_sensor(c.paper),
          degree0(c.degree0), degree90(c.degree90), red_led(c.led_r), green_led(c.led_g) 
        {
            current_state = LOCKED;
            printf("[DEBUG]: current_state = LOCKED\n");
            process_done = false;
            state_timer = 0;

            servo.move(0);
            red_led.write(true);
            green_led.write(false);
        }

        // Start the gate task pinned to a given FreeRTOS core.
        void begin(int core_id) {
            loop_sensor.init();
            pass_sensor.init();
            paper_sensor.init();
            degree0.init();
            degree90.init();
            printf("[DEBUG]: GPIO's initialized.\n");
            xTaskCreatePinnedToCore(
                Gate::task_handler,
                "gate_task",
                4096,
                this,
                5,
                NULL,
                core_id
            );
            printf("[DEBUG]: Task created.\n");
            printf("[SYSTEM]: Initialized gate.\n");
            printf("[SYSTEM]: Mode: %s\n", (gate_mode == ENTRY_PRINTING ? "ENTRY_PRINTING" : "EXIT_VALIDATING"));
            printf("[SYSTEM]: Core: %d\n", core_id);
            printf("[DEBUG]: current_state: LOCKED\n");
        }

        // Mark the current gate operation as complete so the machine can open.
        void release() {
            if (current_state == PROCESSING && process_done == false) {
                process_done = true;
                state_timer = esp_timer_get_time() / 1000;
                if (gate_mode == ENTRY_PRINTING) printf("[GATE]: Task complete. Waiting for ticket\n");
            }
        }

    private:

        Servo& servo;
        DigitalInput& loop_sensor;
        Ultrasonic& pass_sensor;
        DigitalInput& paper_sensor;
        DigitalInput& degree0;
        DigitalInput& degree90;
        Output& red_led;
        Output& green_led;

        std::atomic<uint32_t> state_timer;

        enum OPENED_MACHINE_STATES{
            ON_LOOP,
            OFF_LOOP,
            ON_PASS
        }; OPENED_MACHINE_STATES opened_state = ON_LOOP;

        enum PRINTING_MACHINE_STATES {
            WAIT_PRINT,
            WAIT_REMOVAL
        }; PRINTING_MACHINE_STATES printing_state = WAIT_PRINT;

        // FreeRTOS task entry point that repeatedly runs the state machine.
        static void task_handler(void* pvParameters) {
            Gate* instance = static_cast<Gate*>(pvParameters);

            while (true) {
                instance->run_machine();
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        }

        void run_machine() {
            switch (current_state) {
                case LOCKED:
                    process_done = false;
                    vTaskDelay(pdMS_TO_TICKS(100));
                    if (loop_sensor.read() == 1) {
                        state_timer = esp_timer_get_time() / 1000;
                        current_state = PROCESSING;
                        printing_state = WAIT_PRINT;
                        printf("[DEBUG]: current_state = PROCESSING\n");
                        printf("[%s]: Requesting processment...\n", (gate_mode == ENTRY_PRINTING ? "ENTRY_GATE" : "EXIT_GATE"));

                        if (gate_mode == ENTRY_PRINTING) {
                            printf("[ENTRY_GATE]: Vehicle detected. Requesting ticket printing...\n");
                        } else if (gate_mode == EXIT_VALIDATING){
                            printf("[EXIT_GATE]: Vehicle detected. Awaiting QR code validation...\n");
                        }
                    }
                    if (pass_sensor.read_cm() <= 10) {
                        current_state = SECURITY_BREACH;
                        printf("[DEBUG]: current_state = SECURITY_BREACH\n");
                        printf("[SECURITY_BREACH]: Violated Gate!\n");
                    }
                    break;

                case PROCESSING:
                    red_led.write((esp_timer_get_time() / 500000) % 2);
                    
                    if (process_done) {
                        if (gate_mode == ENTRY_PRINTING) {
                            switch(printing_state){
                                case WAIT_PRINT:
                                    if (paper_sensor.read() == 0) {
                                        if ((esp_timer_get_time() / 1000) - state_timer > 120000) {
                                            printf("[GATE_ERR]: Out of paper or paper stuck\n");
                                            state_timer = esp_timer_get_time() / 1000;
                                            current_state = TIMEOUT_ERR;
                                            printf("[DEBUG]: current_state = TIMEOUT_ERR\n");
                                            return;
                                        }
                                    } else {
                                        printf("[GATE]: Ticket printed. Waiting for the ticket remotion\n");
                                        printing_state = WAIT_REMOVAL;
                                        state_timer = esp_timer_get_time() / 1000;
                                        printf("[DEBUG]: printing_state = WAIT_REMOVAL\n");
                                    }
                                    break;
                                
                                case WAIT_REMOVAL:
                                    if (paper_sensor.read() == 0) {
                                        printf("[GATE]: Ticket removed. Opening gate\n");
                                        state_timer = esp_timer_get_time() / 1000;
                                        current_state = OPENING;
                                        printing_state = WAIT_PRINT;
                                        printf("[DEBUG]: current_state = OPENING\n");
                                    }
                                    if ((esp_timer_get_time() / 1000) - state_timer > 120000 && loop_sensor.read() == 0 && paper_sensor.read() == 1) {
                                        printf("[GATE_WARN]: No vehicle detected to remove ticket. Closing gate\n");
                                        state_timer = esp_timer_get_time() / 1000;
                                        current_state = CLOSING;
                                        printing_state = WAIT_PRINT;
                                        printf("[DEBUG]: current_state = CLOSING\n");                                        
                                    }
                                    break;
                            }
                        } else if (gate_mode == EXIT_VALIDATING) {
                            printf("[GATE]: QRcode validated. Opening gate\n");
                            state_timer = esp_timer_get_time() / 1000;
                            current_state = OPENING;
                            printf("[DEBUG]: current_state = OPENING\n");
                        }
                    }

                    if (!process_done && ((esp_timer_get_time() / 1000) - state_timer > 120000)) {
                        current_state = TIMEOUT_ERR;
                        printf("[GATE_ERR]: Timeout waiting for processment to complete.\n");
                        printf("[DEBUG]: current_state = TIMEOUT_ERR\n");
                    }
                    break;

                case OPENING:
                    red_led.write(false);
                    green_led.write(true);
                    servo.move(90);
                    
                    if (degree90.read() == 1) {
                        if (degree0.read() == 1) printf("[GATE_WARN]: Check degree0 sensor health.\n");
                        state_timer = esp_timer_get_time() / 1000;
                        current_state = OPENED;
                        opened_state = ON_LOOP;
                        printf("[DEBUG]: current_state = OPENED\n");
                        printf("[DEBUG]: Starting OPENED_MACHINE_STATES loop\n");
                        printf("[GATE]: Gate open. End stop reached.\n");
                    } else if ((esp_timer_get_time() / 1000) - state_timer > 120000) {
                        current_state = TIMEOUT_ERR;
                        printf("[GATE_ERR]: Mechanical lock or 90° sensor failure.\n");
                    }
                    if (pass_sensor.read_cm() <= 200) {
                        opened_state = ON_PASS;
                        state_timer = esp_timer_get_time() / 1000;
                        printf("[DEBUG]: opened_state = ON_PASS\n");
                        printf("[GATE_WARN]: Vehicle detected\n");
                    } else opened_state = ON_LOOP;
                    break;

                case OPENED: {
                    switch(opened_state){
                        case ON_LOOP:
                            if (loop_sensor.read() == 0) {
                                opened_state = OFF_LOOP;
                                state_timer = esp_timer_get_time() / 1000;
                                printf("[DEBUG]: opened_state = OFF_LOOP\n");
                                printf("[GATE_WARN]: No vehicle detected\n");
                            }
                            if (pass_sensor.read_cm() <= 200) {
                                opened_state = ON_PASS;
                                state_timer = esp_timer_get_time() / 1000;
                                printf("[DEBUG]: opened_state = ON_PASS\n");
                            }
                            break;
                        
                            case OFF_LOOP:
                                if (pass_sensor.read_cm() <= 200) {
                                    opened_state = ON_PASS;
                                    printf("[GATE_WARN]: Vehicle detected\n");
                                    printf("[DEBUG]: opened_state = ON_PASS\n");
                                }
                                if ((esp_timer_get_time() / 1000) - state_timer > 120000) {
                                    printf("[GATE_WARN]: Closing for time out\n");
                                    state_timer = esp_timer_get_time() / 1000;
                                    current_state = CLOSING;
                                    printf("[DEBUG]: current_state = CLOSING\n");
                                }
                                break;

                            case ON_PASS:
                                if (pass_sensor.read_cm() > 200) {
                                    state_timer = esp_timer_get_time() / 1000;
                                    current_state = CLOSING;
                                    printf("[DEBUG]: current_state = CLOSING\n");
                                }
                                break;
                    }       
                    break;
                }

                case CLOSING:
                    green_led.write(false);
                    red_led.write(true);
                    servo.move(0);

                    if (degree0.read() == 1) {
                        current_state = LOCKED;
                        printf("[GATE]: Gate closed. End stop reached.\n");
                        printf("[DEBUG]: current_state = LOCKED\n");
                    } else if ((esp_timer_get_time() / 1000) - state_timer > 120000) {
                        current_state = TIMEOUT_ERR;
                        printf("[DEBUG]: current_state = TIMEOUT_ERR\n");
                        printf("[GATE_ERR]: Mechanical lock or 0° sensor failure.\n");
                    }
                    if (pass_sensor.read_cm() <= 200) {
                        state_timer = esp_timer_get_time() / 1000;
                        current_state = OPENING;
                        printf("[CRITICAL]: Possible gate obstruction or Secutiry breach. Switching to OPENING state for safety.\n");
                        printf("[DEBUG]: current_state = OPENING\n");
                    }
                    break;

                case TIMEOUT_ERR:
                    red_led.write((esp_timer_get_time() / 200000) % 2);
                    green_led.write((esp_timer_get_time() / 200000) % 2);

                    if (loop_sensor.read() == 0 && pass_sensor.read_cm() > 200) {
                        printf("[GATE_WARN]: Vehicle cleared the loop. Initiating automatic error recovery...\n");
                        process_done = false;
                        state_timer = esp_timer_get_time() / 1000;
                        
                        current_state = CLOSING; 
                        printf("[DEBUG]: current_state = CLOSING (Recovered from TIMEOUT_ERR)\n");
                    }
                    break;
                
                case SECURITY_BREACH:
                    red_led.write((esp_timer_get_time() / 200000) % 2);
                    green_led.write((esp_timer_get_time() / 200000) % 2);
                    if ((esp_timer_get_time() / 200000) % 10 == 0) printf("[SECURITY BREACH]: Calling security!\n");
            }
        }
};

#endif
#endif

#ifndef HARDWARE_DRIVERS_HPP
#define HARDWARE_DRIVERS_HPP

#ifdef __cplusplus

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "esp_adc/adc_oneshot.h"
#include "rom/ets_sys.h"
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>

class DigitalInput {

    public:

        gpio_num_t pin;
        gpio_pull_mode_t pull_mode;
        bool value;

        virtual ~DigitalInput() = default;
        
        DigitalInput(int port, gpio_pull_mode_t pull = GPIO_PULLDOWN_ONLY){
            pin = (gpio_num_t)port;
            pull_mode = pull;
            value = 0;
        }

        void init(){
            gpio_config_t io_conf    = {};
            io_conf.intr_type        = GPIO_INTR_DISABLE;
            io_conf.mode             = GPIO_MODE_INPUT;
            io_conf.pin_bit_mask     = (1ULL << pin);
        
            if (pull_mode == GPIO_PULLUP_ONLY){
                io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
                io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            } else if (pull_mode == GPIO_PULLDOWN_ONLY) {
                io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
                io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
            } else {
                io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
                io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            } gpio_config(&io_conf);
        }

        virtual int read(){
            return gpio_get_level(pin);
        }

};

class AnalogInput {
    
    public:
    
        gpio_num_t pin;
        int value;
        adc_oneshot_unit_handle_t adc_handle;
        adc_channel_t adc_channel;

        virtual ~AnalogInput() = default;

        AnalogInput(int port){
            pin = (gpio_num_t)port;
            value = 0;
            adc_handle = nullptr;
        }
        
        void init(){
            if (adc_handle != nullptr) return;
            if (pin == GPIO_NUM_32) adc_channel = ADC_CHANNEL_4;
            else if (pin == GPIO_NUM_33) adc_channel = ADC_CHANNEL_5;
            else if (pin == GPIO_NUM_34) adc_channel = ADC_CHANNEL_6;
            else if (pin == GPIO_NUM_35) adc_channel = ADC_CHANNEL_7;
            else adc_channel = ADC_CHANNEL_0;
    
            adc_oneshot_unit_init_cfg_t init_config = {};
            init_config.unit_id = ADC_UNIT_1;
            adc_oneshot_new_unit(&init_config, &adc_handle);
    
            adc_oneshot_chan_cfg_t config = {};
            config.bitwidth = ADC_BITWIDTH_12;
            config.atten = ADC_ATTEN_DB_12; 
            adc_oneshot_config_channel(adc_handle, adc_channel, &config);
        }

        virtual int read(){
            int raw_value = 0;
            adc_oneshot_read(adc_handle, adc_channel, &raw_value);
            value = raw_value;
            return value;
        }
};

class Output {

    public:

        gpio_num_t pin;
        bool value;

        Output(int port){
            pin = (gpio_num_t)port;
            value = false;
            init();
        }
        
        void init(){
            gpio_config_t io_conf = {};
            io_conf.intr_type     = GPIO_INTR_DISABLE;
            io_conf.mode          = GPIO_MODE_OUTPUT;
            io_conf.pin_bit_mask  = (1ULL << pin);
            io_conf.pull_up_en    = GPIO_PULLUP_DISABLE;
            io_conf.pull_down_en  = GPIO_PULLDOWN_DISABLE;
            gpio_config(&io_conf);
        }

        Output& write(bool src){
            value = src;
            gpio_set_level(pin, value);
            return *this;
        }

};

class Switch : public DigitalInput {
    
    public:
    
        Switch(int port, gpio_pull_mode_t pull = GPIO_PULLDOWN_ONLY) : DigitalInput(port, pull) {}

};

class Button : public DigitalInput {

    public:

        Button(int port, gpio_pull_mode_t pull = GPIO_PULLDOWN_ONLY) : DigitalInput(port, pull) {}

        bool toggle(){
            actual_state = read();
            TickType_t current_time = xTaskGetTickCount();
            const TickType_t debounce_delay = pdMS_TO_TICKS(50);
            if (actual_state == 1 && last_state == 0) {
                if (current_time - last_debounce_time > debounce_delay) {
                    switch_state = !switch_state;
                    last_debounce_time = current_time;
                }
            }
            last_state = actual_state;
            value = switch_state;
            return value;
        }

    private:

        TickType_t last_debounce_time = 0;
        bool switch_state = 0; // Current toggled state.
        bool last_state = 0;   // Previous raw input read.
        bool actual_state = 0; // Current raw input read.
};

class Servo {

    public:

        gpio_num_t pin;
        ledc_channel_t channel;
        int current_angle;

        Servo(int port, ledc_channel_t ch = LEDC_CHANNEL_0) {
            pin = (gpio_num_t)port;
            channel = ch;
            current_angle = 0;
    
            ledc_timer_config_t ledc_timer = {};
            ledc_timer.speed_mode          = LEDC_LOW_SPEED_MODE;
            ledc_timer.timer_num           = LEDC_TIMER_0;
            ledc_timer.duty_resolution     = LEDC_TIMER_13_BIT;
            ledc_timer.freq_hz             = 50;
            ledc_timer.clk_cfg             = LEDC_AUTO_CLK;
            ledc_timer_config(&ledc_timer);
    
            ledc_channel_config_t ledc_channel = {};
            ledc_channel.speed_mode     = LEDC_LOW_SPEED_MODE;
            ledc_channel.channel        = channel;
            ledc_channel.timer_sel      = LEDC_TIMER_0;
            ledc_channel.gpio_num       = pin;
            ledc_channel.duty           = 0;
            ledc_channel.hpoint         = 0;
            ledc_channel_config(&ledc_channel);
        }

        Servo& move(int angle){
            if (angle < 0) angle = 0;
            if (angle > 180) angle = 180;
            current_angle = angle;

            uint32_t duty = 205 + ((angle * (1024 - 205))/180);
            ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);

            return *this;
        }

};

class Potentiometer : public AnalogInput {

    public:

        Potentiometer(int port) : AnalogInput(port) {}
        
};

class Ultrassonic {
    
    private:
        gpio_num_t trig_pin;
        gpio_num_t echo_pin;
        uint32_t timeout;

    public:
        Ultrassonic(int trig_port, int echo_port)
            : trig_pin((gpio_num_t)trig_port), echo_pin((gpio_num_t)echo_port), timeout(12000) {}
        
        void init() {
            gpio_config_t io_conf = {};
            io_conf.intr_type     = GPIO_INTR_DISABLE;
            io_conf.mode          = GPIO_MODE_OUTPUT;
            io_conf.pin_bit_mask  = (1ULL << trig_pin);
            gpio_config(&io_conf);
            gpio_set_level(trig_pin, 0);

            io_conf.mode          = GPIO_MODE_INPUT;
            io_conf.pin_bit_mask = (1ULL << echo_pin);
            gpio_config(&io_conf);
        }

        float read_cm() {
        gpio_set_level(trig_pin, 1);
        ets_delay_us(10);
        gpio_set_level(trig_pin, 0);

        uint64_t start_time = esp_timer_get_time();
        while (gpio_get_level(echo_pin) == 0) {
            if (esp_timer_get_time() - start_time > timeout) return 999.0f;
        }

        uint64_t echo_start = esp_timer_get_time();
        while (gpio_get_level(echo_pin) == 1) {
            if (esp_timer_get_time() - echo_start > timeout) return 999.0f;
        }
        uint64_t echo_duration = esp_timer_get_time() - echo_start;

        return (float)echo_duration * 0.0343f / 2.0f;
    }

};

#endif
#endif
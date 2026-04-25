/*
 * ESP32 Sumo Robot
 *
 * Academic autonomous sumo robot firmware built with ESP-IDF.
 *
 * Main behavior:
 * - TCRT5000 edge sensors read through ADC1 channels GPIO34/GPIO35.
 * - HC-SR04 ultrasonic sensor measures opponent distance in millimeters.
 * - L298N H-bridge controls two DC motors through direction pins and LEDC PWM.
 * - Behavior priority:
 *   1. both edge sensors detect white -> stop
 *   2. front edge sensor detects white -> move backward
 *   3. rear edge sensor detects white -> move forward
 *   4. opponent <= 150 mm -> attack forward at maximum duty
 *   5. otherwise -> spin in place searching for opponent
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_rom_sys.h"

static const char *TAG = "SUMO_ROBOT";

// L298N direction pins
#define IN1_GPIO 16
#define IN2_GPIO 17
#define IN3_GPIO 18
#define IN4_GPIO 19

// TCRT5000 analog outputs
#define TCRT_FRONT_GPIO 34 // ADC1_CH6
#define TCRT_BACK_GPIO  35 // ADC1_CH7
#define TCRT_FRONT_ADC_CHANNEL ADC1_CHANNEL_6
#define TCRT_BACK_ADC_CHANNEL  ADC1_CHANNEL_7

// HC-SR04 pins
#define TRIG_GPIO 21
#define ECHO_GPIO 32

// L298N enable pins driven by PWM
#define ENA_GPIO 22 // PWM channel A
#define ENB_GPIO 23 // PWM channel B

// PWM settings
#define LEDC_TIMER         LEDC_TIMER_0
#define LEDC_MODE          LEDC_HIGH_SPEED_MODE
#define LEDC_CHANNEL_A     LEDC_CHANNEL_0
#define LEDC_CHANNEL_B     LEDC_CHANNEL_1
#define LEDC_FREQUENCY_HZ  1000
#define LEDC_DUTY_BITS     LEDC_TIMER_8_BIT
#define DUTY_MAX           ((1 << LEDC_DUTY_BITS) - 1)
#define DUTY_HALF          (DUTY_MAX / 2)

// Configure spin orientation: if 1 -> left motor forward, right motor backward.
// If 0 -> left motor backward, right motor forward.
#define SPIN_LEFT_FORWARD 1

// TCRT thresholds (ADC raw 0..4095).
// In this prototype calibration, low raw values indicate the white edge.
#define TCRT_WHITE_RELEASE_THRESHOLD 2000
#define TCRT_HYSTERESIS             250
#define TCRT_WHITE_DETECT_THRESHOLD (TCRT_WHITE_RELEASE_THRESHOLD - TCRT_HYSTERESIS)

#define ADC_SAMPLES 4
#define HC_SR04_TIMEOUT_US 30000
#define OPPONENT_DETECTION_LIMIT_MM 150
#define CONTROL_LOOP_DELAY_MS 50

typedef enum {
    MOTOR_STOP = 0,
    MOTOR_FORWARD,
    MOTOR_BACKWARD,
    MOTOR_SPIN
} motor_state_t;

static motor_state_t current_state = MOTOR_STOP;

static const char *motor_state_to_string(motor_state_t state)
{
    switch (state) {
        case MOTOR_STOP:
            return "stop";
        case MOTOR_FORWARD:
            return "forward";
        case MOTOR_BACKWARD:
            return "backward";
        case MOTOR_SPIN:
            return "spin";
        default:
            return "unknown";
    }
}

static void set_motor_state(motor_state_t state)
{
    if (current_state != state) {
        current_state = state;
        ESP_LOGI(TAG, "Motor state -> %s", motor_state_to_string(state));
    }
}

static void init_pwm(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_BITS,
        .timer_num = LEDC_TIMER,
        .freq_hz = LEDC_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel_a = {
        .gpio_num = ENA_GPIO,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_A,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel_a);

    ledc_channel_config_t ledc_channel_b = {
        .gpio_num = ENB_GPIO,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_B,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel_b);
}

static void motor_set_duty(uint32_t duty_a, uint32_t duty_b)
{
    if (duty_a > DUTY_MAX) {
        duty_a = DUTY_MAX;
    }
    if (duty_b > DUTY_MAX) {
        duty_b = DUTY_MAX;
    }

    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_A, duty_a);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_A);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_B, duty_b);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_B);
}

static void motor_forward_duty(uint32_t duty)
{
    // IN1=1, IN2=0: left motor forward. IN3=1, IN4=0: right motor forward.
    gpio_set_level(IN1_GPIO, 1);
    gpio_set_level(IN2_GPIO, 0);
    gpio_set_level(IN3_GPIO, 1);
    gpio_set_level(IN4_GPIO, 0);
    motor_set_duty(duty, duty);
    set_motor_state(MOTOR_FORWARD);
}

static void motor_backward_duty(uint32_t duty)
{
    // IN1=0, IN2=1: left motor backward. IN3=0, IN4=1: right motor backward.
    gpio_set_level(IN1_GPIO, 0);
    gpio_set_level(IN2_GPIO, 1);
    gpio_set_level(IN3_GPIO, 0);
    gpio_set_level(IN4_GPIO, 1);
    motor_set_duty(duty, duty);
    set_motor_state(MOTOR_BACKWARD);
}

static void motor_stop(void)
{
    gpio_set_level(IN1_GPIO, 0);
    gpio_set_level(IN2_GPIO, 0);
    gpio_set_level(IN3_GPIO, 0);
    gpio_set_level(IN4_GPIO, 0);
    motor_set_duty(0, 0);
    set_motor_state(MOTOR_STOP);
}

static void motor_spin_opposite(uint32_t duty)
{
    // Spin in place: one motor forward, the other backward.
#if SPIN_LEFT_FORWARD
    gpio_set_level(IN1_GPIO, 1);
    gpio_set_level(IN2_GPIO, 0);
    gpio_set_level(IN3_GPIO, 0);
    gpio_set_level(IN4_GPIO, 1);
#else
    gpio_set_level(IN1_GPIO, 0);
    gpio_set_level(IN2_GPIO, 1);
    gpio_set_level(IN3_GPIO, 1);
    gpio_set_level(IN4_GPIO, 0);
#endif
    motor_set_duty(duty, duty);
    set_motor_state(MOTOR_SPIN);
}

// Returns distance in millimeters, or -1 on timeout.
static int32_t measure_distance_mm(void)
{
    gpio_set_level(TRIG_GPIO, 0);
    esp_rom_delay_us(2);
    gpio_set_level(TRIG_GPIO, 1);
    esp_rom_delay_us(10);
    gpio_set_level(TRIG_GPIO, 0);

    int64_t start = esp_timer_get_time();
    while (gpio_get_level(ECHO_GPIO) == 0) {
        if ((esp_timer_get_time() - start) > HC_SR04_TIMEOUT_US) {
            return -1;
        }
    }

    int64_t echo_start = esp_timer_get_time();
    while (gpio_get_level(ECHO_GPIO) == 1) {
        if ((esp_timer_get_time() - echo_start) > HC_SR04_TIMEOUT_US) {
            return -1;
        }
    }

    int64_t echo_end = esp_timer_get_time();
    int64_t pulse_width_us = echo_end - echo_start;

    // Speed of sound is approximately 343 m/s = 0.343 mm/us.
    return (int32_t)((pulse_width_us * 343) / 2000);
}

static int read_tcrt_avg(adc1_channel_t channel, int samples)
{
    if (samples <= 0) {
        samples = 1;
    }

    int64_t sum = 0;
    for (int i = 0; i < samples; ++i) {
        sum += adc1_get_raw(channel);
        esp_rom_delay_us(100);
    }

    return (int)(sum / samples);
}

static int front_is_white_state = 0;
static int back_is_white_state = 0;

static void update_tcrt_states(int front_raw, int back_raw)
{
    // Hysteresis keeps the previous state inside the transition band.
    // For this firmware, state 1 means the white edge was detected.
    if (front_raw >= TCRT_WHITE_RELEASE_THRESHOLD) {
        front_is_white_state = 0;
    } else if (front_raw <= TCRT_WHITE_DETECT_THRESHOLD) {
        front_is_white_state = 1;
    }

    if (back_raw >= TCRT_WHITE_RELEASE_THRESHOLD) {
        back_is_white_state = 0;
    } else if (back_raw <= TCRT_WHITE_DETECT_THRESHOLD) {
        back_is_white_state = 1;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32 sumo robot firmware");

    gpio_config_t motor_and_trigger_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << IN1_GPIO) |
                        (1ULL << IN2_GPIO) |
                        (1ULL << IN3_GPIO) |
                        (1ULL << IN4_GPIO) |
                        (1ULL << TRIG_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&motor_and_trigger_conf);

    gpio_config_t echo_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << ECHO_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&echo_conf);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(TCRT_FRONT_ADC_CHANNEL, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(TCRT_BACK_ADC_CHANNEL, ADC_ATTEN_DB_11);

    init_pwm();
    motor_stop();

    while (1) {
        int front_raw = read_tcrt_avg(TCRT_FRONT_ADC_CHANNEL, ADC_SAMPLES);
        int back_raw = read_tcrt_avg(TCRT_BACK_ADC_CHANNEL, ADC_SAMPLES);
        update_tcrt_states(front_raw, back_raw);

        int front_white = front_is_white_state;
        int back_white = back_is_white_state;

        ESP_LOGI(TAG, "TCRT raw F:%d B:%d state F:%d B:%d",
                 front_raw,
                 back_raw,
                 front_white,
                 back_white);

        if (front_white && back_white) {
            motor_stop();
            vTaskDelay(pdMS_TO_TICKS(CONTROL_LOOP_DELAY_MS));
            continue;
        }

        if (front_white && !back_white) {
            motor_backward_duty(DUTY_HALF);
            vTaskDelay(pdMS_TO_TICKS(CONTROL_LOOP_DELAY_MS));
            continue;
        }

        if (!front_white && back_white) {
            motor_forward_duty(DUTY_HALF);
            vTaskDelay(pdMS_TO_TICKS(CONTROL_LOOP_DELAY_MS));
            continue;
        }

        int32_t distance_mm = measure_distance_mm();
        if (distance_mm > 0 && distance_mm <= OPPONENT_DETECTION_LIMIT_MM) {
            motor_forward_duty(DUTY_MAX);
            vTaskDelay(pdMS_TO_TICKS(CONTROL_LOOP_DELAY_MS));
            continue;
        }

        if (!front_white && !back_white) {
            motor_spin_opposite(DUTY_HALF);
            vTaskDelay(pdMS_TO_TICKS(CONTROL_LOOP_DELAY_MS));
            continue;
        }

        motor_stop();
        vTaskDelay(pdMS_TO_TICKS(CONTROL_LOOP_DELAY_MS));
    }
}

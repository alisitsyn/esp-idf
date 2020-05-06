// Copyright 2015-2017 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

/**
 * @file esp_deep_sleep.h
 * @brief legacy definitions of esp_deep_sleep APIs
 *
 * This file provides compatibility for applications using esp_deep_sleep_* APIs.
 * New applications should use functions defined in "esp_sleep.h" instead.
 * These functions and types will be deprecated at some point.
 */

#include <stdint.h>
#include "esp_sleep.h"		// for esp_sleep_pd_domain_t
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enter deep sleep from wake stub with the configured wakeup options
 *
 * This function does not return.
 */
void esp_wake_stub_deep_sleep_start() __attribute__((noreturn));

/**
 * @brief Enter deep-sleep mode from deep sleep wake stub code 
 *
 * The device will automatically wake up after the deep-sleep time
 * Upon waking up, the device calls deep sleep wake stub, and then proceeds
 * to load application.
 *
 * Call to this function is equivalent to a call to esp_wake_stub_deep_sleep_enable_timer_wakeup
 * followed by a call to esp_wake_stub_deep_sleep_start.
 *
 * This function does not return.
 *
 * @param time_in_us  deep-sleep time, unit: microsecond
 */
void esp_wake_stub_deep_sleep(uint64_t time_in_us) __attribute__((noreturn));

/**
 * @brief Get the source which caused wakeup from sleep
 *
 * @return wakeup cause, or ESP_DEEP_SLEEP_WAKEUP_UNDEFINED if reset happened for reason other than deep sleep wakeup
 */
esp_sleep_wakeup_cause_t esp_wake_stub_sleep_get_wakeup_cause();

/**
 * @brief Enable wakeup using multiple pins
 *
 * This function uses external wakeup feature of RTC controller.
 * The function is similar to esp_sleep_enable_ext1_wakeup, but 
 * can be executed from deep sleep wake stub
 * 
 * This feature can monitor any number of pins which are in RTC IOs.
 * Once any of the selected pins goes into the state given by mode argument,
 * the chip will be woken up.
 *
 * @note This function does not modify pin configuration. The pins are
 *       configured in esp_sleep_start, immediately before
 *       entering sleep mode.
 *
 * @note This function does not change GPIO state it is assumed that they configured 
 *       by esp_sleep_enable_ext1_wakeup
 *
 * @param mask  bit mask of GPIO numbers which will cause wakeup. Only GPIOs
 *              which are have RTC functionality can be used in this bit map:
 *              0,2,4,12-15,25-27,32-39.
 * @param mode select logic function used to determine wakeup condition:
 *            - ESP_EXT1_WAKEUP_ALL_LOW: wake up when all selected GPIOs are low
 *            - ESP_EXT1_WAKEUP_ANY_HIGH: wake up when any of the selected GPIOs is high
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if any of the selected GPIOs is not an RTC GPIO,
 *        or mode is invalid
 */
esp_err_t esp_wake_stub_enable_ext1_wakeup(uint64_t mask, esp_sleep_ext1_wakeup_mode_t mode);

/**
 * @brief Enable wakeup using a pin
 *
 * This function uses external wakeup feature of RTC_IO peripheral.
 * It can be executed  from deep sleep wake stub code
 *
 * This feature can monitor any pin which is an RTC IO. Once the pin transitions
 * into the state given by level argument, the chip will be woken up.
 *
 * @note This function does not modify pin configuration. The pin is
 *       configured in esp_sleep_start, immediately before entering sleep mode.
 *
 * @note In revisions 0 and 1 of the ESP32, ext0 wakeup source
 *       can not be used together with touch or ULP wakeup sources.
 *
 * @param gpio_num  GPIO number used as wakeup source. Only GPIOs which are have RTC
 *             functionality can be used: 0,2,4,12-15,25-27,32-39.
 * @param level  input level which will trigger wakeup (0=low, 1=high)
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if the selected GPIO is not an RTC GPIO,
 *        or the mode is invalid
 *      - ESP_ERR_INVALID_STATE if wakeup triggers conflict
 */
esp_err_t esp_wake_stub_enable_ext0_wakeup(gpio_num_t gpio_num, int level);

// this function is shared just for debugging
uint64_t esp_wake_stub_get_sleep_time_us();

/**
 * @brief Disable wakeup source
 *
 * This function is used to deactivate wake up trigger for source
 * defined as parameter of the function from wake stub function executed
 * from first stage loader.
 *
 * @note This function does not modify wake up configuration in RTC.
 *       It will be performed in esp_sleep_start function.
 *
 * See docs/sleep-modes.rst for details.
 *
 * @param source - number of source to disable of type esp_sleep_source_t
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if trigger was not active
 */
esp_err_t esp_wake_stub_disable_wakeup_source(esp_sleep_source_t source);

/**
 * @brief Wait while transmission is in progress
 *
 * This function is waiting while uart transmission is not completed.
 *
 */
void esp_wake_stub_uart_tx_wait_idle(uint8_t uart_no);

#ifdef __cplusplus
}
#endif


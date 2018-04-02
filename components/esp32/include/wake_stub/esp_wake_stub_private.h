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

#include "esp_sleep.h"		// for type esp_sleep_pd_domain_t and other shared types and defines

#ifdef __cplusplus
extern "C" {
#endif

/*
typedef esp_sleep_pd_domain_t esp_deep_sleep_pd_domain_t;
typedef esp_sleep_pd_option_t esp_deep_sleep_pd_option_t;
typedef esp_sleep_ext1_wakeup_mode_t esp_ext1_wakeup_mode_t;
typedef esp_sleep_wakeup_cause_t esp_deep_sleep_wakeup_cause_t;
*/

/*
#define ESP_DEEP_SLEEP_WAKEUP_UNDEFINED     ESP_SLEEP_WAKEUP_UNDEFINED
#define ESP_DEEP_SLEEP_WAKEUP_EXT0          ESP_SLEEP_WAKEUP_EXT0
#define ESP_DEEP_SLEEP_WAKEUP_EXT1          ESP_SLEEP_WAKEUP_EXT1
#define ESP_DEEP_SLEEP_WAKEUP_TIMER         ESP_SLEEP_WAKEUP_TIMER
#define ESP_DEEP_SLEEP_WAKEUP_TOUCHPAD      ESP_SLEEP_WAKEUP_TOUCHPAD
#define ESP_DEEP_SLEEP_WAKEUP_ULP           ESP_SLEEP_WAKEUP_ULP
*/

#define WAKESTUB_DEBUG_OPTION 1

#define RTC_DBG_PRINT(str, ...) { \
    static RTC_RODATA_ATTR const char str_fmt[] = str; \
    ets_printf(str_fmt, __VA_ARGS__); \
    while (REG_GET_FIELD(UART_STATUS_REG(0), UART_ST_UTX_OUT)); \
 }

/*
 * Internal structure which holds all requested deep sleep parameters
 */
typedef struct {
    esp_sleep_pd_option_t pd_options[ESP_PD_DOMAIN_MAX];
    uint32_t wake_stub_addr;
    uint32_t rtc_int_flags;
    uint64_t sleep_duration;
    uint64_t rtc_crc_reg;
    //uint64_t start_rtc_count;    // used for time debugging of RTC counter
    //uint64_t end_rtc_count;
    uint32_t init_flag;

    uint32_t wakeup_triggers : 11;
    uint32_t ext1_trigger_mode : 1;
    uint32_t ext1_rtc_gpio_mask : 18;
    uint32_t ext0_trigger_level : 1;
    uint32_t ext0_rtc_gpio_num : 5;
} wakestub_deep_sleep_config_t;

extern wakestub_deep_sleep_config_t wsapi_s_config;

// Helper functions used by wake stub API
uint64_t RTC_IRAM_ATTR wsapi_get_sleep_time_us();
void RTC_IRAM_ATTR wsapi_ext0_wakeup_prepare();
void RTC_IRAM_ATTR wsapi_ext1_wakeup_prepare();
void RTC_IRAM_ATTR wsapi_timer_wakeup_prepare();
uint32_t RTC_IRAM_ATTR wsapi_rtc_sleep_start(uint32_t wakeup_opt, uint32_t reject_opt);
uint32_t RTC_IRAM_ATTR wsapi_get_power_down_flags();
esp_err_t RTC_IRAM_ATTR wsapi_update_wakeup_options();
void RTC_IRAM_ATTR wsapi_init_default_rtc();
uint64_t RTC_IRAM_ATTR wsapi_rtc_time_get();
uint32_t RTC_IRAM_ATTR wsapi_clk_slowclk_cal_get();

#ifdef __cplusplus
}
#endif


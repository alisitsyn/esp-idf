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
#include "esp_log.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
#define RTC_STR(str) (__extension__({static const RTC_RODATA_ATTR char _fmt[] = (str); (const char *)&_fmt;}))
#define RTC_LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter format LOG_RESET_COLOR "\n"

#define ESP_RTC_LOG( level, format, ... )  if (LOG_LOCAL_LEVEL >= level) { ets_printf(RTC_STR(format), ##__VA_ARGS__); \
                                                                            esp_wake_stub_uart_tx_wait_idle(0); }
#define ESP_RTC_LOGE( format, ... )  ESP_RTC_LOG(ESP_LOG_ERROR, RTC_LOG_FORMAT(E, format), ##__VA_ARGS__)
#define ESP_RTC_LOGW( format, ... )  ESP_RTC_LOG(ESP_LOG_WARN, RTC_LOG_FORMAT(W, format), ##__VA_ARGS__)
#define ESP_RTC_LOGI( format, ... )  ESP_RTC_LOG(ESP_LOG_INFO, RTC_LOG_FORMAT(I, format), ##__VA_ARGS__)
#define ESP_RTC_LOGD( format, ... )  ESP_RTC_LOG(ESP_LOG_DEBUG, RTC_LOG_FORMAT(D, format), ##__VA_ARGS__)
#define ESP_RTC_LOGV( format, ... )  ESP_RTC_LOG(ESP_LOG_VERBOSE, RTC_LOG_FORMAT(V, format), ##__VA_ARGS__)
*/

/*
 * Internal structure which holds all requested deep sleep parameters
 */
typedef struct {
    esp_sleep_pd_option_t pd_options[ESP_PD_DOMAIN_MAX];
    uint32_t wake_stub_addr;
    uint32_t rtc_int_flags;
    uint64_t sleep_duration;
    uint64_t rtc_crc_reg;
    uint32_t init_flag;

    uint32_t wakeup_triggers : 11;
    uint32_t ext1_trigger_mode : 1;
    uint32_t ext1_rtc_gpio_mask : 18;
    uint32_t ext0_trigger_level : 1;
    uint32_t ext0_rtc_gpio_num : 5;
} wakestub_deep_sleep_config_t;

wakestub_deep_sleep_config_t wake_stub_s_config;

// Helper functions used by wake stub API
uint64_t wake_stub_get_sleep_time_us();
void wake_stub_ext0_wakeup_prepare();
void wake_stub_ext1_wakeup_prepare();
void wake_stub_timer_wakeup_prepare();
uint32_t wake_stub_rtc_sleep_start(uint32_t wakeup_opt, uint32_t reject_opt);
uint32_t wake_stub_get_power_down_flags();
esp_err_t wake_stub_update_wakeup_options();
void wake_stub_init_default_rtc();
uint64_t wake_stub_rtc_time_get();
uint32_t wake_stub_clk_slowclk_cal_get();
void wake_stub_uart_tx_wait_idle(uint8_t uart_no);

#ifdef __cplusplus
}
#endif


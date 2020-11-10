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

#include <stddef.h>
#include <sys/lock.h>
#include <sys/param.h>
#include "esp_attr.h"                   // for RTC_IRAM_ATTR attribute
#include "esp_err.h"
#include "soc/rtc.h"
#include "soc/rtc_io_reg.h"
#include "soc/rtc_cntl_reg.h"

#ifdef CONFIG_IDF_TARGET_ESP32
#include "esp32/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/rtc.h"
#endif

#include "esp_wake_stub_private.h"

esp_err_t esp_wake_stub_enable_timer_wakeup(uint64_t time_in_us)
{
    // Get current wakeup options from RTC registers
    wake_stub_update_wakeup_options();

    // Set options for timer wakeup
    wake_stub_s_config.wakeup_triggers |= RTC_TIMER_TRIG_EN;
    wake_stub_s_config.sleep_duration = time_in_us;
    return ESP_OK;
}

void esp_wake_stub_deep_sleep(uint64_t time_in_us)
{
    // Update configuration
    wake_stub_update_wakeup_options();

    CLEAR_PERI_REG_MASK(RTC_CNTL_STATE0_REG, RTC_CNTL_SLEEP_EN);
    esp_wake_stub_enable_timer_wakeup(time_in_us);
    
    // Prepare RTC timer for wakeup
    wake_stub_timer_wakeup_prepare();

    // Set wakeup sources and go to sleep
    wake_stub_rtc_sleep_start(wake_stub_s_config.wakeup_triggers, 0);
}

uint32_t wake_stub_clk_slowclk_cal_get()
{
    // Get RTC_SLOW_CLK calibration value saved in the register
    return REG_READ(RTC_SLOW_CLK_CAL_REG);
}

uint64_t wake_stub_rtc_time_get()
{
    // update according to targets rtc_time_get() !!!!!!!!!!!!!!!!!!!

    SET_PERI_REG_MASK(RTC_CNTL_TIME_UPDATE_REG, RTC_CNTL_TIME_UPDATE);
#ifdef CONFIG_IDF_TARGET_ESP32
    while (GET_PERI_REG_MASK(RTC_CNTL_TIME_UPDATE_REG, RTC_CNTL_TIME_VALID) == 0) {
        ets_delay_us(1); // might take 1 RTC slowclk period, don't flood RTC bus
    }
    SET_PERI_REG_MASK(RTC_CNTL_INT_CLR_REG, RTC_CNTL_TIME_VALID_INT_CLR);
#elif CONFIG_IDF_TARGET_ESP32S2
    // Place code here to get timer value
#endif
    uint64_t t = READ_PERI_REG(RTC_CNTL_TIME0_REG);
    t |= ((uint64_t) READ_PERI_REG(RTC_CNTL_TIME1_REG)) << 32;
    return t;
}

void wake_stub_rtc_sleep_set_wakeup_time(uint64_t time)
{
    // change to rtc_cntl_ll_set_wakeup_timer(uint64_t t)
    // rtc_hal_set_wakeup_timer
    WRITE_PERI_REG(RTC_CNTL_SLP_TIMER0_REG, time & UINT32_MAX);
    WRITE_PERI_REG(RTC_CNTL_SLP_TIMER1_REG, time >> 32);
#if CONFIG_IDF_TARGET_ESP32S2
    SET_PERI_REG_MASK(RTC_CNTL_INT_CLR_REG, RTC_CNTL_MAIN_TIMER_INT_CLR_M);
    SET_PERI_REG_MASK(RTC_CNTL_SLP_TIMER1_REG, RTC_CNTL_MAIN_TIMER_ALARM_EN_M);
#endif
}

void wake_stub_timer_wakeup_prepare()
{
    // Update wakeup options from RTC if needed 
    wake_stub_update_wakeup_options();
    
    // Get microseconds per RTC clock tick (scaled by 2^19)
    uint32_t slow_clk_value = wake_stub_clk_slowclk_cal_get();
    
    // Calculate number of RTC clock ticks until wakeup
    uint64_t rtc_count_delta = ((wake_stub_s_config.sleep_duration * \
                                (1 << RTC_CLK_CAL_FRACT)) / slow_clk_value);
    
    // Get current count
    uint64_t rtc_curr_count = wake_stub_rtc_time_get();

    wake_stub_rtc_sleep_set_wakeup_time(rtc_curr_count + rtc_count_delta);
}

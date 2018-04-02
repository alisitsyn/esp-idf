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
#include "esp_attr.h"            // for RTC_IRAM_ATTR attribute
#include "esp_err.h"
#include "rom/cache.h"
#include "rom/rtc.h"
#include "rom/uart.h"
#include "soc/cpu.h"
#include "soc/rtc.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/timer_group_reg.h"     // for TIMG_WDTFEED_REG
#include "soc/dport_reg.h"        // for __DPORT_REG

#include "wake_stub/esp_wake_stub_private.h"

void RTC_IRAM_ATTR wsapi_timer_wakeup_prepare();

esp_err_t RTC_IRAM_ATTR esp_wsapi_enable_timer_wakeup(uint64_t time_in_us)
{
    // Get current wakeup options from RTC registers
    wsapi_update_wakeup_options();

    // Set options for timer wakeup
    wsapi_s_config.wakeup_triggers |= RTC_TIMER_TRIG_EN;
    wsapi_s_config.sleep_duration = time_in_us;
    return ESP_OK;
}

void RTC_IRAM_ATTR esp_wsapi_deep_sleep(uint64_t time_in_us)
{
    // Update configuration
    wsapi_update_wakeup_options();

    CLEAR_PERI_REG_MASK(RTC_CNTL_STATE0_REG, RTC_CNTL_SLEEP_EN);
    esp_wsapi_enable_timer_wakeup(time_in_us);
    
    // Preapare RTC timer for wakeup
    wsapi_timer_wakeup_prepare();

    // Set wakeup sources and go to sleep
    wsapi_rtc_sleep_start(wsapi_s_config.wakeup_triggers, 0);
}

inline __attribute__((always_inline))
uint32_t RTC_IRAM_ATTR wsapi_clk_slowclk_cal_get()
{
    // Get RTC_SLOW_CLK calibration value saved in the register
    return REG_READ(RTC_SLOW_CLK_CAL_REG);
}

inline __attribute__((always_inline))
uint64_t RTC_IRAM_ATTR wsapi_time_us_to_slowclk(uint64_t time_in_us, uint32_t period)
{
    /* Overflow will happen in this function if time_in_us >= 2^45, which is about 400 days.
     * TODO: fix overflow.
     period = number of microseconds per RTC tick scaled by 2^19
     */
    return ((time_in_us * (1 << RTC_CLK_CAL_FRACT)) / period);
}

inline __attribute__((always_inline))
uint64_t RTC_IRAM_ATTR wsapi_rtc_time_get()
{
    SET_PERI_REG_MASK(RTC_CNTL_TIME_UPDATE_REG, RTC_CNTL_TIME_UPDATE);
    while (GET_PERI_REG_MASK(RTC_CNTL_TIME_UPDATE_REG, RTC_CNTL_TIME_VALID) == 0) {
        ets_delay_us(1); // might take 1 RTC slowclk period, don't flood RTC bus
    }
    
    SET_PERI_REG_MASK(RTC_CNTL_INT_CLR_REG, RTC_CNTL_TIME_VALID_INT_CLR);
    uint64_t t = READ_PERI_REG(RTC_CNTL_TIME0_REG);
    t |= ((uint64_t) READ_PERI_REG(RTC_CNTL_TIME1_REG)) << 32;
    return t;
}

inline __attribute__((always_inline))
void RTC_IRAM_ATTR wsapi_rtc_sleep_set_wakeup_time(uint64_t time)
{
    WRITE_PERI_REG(RTC_CNTL_SLP_TIMER0_REG, time & UINT32_MAX);
    WRITE_PERI_REG(RTC_CNTL_SLP_TIMER1_REG, time >> 32);
}

inline __attribute__((always_inline))
void RTC_IRAM_ATTR wsapi_timer_wakeup_prepare()
{
    // Update wakeup options from RTC if needed 
    wsapi_update_wakeup_options();
    
    // Get microseconds per RTC clock tick (scaled by 2^19)
    uint32_t slow_clk_value = wsapi_clk_slowclk_cal_get();
    
    // Calculate number of RTC clock ticks until wakeup
    uint64_t rtc_count_delta = ((wsapi_s_config.sleep_duration * \
                                (1 << RTC_CLK_CAL_FRACT)) / slow_clk_value); 
    //wsapi_time_us_to_slowclk(wsapi_s_config.sleep_duration, slow_clk_value);
    
    // Get current count
    uint64_t rtc_curr_count = wsapi_rtc_time_get();

    wsapi_rtc_sleep_set_wakeup_time(rtc_curr_count + rtc_count_delta);
}

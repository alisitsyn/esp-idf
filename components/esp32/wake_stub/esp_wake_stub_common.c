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

#include "esp_attr.h"
#include "esp_err.h"

#include "esp_sleep.h"
#include "rom/cache.h"
#include "rom/rtc.h"
#include "rom/uart.h"
#include "soc/cpu.h"
#include "soc/rtc.h"
#include "rom/rtc.h"            // for rtc storage registers
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
/*
#include "soc/spi_reg.h"
#include "soc/sens_reg.h"
*/
#include "soc/dport_reg.h"        // for __DPORT_REG
#include "wake_stub/esp_wake_stub_private.h"

#define WAKE_STUB_CONF_ACTIVE_MARKER 0x55555555

wakestub_deep_sleep_config_t wsapi_s_config = {
    .pd_options = { ESP_PD_OPTION_AUTO, ESP_PD_OPTION_AUTO, ESP_PD_OPTION_AUTO },
    .wakeup_triggers = 0
};

// These are for debugging to control time since last sleep in microseconds
static RTC_DATA_ATTR uint64_t rtc_start_count = 0;
static RTC_DATA_ATTR uint64_t rtc_stop_count = 0;

uint64_t RTC_IRAM_ATTR esp_wsapi_get_sleep_time_us()
{
    // Get number of timer ticks

    // These are saved to control time since last sleep for debugging
    rtc_start_count = rtc_stop_count; 
    rtc_stop_count = wsapi_rtc_time_get();
    
    const uint64_t ticks = (rtc_stop_count - rtc_start_count);
    // Get calibration value (uS per tick)
    const uint32_t cal = wsapi_clk_slowclk_cal_get();

    // Calculate time 
    const uint64_t ticks_low = ticks & UINT32_MAX;
    const uint64_t ticks_high = ticks >> 32;
    return ((ticks_low * cal) >> RTC_CLK_CAL_FRACT) +
            ((ticks_high * cal) << (32 - RTC_CLK_CAL_FRACT));
}

// Update local options from RTC set by deep sleep api calls 
// before went to deep sleep mode
esp_err_t RTC_IRAM_ATTR wsapi_update_wakeup_options()
{
    esp_err_t result = ESP_OK;

    if (wsapi_s_config.init_flag != WAKE_STUB_CONF_ACTIVE_MARKER)
    {

        // Get wakeup sources from RTC wakeup star register
        wsapi_s_config.wakeup_triggers = REG_GET_FIELD(RTC_CNTL_WAKEUP_STATE_REG, \
                                                        RTC_CNTL_WAKEUP_ENA);
        // Get ext0 options from RTC registers
        wsapi_s_config.ext0_rtc_gpio_num = REG_GET_FIELD(RTC_IO_EXT_WAKEUP0_REG, \
                                                        RTC_IO_EXT_WAKEUP0_SEL);
        wsapi_s_config.ext0_trigger_level = GET_PERI_REG_BITS2(RTC_CNTL_EXT_WAKEUP_CONF_REG, \
                                                                RTC_CNTL_EXT_WAKEUP0_LV_V, \
                                                                RTC_CNTL_EXT_WAKEUP0_LV_S);
        // Get ext1 options from RTC registers
        wsapi_s_config.ext1_rtc_gpio_mask = REG_GET_FIELD(RTC_CNTL_EXT_WAKEUP1_REG, \
                                                        RTC_CNTL_EXT_WAKEUP1_SEL);
        wsapi_s_config.ext1_trigger_mode = \
                        GET_PERI_REG_BITS2(RTC_CNTL_EXT_WAKEUP_CONF_REG, \
                                        RTC_CNTL_EXT_WAKEUP1_LV_V, RTC_CNTL_EXT_WAKEUP1_LV_S);
        // Get wake stub address from RTC registers (for debugging)
        wsapi_s_config.wake_stub_addr = REG_READ(RTC_ENTRY_ADDR_REG);
    
        wsapi_s_config.rtc_int_flags = REG_READ(RTC_CNTL_INT_ENA_REG);
        wsapi_s_config.rtc_crc_reg = REG_READ(RTC_CNTL_STORE7_REG); 

        // Save timer counter to measure time in sleep
        //rtc_stop_count = wsapi_rtc_time_get();
        
#ifdef WAKESTUB_DEBUG_OPTION
        RTC_DBG_PRINT("wakeup_trig: 0x%.4x\n", wsapi_s_config.wakeup_triggers);
        RTC_DBG_PRINT("ext0_gpio_num: 0x%.4x\n", wsapi_s_config.ext0_rtc_gpio_num);
        RTC_DBG_PRINT("ext0_trigger_level: 0x%.4x\n", wsapi_s_config.ext0_trigger_level);
        RTC_DBG_PRINT("ext1_rtc_gpio_mask: 0x%.4x\n", wsapi_s_config.ext1_rtc_gpio_mask);
        RTC_DBG_PRINT("ext1_trigger_mode: 0x%.4x\n", wsapi_s_config.ext1_trigger_mode);
        RTC_DBG_PRINT("wake_stub_addr: 0x%x\n", wsapi_s_config.wake_stub_addr);
        RTC_DBG_PRINT("rtc_int_flags: 0x%x\n", wsapi_s_config.rtc_int_flags);
        //RTC_DBG_PRINT("rtc_crc: 0x%x\n", wsapi_s_config.rtc_crc_reg);
        //RTC_DBG_PRINT("Sleep_time: %u\n", (uint32_t)wsapi_get_sleep_time());
#endif
        wsapi_s_config.init_flag = WAKE_STUB_CONF_ACTIVE_MARKER; // Set initialization marker
        result = ESP_ERR_NOT_FOUND;
    }
    return result;
}

uint32_t RTC_IRAM_ATTR wsapi_get_power_down_flags()
{
    // Where needed, convert AUTO options to ON. Later interpret AUTO as OFF.

    // RTC_SLOW_MEM is needed for the ULP, so keep RTC_SLOW_MEM powered up if ULP
    // is used and RTC_SLOW_MEM is Auto.
    // If there is any data placed into .rtc.data or .rtc.bss segments, and
    // RTC_SLOW_MEM is Auto, keep it powered up as well.

    // These labels are defined in the linker script:
    extern int _rtc_data_start, _rtc_data_end, _rtc_bss_start, _rtc_bss_end;

    if (wsapi_s_config.pd_options[ESP_PD_DOMAIN_RTC_SLOW_MEM] == ESP_PD_OPTION_AUTO ||
            &_rtc_data_end > &_rtc_data_start ||
            &_rtc_bss_end > &_rtc_bss_start) {
        wsapi_s_config.pd_options[ESP_PD_DOMAIN_RTC_SLOW_MEM] = ESP_PD_OPTION_ON;
    }

    // RTC_FAST_MEM is needed for deep sleep stub.
    // If RTC_FAST_MEM is Auto, keep it powered on, so that deep sleep stub
    // can run.
    // In the new chip revision, deep sleep stub will be optional,
    // and this can be changed.
    if (wsapi_s_config.pd_options[ESP_PD_DOMAIN_RTC_FAST_MEM] == ESP_PD_OPTION_AUTO) {
        wsapi_s_config.pd_options[ESP_PD_DOMAIN_RTC_FAST_MEM] = ESP_PD_OPTION_ON;
    }

    // RTC_PERIPH is needed for EXT0 wakeup.
    // If RTC_PERIPH is auto, and EXT0 isn't enabled, power down RTC_PERIPH.
    if (wsapi_s_config.pd_options[ESP_PD_DOMAIN_RTC_PERIPH] == ESP_PD_OPTION_AUTO) {
        if (wsapi_s_config.wakeup_triggers & RTC_EXT0_TRIG_EN) {
            wsapi_s_config.pd_options[ESP_PD_DOMAIN_RTC_PERIPH] = ESP_PD_OPTION_ON;
        } else if (wsapi_s_config.wakeup_triggers & (RTC_TOUCH_TRIG_EN | RTC_ULP_TRIG_EN)) {
            // In both rev. 0 and rev. 1 of ESP32, forcing power up of RTC_PERIPH
            // prevents ULP timer and touch FSMs from working correctly.
            wsapi_s_config.pd_options[ESP_PD_DOMAIN_RTC_PERIPH] = ESP_PD_OPTION_OFF;
        }
    }

    // Prepare flags based on the selected options
    uint32_t pd_flags = 0;
    if (wsapi_s_config.pd_options[ESP_PD_DOMAIN_RTC_FAST_MEM] != ESP_PD_OPTION_ON) {
        pd_flags |= RTC_SLEEP_PD_RTC_FAST_MEM;
    }
    if (wsapi_s_config.pd_options[ESP_PD_DOMAIN_RTC_SLOW_MEM] != ESP_PD_OPTION_ON) {
        pd_flags |= RTC_SLEEP_PD_RTC_SLOW_MEM;
    }
    if (wsapi_s_config.pd_options[ESP_PD_DOMAIN_RTC_PERIPH] != ESP_PD_OPTION_ON) {
        pd_flags |= RTC_SLEEP_PD_RTC_PERIPH;
    }
    return pd_flags;
}

esp_sleep_wakeup_cause_t RTC_IRAM_ATTR esp_wsapi_sleep_get_wakeup_cause()
{
    uint32_t wakeup_cause = REG_GET_FIELD(RTC_CNTL_WAKEUP_STATE_REG, RTC_CNTL_WAKEUP_CAUSE);
    if (wakeup_cause & RTC_EXT0_TRIG_EN) {
        return ESP_SLEEP_WAKEUP_EXT0;
    } else if (wakeup_cause & RTC_EXT1_TRIG_EN) {
        return ESP_SLEEP_WAKEUP_EXT1;
    } else if (wakeup_cause & RTC_TIMER_TRIG_EN) {
        return ESP_SLEEP_WAKEUP_TIMER;
    } else if (wakeup_cause & RTC_TOUCH_TRIG_EN) {
        return ESP_SLEEP_WAKEUP_TOUCHPAD;
    } else if (wakeup_cause & RTC_ULP_TRIG_EN) {
        return ESP_SLEEP_WAKEUP_ULP;
    } else {
        return ESP_SLEEP_WAKEUP_UNDEFINED;
    }
}

inline __attribute__((always_inline))
uint32_t RTC_IRAM_ATTR wsapi_rtc_sleep_start(uint32_t wakeup_opt, uint32_t reject_opt)
{
    REG_SET_FIELD(RTC_CNTL_WAKEUP_STATE_REG, RTC_CNTL_WAKEUP_ENA, wakeup_opt);
    WRITE_PERI_REG(RTC_CNTL_SLP_REJECT_CONF_REG, reject_opt);

    /* Start entry into sleep mode */
    CLEAR_PERI_REG_MASK(RTC_CNTL_STATE0_REG, RTC_CNTL_SLEEP_EN);
    SET_PERI_REG_MASK(RTC_CNTL_STATE0_REG, RTC_CNTL_SLEEP_EN);

    while (GET_PERI_REG_MASK(RTC_CNTL_INT_RAW_REG,
            RTC_CNTL_SLP_REJECT_INT_RAW | RTC_CNTL_SLP_WAKEUP_INT_RAW) == 0) {
        ;
    }

    // In deep sleep mode, we never get here
     uint32_t reject = REG_GET_FIELD(RTC_CNTL_INT_RAW_REG, RTC_CNTL_SLP_REJECT_INT_RAW);
     SET_PERI_REG_MASK(RTC_CNTL_INT_CLR_REG,
            RTC_CNTL_SLP_REJECT_INT_CLR | RTC_CNTL_SLP_WAKEUP_INT_CLR);

    return reject;
}


static inline RTC_IRAM_ATTR void wsapi_uart_tx_wait_idle(uint8_t uart_no) {
    while(REG_GET_FIELD(UART_STATUS_REG(uart_no), UART_ST_UTX_OUT)) {
        ;
    }
}

static uint32_t RTC_IRAM_ATTR esp_wsapi_sleep_start(uint32_t pd_flags)
{
    // Flush UARTs so that output is not lost due to APB frequency change
    wsapi_uart_tx_wait_idle(0);

    // Configure pins for external wakeup
    if (wsapi_s_config.wakeup_triggers & RTC_EXT0_TRIG_EN) {
        wsapi_ext0_wakeup_prepare();
    } else if (wsapi_s_config.wakeup_triggers & RTC_EXT1_TRIG_EN) {
        wsapi_ext1_wakeup_prepare();
    } else if (wsapi_s_config.wakeup_triggers & RTC_ULP_TRIG_EN) {
        // Enable ULP wakeup
        SET_PERI_REG_MASK(RTC_CNTL_STATE0_REG, RTC_CNTL_ULP_CP_WAKEUP_FORCE_EN);
    } else if ((wsapi_s_config.wakeup_triggers & RTC_TIMER_TRIG_EN) &&
            wsapi_s_config.sleep_duration > 0) {
        // Configure timer wakeup
        wsapi_timer_wakeup_prepare();
    }

    // Enter sleep
    // Skip initialization of RTC config,
    // it is assumed initialization is done from regular deep sleep api
    // update just wake up registers
    //rtc_sleep_config_t config = RTC_SLEEP_CONFIG_DEFAULT(pd_flags);
    //wsapi_rtc_sleep_init(config);
    return wsapi_rtc_sleep_start(wsapi_s_config.wakeup_triggers, 0);
}

void RTC_IRAM_ATTR esp_wsapi_deep_sleep_start()
{
    // Get options from RTC registers 
    wsapi_update_wakeup_options();

    // Decide which power domains can be powered down
    uint32_t pd_flags = wsapi_get_power_down_flags();

    // Enter sleep
    esp_wsapi_sleep_start(RTC_SLEEP_PD_DIG | pd_flags); //RTC_SLEEP_PD_VDDSDIO

    // Because RTC is in a slower clock domain than the CPU, it
    // can take several CPU cycles for the sleep mode to start.
    while (1) {
        ;
    }
}



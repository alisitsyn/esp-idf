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
#include "esp_sleep.h"
#include "rom/cache.h"
#include "rom/rtc.h"
#include "rom/uart.h"
#include "soc/cpu.h"
#include "soc/rtc.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/rtc_gpio_channel.h"   // for RTC gpio pin numbers

#include "soc/dport_reg.h"          // for __DPORT_REG debugging support

#include "sdkconfig.h"
#include "wake_stub/esp_wake_stub_private.h"

#define INVALID_GPIO 0xFF

typedef struct {
    uint8_t gpio_num;
    uint8_t rtc_gpio_num;
} rtc_gpio_num_t;


// Lookup table to get rtc_gpio_num for gpio_num
static RTC_RODATA_ATTR const rtc_gpio_num_t gpio_lookup_table[] = {
    { RTCIO_GPIO36_CHANNEL, RTCIO_CHANNEL_0_GPIO_NUM },
    { RTCIO_GPIO37_CHANNEL, RTCIO_CHANNEL_1_GPIO_NUM },
    { RTCIO_GPIO38_CHANNEL, RTCIO_CHANNEL_2_GPIO_NUM },
    { RTCIO_GPIO39_CHANNEL, RTCIO_CHANNEL_3_GPIO_NUM },
    { RTCIO_GPIO34_CHANNEL, RTCIO_CHANNEL_4_GPIO_NUM },
    { RTCIO_GPIO35_CHANNEL, RTCIO_CHANNEL_5_GPIO_NUM }, 
    { RTCIO_GPIO25_CHANNEL, RTCIO_CHANNEL_6_GPIO_NUM },
    { RTCIO_GPIO26_CHANNEL, RTCIO_CHANNEL_7_GPIO_NUM },
    { RTCIO_GPIO33_CHANNEL, RTCIO_CHANNEL_8_GPIO_NUM },
    { RTCIO_GPIO32_CHANNEL, RTCIO_CHANNEL_9_GPIO_NUM },
    { RTCIO_GPIO4_CHANNEL, RTCIO_CHANNEL_10_GPIO_NUM },
    { RTCIO_GPIO0_CHANNEL, RTCIO_CHANNEL_11_GPIO_NUM },
    { RTCIO_GPIO2_CHANNEL, RTCIO_CHANNEL_12_GPIO_NUM },
    { RTCIO_GPIO15_CHANNEL, RTCIO_CHANNEL_13_GPIO_NUM },
    { RTCIO_GPIO13_CHANNEL, RTCIO_CHANNEL_14_GPIO_NUM },
    { RTCIO_GPIO12_CHANNEL, RTCIO_CHANNEL_15_GPIO_NUM },
    { RTCIO_GPIO14_CHANNEL, RTCIO_CHANNEL_16_GPIO_NUM },
    { RTCIO_GPIO27_CHANNEL, RTCIO_CHANNEL_17_GPIO_NUM }
};

#define GPIO_LOOKUP_SIZE (sizeof(gpio_lookup_table)/sizeof(gpio_lookup_table[0]))

// The helper function to get RTC gpio for gpio number
RTC_IRAM_ATTR uint8_t wsapi_get_rtc_gpio(uint8_t gpio_num)
{
    for (int gpio = 0; (gpio < GPIO_LOOKUP_SIZE); ++gpio) {
        if ((gpio_lookup_table[gpio].gpio_num == gpio_num) ) {
            return gpio_lookup_table[gpio].rtc_gpio_num;
        }
    }
    return INVALID_GPIO;
}

RTC_IRAM_ATTR void wsapi_ext0_wakeup_prepare()
{
    int rtc_gpio_num = wsapi_s_config.ext0_rtc_gpio_num;
    // Set GPIO to be used for wakeup
    REG_SET_FIELD(RTC_IO_EXT_WAKEUP0_REG, RTC_IO_EXT_WAKEUP0_SEL, rtc_gpio_num);
    // Set level which will trigger wakeup
    SET_PERI_REG_BITS(RTC_CNTL_EXT_WAKEUP_CONF_REG, 0x1,
            wsapi_s_config.ext0_trigger_level, RTC_CNTL_EXT_WAKEUP0_LV_S);
}

RTC_IRAM_ATTR esp_err_t esp_wsapi_enable_ext0_wakeup(gpio_num_t gpio_num, int level)
{
    esp_err_t result = ESP_ERR_INVALID_ARG;
    uint8_t rtc_gpio_num = wsapi_get_rtc_gpio(gpio_num);
    if (rtc_gpio_num != INVALID_GPIO) {
        wsapi_s_config.ext0_rtc_gpio_num = rtc_gpio_num;
        wsapi_s_config.ext0_trigger_level = level;
        wsapi_s_config.wakeup_triggers |= RTC_EXT0_TRIG_EN;
        result = ESP_OK;
    }
    return result;
}

RTC_IRAM_ATTR esp_err_t esp_wsapi_enable_ext1_wakeup(uint64_t mask, esp_sleep_ext1_wakeup_mode_t mode)
{
    if (mode > ESP_EXT1_WAKEUP_ANY_HIGH) {
        return ESP_ERR_INVALID_ARG;
    }
    wsapi_s_config.ext1_rtc_gpio_mask = mask;
    wsapi_s_config.ext1_trigger_mode = mode;
    wsapi_s_config.wakeup_triggers |= RTC_EXT1_TRIG_EN;
    return ESP_OK;
}

RTC_IRAM_ATTR void wsapi_ext1_wakeup_prepare()
{
    // Clear state from previous wakeup
    REG_SET_BIT(RTC_CNTL_EXT_WAKEUP1_REG, RTC_CNTL_EXT_WAKEUP1_STATUS_CLR);
    // Set pins to be used for wakeup
    REG_SET_FIELD(RTC_CNTL_EXT_WAKEUP1_REG, RTC_CNTL_EXT_WAKEUP1_SEL, \
                                            wsapi_s_config.ext1_rtc_gpio_mask);
    // Set logic function (any low, all high)
    SET_PERI_REG_BITS(RTC_CNTL_EXT_WAKEUP_CONF_REG, 0x1,
                    wsapi_s_config.ext1_trigger_mode, RTC_CNTL_EXT_WAKEUP1_LV_S);
}

/*
esp_err_t esp_sleep_enable_ulp_wakeup()
{
#ifdef CONFIG_ULP_COPROC_ENABLED
    if(wsapi_s_config.wakeup_triggers & RTC_EXT0_TRIG_EN) {
        ESP_LOGE(TAG, "Conflicting wake-up trigger: ext0");
        return ESP_ERR_INVALID_STATE;
    }
    wsapi_s_config.wakeup_triggers |= RTC_ULP_TRIG_EN;
    return ESP_OK;
#else
    return ESP_ERR_INVALID_STATE;
#endif
}

esp_err_t esp_sleep_enable_touchpad_wakeup()
{
    if (wsapi_s_config.wakeup_triggers & (RTC_EXT0_TRIG_EN)) {
        ESP_LOGE(TAG, "Conflicting wake-up trigger: ext0");
        return ESP_ERR_INVALID_STATE;
    }
    wsapi_s_config.wakeup_triggers |= RTC_TOUCH_TRIG_EN;
    return ESP_OK;
}

touch_pad_t esp_sleep_get_touchpad_wakeup_status()
{
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TOUCHPAD) {
        return TOUCH_PAD_MAX;
    }
    uint32_t touch_mask = REG_GET_FIELD(SENS_SAR_TOUCH_CTRL2_REG, SENS_TOUCH_MEAS_EN);
    assert(touch_mask != 0 && "wakeup reason is RTC_TOUCH_TRIG_EN but SENS_TOUCH_MEAS_EN is zero");
    return (touch_pad_t) (__builtin_ffs(touch_mask) - 1);
}

uint64_t esp_sleep_get_ext1_wakeup_status()
{
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT1) {
        return 0;
    }

    uint32_t status = REG_GET_FIELD(RTC_CNTL_EXT_WAKEUP1_STATUS_REG, RTC_CNTL_EXT_WAKEUP1_STATUS);
    // Translate bit map of RTC IO numbers into the bit map of GPIO numbers
    uint64_t gpio_mask = 0;
    for (int gpio = 0; gpio < GPIO_PIN_COUNT; ++gpio) {
        if (!RTC_GPIO_IS_VALID_GPIO(gpio)) {
            continue;
        }
        int rtc_pin = rtc_gpio_desc[gpio].rtc_num;
        if ((status & BIT(rtc_pin)) == 0) {
            continue;
        }
        gpio_mask |= 1ULL << gpio;
    }
    return gpio_mask;
}
*/

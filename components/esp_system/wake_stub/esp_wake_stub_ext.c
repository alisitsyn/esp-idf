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

#include "soc/rtc.h"
#include "soc/rtc_io_reg.h"
#include "soc/rtc_cntl_reg.h"

#ifdef CONFIG_IDF_TARGET_ESP32
#include "esp32/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/rtc.h"
#endif

#include "driver/rtc_io.h"
#include "esp_wake_stub_private.h"
#include "esp_wake_stub_sleep.h"

#define EXT_TAG "wake_stub_ext"

RTC_RODATA_ATTR int wake_stub_rtc_io_num_map[GPIO_PIN_COUNT] = {
#ifdef CONFIG_IDF_TARGET_ESP32
    RTCIO_GPIO0_CHANNEL,    //GPIO0
    -1,//GPIO1
    RTCIO_GPIO2_CHANNEL,    //GPIO2
    -1,//GPIO3
    RTCIO_GPIO4_CHANNEL,    //GPIO4
    -1,//GPIO5
    -1,//GPIO6
    -1,//GPIO7
    -1,//GPIO8
    -1,//GPIO9
    -1,//GPIO10
    -1,//GPIO11
    RTCIO_GPIO12_CHANNEL,   //GPIO12
    RTCIO_GPIO13_CHANNEL,   //GPIO13
    RTCIO_GPIO14_CHANNEL,   //GPIO14
    RTCIO_GPIO15_CHANNEL,   //GPIO15
    -1,//GPIO16
    -1,//GPIO17
    -1,//GPIO18
    -1,//GPIO19
    -1,//GPIO20
    -1,//GPIO21
    -1,//GPIO22
    -1,//GPIO23
    -1,//GPIO24
    RTCIO_GPIO25_CHANNEL,   //GPIO25
    RTCIO_GPIO26_CHANNEL,   //GPIO26
    RTCIO_GPIO27_CHANNEL,   //GPIO27
    -1,//GPIO28
    -1,//GPIO29
    -1,//GPIO30
    -1,//GPIO31
    RTCIO_GPIO32_CHANNEL,   //GPIO32
    RTCIO_GPIO33_CHANNEL,   //GPIO33
    RTCIO_GPIO34_CHANNEL,   //GPIO34
    RTCIO_GPIO35_CHANNEL,   //GPIO35
    RTCIO_GPIO36_CHANNEL,   //GPIO36
    RTCIO_GPIO37_CHANNEL,   //GPIO37
    RTCIO_GPIO38_CHANNEL,   //GPIO38
    RTCIO_GPIO39_CHANNEL,   //GPIO39
#else
    RTCIO_GPIO0_CHANNEL,    //GPIO0
    RTCIO_GPIO1_CHANNEL,    //GPIO1
    RTCIO_GPIO2_CHANNEL,    //GPIO2
    RTCIO_GPIO3_CHANNEL,    //GPIO3
    RTCIO_GPIO4_CHANNEL,    //GPIO4
    RTCIO_GPIO5_CHANNEL,    //GPIO5
    RTCIO_GPIO6_CHANNEL,    //GPIO6
    RTCIO_GPIO7_CHANNEL,    //GPIO7
    RTCIO_GPIO8_CHANNEL,    //GPIO8
    RTCIO_GPIO9_CHANNEL,    //GPIO9
    RTCIO_GPIO10_CHANNEL,   //GPIO10
    RTCIO_GPIO11_CHANNEL,   //GPIO11
    RTCIO_GPIO12_CHANNEL,   //GPIO12
    RTCIO_GPIO13_CHANNEL,   //GPIO13
    RTCIO_GPIO14_CHANNEL,   //GPIO14
    RTCIO_GPIO15_CHANNEL,   //GPIO15
    RTCIO_GPIO16_CHANNEL,   //GPIO16
    RTCIO_GPIO17_CHANNEL,   //GPIO17
    RTCIO_GPIO18_CHANNEL,   //GPIO18
    RTCIO_GPIO19_CHANNEL,   //GPIO19
    RTCIO_GPIO20_CHANNEL,   //GPIO20
    RTCIO_GPIO21_CHANNEL,   //GPIO21
    -1,//GPIO22
    -1,//GPIO23
    -1,//GPIO24
    -1,//GPIO25
    -1,//GPIO26
    -1,//GPIO27
    -1,//GPIO28
    -1,//GPIO29
    -1,//GPIO30
    -1,//GPIO31
    -1,//GPIO32
    -1,//GPIO33
    -1,//GPIO34
    -1,//GPIO35
    -1,//GPIO36
    -1,//GPIO37
    -1,//GPIO38
    -1,//GPIO39
    -1,//GPIO40
    -1,//GPIO41
    -1,//GPIO42
    -1,//GPIO43
    -1,//GPIO44
    -1,//GPIO45
    -1,//GPIO46
    -1,//GPIO47
#endif
};

// Get RTC IO index number by gpio number.
static int wake_stub_io_number_get(gpio_num_t gpio_num)
{
    return wake_stub_rtc_io_num_map[gpio_num]; // 
}

void wake_stub_ext0_wakeup_prepare()
{
    int rtc_gpio_num = wake_stub_s_config.ext0_rtc_gpio_num;
    // Set GPIO to be used for wakeup
    REG_SET_FIELD(RTC_IO_EXT_WAKEUP0_REG, RTC_IO_EXT_WAKEUP0_SEL, rtc_gpio_num);
    // Set level which will trigger wakeup
    SET_PERI_REG_BITS(RTC_CNTL_EXT_WAKEUP_CONF_REG, 0x1,
            wake_stub_s_config.ext0_trigger_level, RTC_CNTL_EXT_WAKEUP0_LV_S);
}

esp_err_t esp_wake_stub_enable_ext0_wakeup(gpio_num_t gpio_num, int level)
{
    if (level < 0 || level > 1) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!wake_stub_io_number_get(gpio_num)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (wake_stub_s_config.wakeup_triggers & (RTC_TOUCH_TRIG_EN | RTC_ULP_TRIG_EN)) {
        ESP_RTC_LOGE("Conflicting wake-up triggers: touch / ULP");
        return ESP_ERR_INVALID_STATE;
    }
    wake_stub_s_config.ext0_rtc_gpio_num = wake_stub_io_number_get(gpio_num);
    wake_stub_s_config.ext0_trigger_level = level;
    wake_stub_s_config.wakeup_triggers |= RTC_EXT0_TRIG_EN;
    return ESP_OK;
}

esp_err_t esp_wake_stub_enable_ext1_wakeup(uint64_t mask, esp_sleep_ext1_wakeup_mode_t mode)
{
    if (mode > ESP_EXT1_WAKEUP_ANY_HIGH) {
        return ESP_ERR_INVALID_ARG;
    }
    wake_stub_s_config.ext1_rtc_gpio_mask = mask;
    wake_stub_s_config.ext1_trigger_mode = mode;
    wake_stub_s_config.wakeup_triggers |= RTC_EXT1_TRIG_EN;
    return ESP_OK;
}

void wake_stub_ext1_wakeup_prepare()
{
    // Todo:rtcio_ll_ext1_clear_wakeup_pins(void)
    // Clear state from previous wakeup
    REG_SET_BIT(RTC_CNTL_EXT_WAKEUP1_REG, RTC_CNTL_EXT_WAKEUP1_STATUS_CLR);
    // Todo: rtcio_ll_ext1_set_wakeup_pins(uint32_t mask, int mode)
    // Set pins to be used for wakeup
    REG_SET_FIELD(RTC_CNTL_EXT_WAKEUP1_REG, RTC_CNTL_EXT_WAKEUP1_SEL, \
                                            wake_stub_s_config.ext1_rtc_gpio_mask);
    // Set logic function (any low, all high)
    SET_PERI_REG_BITS(RTC_CNTL_EXT_WAKEUP_CONF_REG, 0x1,
                    wake_stub_s_config.ext1_trigger_mode, RTC_CNTL_EXT_WAKEUP1_LV_S);
}

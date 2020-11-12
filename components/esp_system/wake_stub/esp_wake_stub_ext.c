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
#include "hal/rtc_hal.h"
#include "hal/rtc_io_hal.h"

#ifdef CONFIG_IDF_TARGET_ESP32
#include "esp32/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S3
#include "esp32s3/rom/rtc.h"
#endif

#include "esp_wake_stub_private.h"
#include "esp_wake_stub_sleep.h"

#define EXT_TAG "WAKE_STUB_EXT"

// Get RTC IO index number by gpio number.
static int wake_stub_io_number_get(gpio_num_t gpio_num)
{
    return rtc_io_num_map[gpio_num];
}

void wake_stub_ext0_wakeup_prepare()
{
    int rtc_gpio_num = wake_stub_s_config.ext0_rtc_gpio_num;
    rtcio_hal_ext0_set_wakeup_pin(rtc_gpio_num, wake_stub_s_config.ext0_trigger_level);
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
    // Clear state from previous wakeup
    rtc_hal_ext1_clear_wakeup_pins();

    // Set pins to be used for wakeup
    rtc_hal_ext1_set_wakeup_pins(wake_stub_s_config.ext1_rtc_gpio_mask,
                                    wake_stub_s_config.ext1_trigger_mode);
}

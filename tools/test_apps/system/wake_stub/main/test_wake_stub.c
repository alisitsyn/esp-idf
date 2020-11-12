#include "unity.h"
#include "esp_system.h"
#include <sys/time.h>               // for time measurement
#include "esp_log.h"                // for log write functionality
#include "esp_err.h"
#include "driver/rtc_io.h"          // for gpio configuration
#include "soc/uart_reg.h"
#include "driver/rtc_io.h"

#ifdef CONFIG_IDF_TARGET_ESP32
#include "esp32/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/rtc.h"
#endif

#include "esp_sleep.h"              // include sleep related functionality
#include "soc/timer_group_reg.h"    // for watchdog register defines
#include "esp_wake_stub_sleep.h"    // for rtc cntl register defines
#include "sdkconfig.h"

// Test notes:
// This test case sequence checks behavior of wake stub API to go deep sleep

#define ESP_EXT0_WAKEUP_LEVEL_LOW 0
#define ESP_EXT0_WAKEUP_LEVEL_HIGH 1

#define WAKE_STUB_ENTER_COUNT 4
#define TIMER_TIMEOUT_SEC 1
#define TEST_EXT0_WAKEUP_DONE 1
#define TEST_TIMER_WAKEUP_DONE 2

#define TEST_EXT_INP_PIN GPIO_NUM_13

#define TEST_WAKE_STUB_TAG "TEST_WAKE_STUB"

static RTC_DATA_ATTR int wake_count = 0;
static RTC_DATA_ATTR uint64_t sleep_time = 0;
static RTC_DATA_ATTR uint8_t test_state = 0;

#define TEST_CHECK(a, ret_val, str, ...) \
    if (!(a)) { \
        ESP_LOGE(TEST_WAKE_STUB_TAG, "%s(%u): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        return ret_val; \
    }

static RTC_IRAM_ATTR void wake_stub_ext0(void)
{
    esp_default_wake_deep_sleep();
    // Set the pointer of the new wake stub function.
    // It will be checked in test to make sure the wake stub entered
    REG_WRITE(RTC_ENTRY_ADDR_REG, (uint32_t)&wake_stub_ext0);
    sleep_time = esp_wake_stub_get_sleep_time_us();

    if (wake_count < WAKE_STUB_ENTER_COUNT) {
        wake_count++;
    } else {
        test_state = TEST_EXT0_WAKEUP_DONE;
        return;
    }

    ESP_RTC_LOGI("Wake stab enter count: %d\n", wake_count);
    esp_wake_stub_uart_tx_wait_idle(CONFIG_ESP_CONSOLE_UART_NUM);
    esp_wake_stub_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_wake_stub_enable_ext0_wakeup(TEST_EXT_INP_PIN, ESP_EXT0_WAKEUP_LEVEL_HIGH);
    esp_wake_stub_deep_sleep_start();
}

static RTC_IRAM_ATTR void wake_stub_timer(void)
{
    esp_default_wake_deep_sleep();
    // Set the pointer of the new wake stub function.
    // It will be checked in test to make sure the wake stub entered
    REG_WRITE(RTC_ENTRY_ADDR_REG, (uint32_t)&wake_stub_timer);

    if (wake_count < WAKE_STUB_ENTER_COUNT) {
        wake_count++;
        sleep_time = esp_wake_stub_get_sleep_time_us();
    } else {
        test_state = TEST_TIMER_WAKEUP_DONE;
        return;
    }
    ESP_RTC_LOGI("Wake stab enter count: %d\n", wake_count);
    esp_wake_stub_uart_tx_wait_idle(CONFIG_ESP_CONSOLE_UART_NUM);
    esp_wake_stub_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_wake_stub_deep_sleep(1000000 * TIMER_TIMEOUT_SEC);
}

static void setup_ext0_deep_sleep(void)
{
    // Set wake stub function to check its behavior
    // This function sets checksum of RTC fast memory appropriately
    esp_set_deep_sleep_wake_stub(&wake_stub_ext0);
    // Setup ext0 configuration to wake up immediately
    ESP_ERROR_CHECK(rtc_gpio_init(TEST_EXT_INP_PIN));
    ESP_ERROR_CHECK(rtc_gpio_set_direction_in_sleep(TEST_EXT_INP_PIN, RTC_GPIO_MODE_INPUT_ONLY));
    ESP_ERROR_CHECK(rtc_gpio_pullup_en(TEST_EXT_INP_PIN));
    ESP_ERROR_CHECK(rtc_gpio_pulldown_dis(TEST_EXT_INP_PIN));
    ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(TEST_EXT_INP_PIN, ESP_EXT0_WAKEUP_LEVEL_HIGH));
    test_state = 0;
    wake_count = 0;
    esp_deep_sleep_start();
}

static void setup_timer_deep_sleep(void)
{
    // Set wake stub function to check its behavior
    // This function sets checksum of RTC fast memory appropriately
    esp_set_deep_sleep_wake_stub(&wake_stub_timer);
    esp_sleep_enable_timer_wakeup(1000000 * TIMER_TIMEOUT_SEC);
    test_state = 0;
    wake_count = 0;
    esp_deep_sleep_start();
}

static bool check_timer_wake_deep_sleep()
{
    printf("Wake stub count: %u\n", wake_count);
    printf("Wake stub sleep time since last enter: %llu (uS)\n", (uint64_t)sleep_time);
    TEST_CHECK(wake_count == WAKE_STUB_ENTER_COUNT, false,
                        "Test wake stub wake up from timer is failed, counter = %d", wake_count);
    const uint64_t sleep_time_us = TIMER_TIMEOUT_SEC * 1000000;
    TEST_ASSERT_INT32_WITHIN(80000, sleep_time_us, sleep_time);
    return true;
}

static bool check_ext0_wake_deep_sleep()
{
    printf("Wake stub count: %u\n", wake_count);
    printf("Wake stub sleep time since last enter: %llu (uS)\n", (uint64_t)sleep_time);
    TEST_CHECK(wake_count == WAKE_STUB_ENTER_COUNT, false,
                        "Test wake stub wake up from timer is failed, counter = %d", wake_count);
    return true;
}

void app_main()
{
    printf("Start wake stub test main.\n");
    RESET_REASON reason = rtc_get_reset_reason(0);
    bool test_passed = false;

    switch (reason)
    {
        case TG0WDT_SYS_RESET:
            ESP_LOGI(TEST_WAKE_STUB_TAG, "TG0WDT_SYS_RESET reason=(%d)\n", (uint16_t)reason);
            break;

        case DEEPSLEEP_RESET:
            ESP_LOGI(TEST_WAKE_STUB_TAG, "DEEPSLEEP_RESET reason=(%d)\n", (uint16_t)reason);
            if (test_state == TEST_EXT0_WAKEUP_DONE){
                if (check_ext0_wake_deep_sleep()) {
                    printf("Test wake stub ext0 is passed.\n");
                    printf("Setup wake stub timer test.\n");
                    test_state = 0;
                    setup_timer_deep_sleep();
                }
            } else if (test_state == TEST_TIMER_WAKEUP_DONE) {
                if (check_timer_wake_deep_sleep()) {
                    printf("Test wake stub timer is passed.\n");
                    test_passed = true;
                }
            } else {
                test_state = 0;
                setup_ext0_deep_sleep();
            }
            break;

        case RTCWDT_RTC_RESET:
        case POWERON_RESET:
        default:
            if (test_state == 0) {
                setup_ext0_deep_sleep();
            } else {
                printf("Wake up from deep sleep in incorrect state.\n");
            }
            test_state = 0;

            ESP_LOGI(TEST_WAKE_STUB_TAG, "Reset reason = (%d)\n", (uint16_t)reason);
            printf("Go to deep sleep to check DEEP_SLEEP_RESET behavior.\n");
            setup_ext0_deep_sleep();
            break;
    }
    if (test_passed) {
        printf("All wake stub tests are passed.\n");
    }
}

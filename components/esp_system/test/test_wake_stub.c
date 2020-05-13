#include "unity.h"
#include "esp_system.h"
#include <sys/time.h>               // for time measurement

#ifdef CONFIG_IDF_TARGET_ESP32
#include "esp32/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/rtc.h"
#endif

#include "esp_log.h"                // for log write functionality
#include "driver/rtc_io.h"          // for gpio configuration
#include "esp_sleep.h"              // include sleep related functionality
#include "soc/soc.h"                // include access to soc macro
#include "esp_wake_stub_sleep.h"    // for wake stub API access
#include "esp_sleep.h"              // for deep sleep cause
#include "sdkconfig.h"

// Test notes:
// This test case sequence checks behavior of wake stub API to go deep sleep

#define ESP_EXT0_WAKEUP_LEVEL_LOW 0
#define ESP_EXT0_WAKEUP_LEVEL_HIGH 1

#define WAKE_STUB_ENTER_COUNT 4
#define TIMER_TIMEOUT_SEC 1

#define TEST_EXT0_GPIO_PIN GPIO_NUM_13

static RTC_DATA_ATTR int wake_count = 0;
static RTC_DATA_ATTR esp_sleep_wakeup_cause_t wake_cause = ESP_SLEEP_WAKEUP_UNDEFINED;
static RTC_DATA_ATTR uint64_t sleep_time = 0;
static struct timeval tv_start, tv_stop;

static const char* tag = "wake_stub_UnitTestMain";
//static RTC_RODATA_ATTR const char fmt[] = "Wake stub enter count: %d\n";

static RTC_IRAM_ATTR void wake_stub_timer(void)
{
    esp_default_wake_deep_sleep();

    sleep_time = esp_wake_stub_get_sleep_time_us();
    wake_cause = esp_wake_stub_sleep_get_wakeup_cause();

    if (wake_count < WAKE_STUB_ENTER_COUNT) {
        wake_count++;
    } else {
        return;
    }

    ESP_RTC_LOGI("Enter count: %d\n", wake_count);
    esp_wake_stub_uart_tx_wait_idle(CONFIG_ESP_CONSOLE_UART_NUM);
    // Set the pointer of the new wake stub function.
    // It will be checked in test to make sure the wake stub entered
    REG_WRITE(RTC_ENTRY_ADDR_REG, (uint32_t)&wake_stub_timer);
    set_rtc_memory_crc(); // update RTC memory CRC using ROM function
    esp_wake_stub_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_wake_stub_deep_sleep(1000000 * TIMER_TIMEOUT_SEC);
}

static RTC_IRAM_ATTR void wake_stub_ext0(void)
{
    esp_default_wake_deep_sleep();

    sleep_time = esp_wake_stub_get_sleep_time_us();
    wake_cause = esp_wake_stub_sleep_get_wakeup_cause();

    if (wake_count < WAKE_STUB_ENTER_COUNT) {
        wake_count++;
    } else {
        return;
    }

    ESP_RTC_LOGI("Enter count: %d\n", wake_count);
    esp_wake_stub_uart_tx_wait_idle(CONFIG_ESP_CONSOLE_UART_NUM);
    // Set the pointer of the new wake stub function.
    // It will be checked in test to make sure the wake stub entered
    REG_WRITE(RTC_ENTRY_ADDR_REG, (uint32_t)&wake_stub_ext0);
    set_rtc_memory_crc(); // update RTC memory CRC
    esp_wake_stub_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_wake_stub_enable_ext0_wakeup(TEST_EXT0_GPIO_PIN, ESP_EXT0_WAKEUP_LEVEL_HIGH);
    esp_wake_stub_deep_sleep_start();
}

static float get_time_ms(void)
{
    gettimeofday(&tv_stop, NULL);
    float dt = (tv_stop.tv_sec - tv_start.tv_sec) * 1e3f +
                (tv_stop.tv_usec - tv_start.tv_usec) * 1e-3f;
    return abs(dt);
}

static void setup_ext0_deep_sleep(void)
{
    printf("Go to deep sleep to check wake stub ext0 behavior. \n");
    gettimeofday(&tv_start, NULL);
    wake_count = 0; // Init wakeup counter
    sleep_time = 0;
    // Set wake stub function to check its behavior
    // This function sets checksum of RTC fast memory appropriately
    esp_set_deep_sleep_wake_stub(&wake_stub_ext0);
    // Setup ext0 configuration to wake up immediately
    ESP_ERROR_CHECK(rtc_gpio_init(TEST_EXT0_GPIO_PIN));
    ESP_ERROR_CHECK(gpio_pullup_en(TEST_EXT0_GPIO_PIN));
    ESP_ERROR_CHECK(gpio_pulldown_dis(TEST_EXT0_GPIO_PIN));
    ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(TEST_EXT0_GPIO_PIN, ESP_EXT0_WAKEUP_LEVEL_HIGH));
    esp_deep_sleep_start();
}

static void check_ext0_wake_stub(void)
{   
    TEST_ASSERT_EQUAL(ESP_RST_DEEPSLEEP, esp_reset_reason());
    gettimeofday(&tv_stop, NULL);
    float dt = get_time_ms();
    printf("Time from start: %u\n", (int)dt);
    printf("Wake stub count: %u\n", wake_count);
    printf("Wake stub sleep time since last enter: %llu (uS)\n", (uint64_t)sleep_time);
    printf("Wake cause: %d\n", wake_cause);
    TEST_ASSERT(wake_count == WAKE_STUB_ENTER_COUNT);
    printf("Wake stub ext0 test is done.");
}

TEST_CASE_MULTIPLE_STAGES("Deep sleep wake stub ext0 check", "[wake_stub][reset=DEEPSLEEP_RESET]",
        setup_ext0_deep_sleep,
        check_ext0_wake_stub);

static void setup_timer_deep_sleep(void)
{
    // Set wake stub function to check its behavior
    // This function sets checksum of RTC fast memory appropriately
    esp_set_deep_sleep_wake_stub(&wake_stub_timer);
    gettimeofday(&tv_start, NULL);
    wake_count = 0; // Reset counter before start
    sleep_time = 0;
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(1000000 * TIMER_TIMEOUT_SEC));
    esp_deep_sleep_start();
}        
        
static void check_timer_wake_stub(void)
{   
    ESP_LOGI(tag, "DEEPSLEEP_RESET, reason=(%d)\n", esp_reset_reason());
    TEST_ASSERT_EQUAL(ESP_RST_DEEPSLEEP, esp_reset_reason());
    gettimeofday(&tv_stop, NULL);
    float dt = get_time_ms();
    printf("Time since start: %u\n", (int)dt);
    printf("Wake stub count: %u\n", wake_count);
    printf("Wake stub sleep time since last enter: %llu (uS)\n", sleep_time);
    const uint64_t sleep_time_us = TIMER_TIMEOUT_SEC * 1000000;
    TEST_ASSERT_INT32_WITHIN(80000, sleep_time_us, sleep_time);
    printf("Wake cause: %d\n", wake_cause);
    TEST_ASSERT(wake_count == WAKE_STUB_ENTER_COUNT);
    printf("Wake stub timer test is done.");
}

TEST_CASE_MULTIPLE_STAGES("Deep sleep wake stub timer check", "[wake_stub][reset=DEEPSLEEP_RESET]",
        setup_timer_deep_sleep,
        check_timer_wake_stub);

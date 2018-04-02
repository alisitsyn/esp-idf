#include "unity.h"
#include "esp_system.h"
#include <sys/time.h>               // for time measurement
#include "rom/rtc.h"                // for rtc defines
#include "esp_log.h"                // for log write functionality
#include "driver/rtc_io.h"          // for gpio configuration 
#include "esp_sleep.h"              // include sleep related functionality
#include "soc/soc.h"                // include access to soc macros
#include "soc/timer_group_reg.h"    // for watchdog register defines
#include "soc/rtc_cntl_reg.h"       // for rtc cntl register defines
#include "wake_stub/esp_wake_stub_sleep.h"       // for rtc cntl register defines

#include "soc/dport_access.h"       // for dport register debugging
#include "soc/dport_reg.h"

// Test notes:
// This test case sequence checks behavior of wake stub API to go deep sleep

#define ESP_EXT0_WAKEUP_LEVEL_LOW 0
#define ESP_EXT0_WAKEUP_LEVEL_HIGH 1

#define WAKE_STUB_ENTER_COUNT 4
#define TIMER_TIMEOUT_SEC 1

static RTC_RODATA_ATTR const char fmt_str[] = "Wake stub enter count: %d\n";
static RTC_DATA_ATTR int wake_count = 0;
static RTC_DATA_ATTR uint64_t sleep_time = 0;

static struct timeval tv_start, tv_stop;

extern int _rtc_data_start;
extern int _rtc_data_end;

// This points to start of rtc_data segment 
// The rtc_rodata values are placed right after data values
static uint32_t *rtc_data_val_addr = (uint32_t*)&_rtc_data_start;

static const char* tag = "wake_stub_UnitTestMain";

static void RTC_IRAM_ATTR wake_stub_ext0(void)
{
    esp_default_wake_deep_sleep();
    
    // Set the pointer of the new wake stub function. 
    // It will be checked in test to make sure the wake stub entered
    REG_WRITE(RTC_ENTRY_ADDR_REG, (uint32_t)&wake_stub_ext0);
    
    sleep_time = esp_wsapi_get_sleep_time_us();

    if (wake_count < WAKE_STUB_ENTER_COUNT) {
        wake_count++;
    } else {
        return;
    }
    
    ets_printf(fmt_str, wake_count);
    //sleep_time = (uint32_t)wsapi_get_sleep_time();

    set_rtc_memory_crc(); // update rtc memory CRC
    esp_wsapi_deep_sleep_start();
}

static void RTC_IRAM_ATTR wake_stub_timer(void)
{
    esp_default_wake_deep_sleep();
    
    // Set the pointer of the new wake stub function. 
    // It will be checked in test to make sure the wake stub entered
    REG_WRITE(RTC_ENTRY_ADDR_REG, (uint32_t)&wake_stub_timer);
    
    if (wake_count < WAKE_STUB_ENTER_COUNT) {
        wake_count++;
        sleep_time = esp_wsapi_get_sleep_time_us();
    } else {
        return;
    }
    
    ets_printf(fmt_str, wake_count);
    //sleep_time = (uint32_t)wsapi_get_sleep_time();

    set_rtc_memory_crc(); // update rtc memory CRC

    esp_wsapi_deep_sleep(1000000LL * TIMER_TIMEOUT_SEC);
    //esp_wsapi_deep_sleep_start();
}

static void setup_ext0_deep_sleep(void)
{
    // Set wake stub function to check its behavior
    // This function sets checksum of RTC fast memory appropriately
    esp_set_deep_sleep_wake_stub(&wake_stub_ext0);

    // Setup ext0 configuration to wake up immediately
    ESP_ERROR_CHECK(rtc_gpio_init(GPIO_NUM_13));
    ESP_ERROR_CHECK(gpio_pullup_en(GPIO_NUM_13));
    ESP_ERROR_CHECK(gpio_pulldown_dis(GPIO_NUM_13));
    ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(GPIO_NUM_13, ESP_EXT0_WAKEUP_LEVEL_HIGH));
    
    esp_deep_sleep_start();
}

static void setup_timer_deep_sleep(void)
{
    // Set wake stub function to check its behavior
    // This function sets checksum of RTC fast memory appropriately
    esp_set_deep_sleep_wake_stub(&wake_stub_timer);
    
    esp_sleep_enable_timer_wakeup(1000000 * TIMER_TIMEOUT_SEC);
    esp_deep_sleep_start();
}

static float get_time_ms(void)
{
    gettimeofday(&tv_stop, NULL);
    
    float dt = (tv_stop.tv_sec - tv_start.tv_sec) * 1e3f +
                (tv_stop.tv_usec - tv_start.tv_usec) * 1e-3f;
    return abs(dt);
}

// Theis is defined for debbuging access to ROM functions which 
// may cause WDT0 reset
void enable_wdt_info(void)
{
    DPORT_REG_SET_BIT(DPORT_PRO_CPU_RECORD_CTRL_REG, \
        DPORT_PRO_CPU_PDEBUG_ENABLE | DPORT_PRO_CPU_RECORD_ENABLE);
    DPORT_REG_CLR_BIT(DPORT_PRO_CPU_RECORD_CTRL_REG, DPORT_PRO_CPU_RECORD_ENABLE);
}

// Helper for debugging of WDT reset
// When wake stub code calls some libgcc functions from ROM 
// it cause TG0WDT_SYS_RESET
static void test_wdt_info_dump(int cpu)
{
    uint32_t inst = 0, pid = 0, stat = 0, data = 0, pc = 0,
             lsstat = 0, lsaddr = 0, lsdata = 0, dstat = 0;
    char *cpu_name = cpu ? "APP" : "PRO";

    if (cpu == 0) {
        stat    = DPORT_REG_READ(DPORT_PRO_CPU_RECORD_STATUS_REG);
        pid     = DPORT_REG_READ(DPORT_PRO_CPU_RECORD_PID_REG);
        inst    = DPORT_REG_READ(DPORT_PRO_CPU_RECORD_PDEBUGINST_REG);
        dstat   = DPORT_REG_READ(DPORT_PRO_CPU_RECORD_PDEBUGSTATUS_REG);
        data    = DPORT_REG_READ(DPORT_PRO_CPU_RECORD_PDEBUGDATA_REG);
        pc      = DPORT_REG_READ(DPORT_PRO_CPU_RECORD_PDEBUGPC_REG);
        lsstat  = DPORT_REG_READ(DPORT_PRO_CPU_RECORD_PDEBUGLS0STAT_REG);
        lsaddr  = DPORT_REG_READ(DPORT_PRO_CPU_RECORD_PDEBUGLS0ADDR_REG);
        lsdata  = DPORT_REG_READ(DPORT_PRO_CPU_RECORD_PDEBUGLS0DATA_REG);

    } else {
        stat    = DPORT_REG_READ(DPORT_APP_CPU_RECORD_STATUS_REG);
        pid     = DPORT_REG_READ(DPORT_APP_CPU_RECORD_PID_REG);
        inst    = DPORT_REG_READ(DPORT_APP_CPU_RECORD_PDEBUGINST_REG);
        dstat   = DPORT_REG_READ(DPORT_APP_CPU_RECORD_PDEBUGSTATUS_REG);
        data    = DPORT_REG_READ(DPORT_APP_CPU_RECORD_PDEBUGDATA_REG);
        pc      = DPORT_REG_READ(DPORT_APP_CPU_RECORD_PDEBUGPC_REG);
        lsstat  = DPORT_REG_READ(DPORT_APP_CPU_RECORD_PDEBUGLS0STAT_REG);
        lsaddr  = DPORT_REG_READ(DPORT_APP_CPU_RECORD_PDEBUGLS0ADDR_REG);
        lsdata  = DPORT_REG_READ(DPORT_APP_CPU_RECORD_PDEBUGLS0DATA_REG);
    }
    if (DPORT_RECORD_PDEBUGINST_SZ(inst) == 0 &&
        DPORT_RECORD_PDEBUGSTATUS_BBCAUSE(dstat) == DPORT_RECORD_PDEBUGSTATUS_BBCAUSE_WAITI) {
        ESP_LOGW(tag, "WDT reset info: %s CPU PC=0x%x (waiti mode)", cpu_name, pc);
    } else {
        ESP_LOGW(tag, "WDT reset info: %s CPU PC=0x%x", cpu_name, pc);
    }
    ESP_LOGD(tag, "WDT reset info: %s CPU STATUS        0x%08x", cpu_name, stat);
    ESP_LOGD(tag, "WDT reset info: %s CPU PID           0x%08x", cpu_name, pid);
    ESP_LOGD(tag, "WDT reset info: %s CPU PDEBUGINST    0x%08x", cpu_name, inst);
    ESP_LOGD(tag, "WDT reset info: %s CPU PDEBUGSTATUS  0x%08x", cpu_name, dstat);
    ESP_LOGD(tag, "WDT reset info: %s CPU PDEBUGDATA    0x%08x", cpu_name, data);
    ESP_LOGD(tag, "WDT reset info: %s CPU PDEBUGPC      0x%08x", cpu_name, pc);
    ESP_LOGD(tag, "WDT reset info: %s CPU PDEBUGLS0STAT 0x%08x", cpu_name, lsstat);
    ESP_LOGD(tag, "WDT reset info: %s CPU PDEBUGLS0ADDR 0x%08x", cpu_name, lsaddr);
    ESP_LOGD(tag, "WDT reset info: %s CPU PDEBUGLS0DATA 0x%08x", cpu_name, lsdata);
}

TEST_CASE("Deep sleep wake stub timer check", "[wake_stub][retry_after_reset=2]")
{
    printf("This test case checks behavior of wake stub API for timer. \n");
    
    // Enable debugging of WDT reset (can be triggered on access error)
    enable_wdt_info();
    RESET_REASON reason = rtc_get_reset_reason(0);
    
    switch (reason)
    {
        case TG0WDT_SYS_RESET:
            ESP_LOGI(tag, "TG0WDT_SYS_RESET, reason=(%d)\n", (uint16_t)reason);
            
            test_wdt_info_dump(0);
            break;

        case DEEPSLEEP_RESET:
            ESP_LOGI(tag, "DEEPSLEEP_RESET, reason=(%d)\n", (uint16_t)reason);

            gettimeofday(&tv_stop, NULL);

            float dt = get_time_ms();

            printf("Time since start: %u\n", (int)dt);

            printf("Wake stub count: %u\n", wake_count);

            //sleep_time = (uint64_t)esp_wsapi_get_sleep_time_us();
            printf("Wake stub sleep time since last enter: %llu (uS)\n", (uint64_t)sleep_time);
            
            TEST_ASSERT(wake_count == WAKE_STUB_ENTER_COUNT);
            const uint64_t sleep_time_us = TIMER_TIMEOUT_SEC * 1000000; 

            TEST_ASSERT_INT32_WITHIN(80000, sleep_time_us, sleep_time);


            if (wake_count == WAKE_STUB_ENTER_COUNT) {
                wake_count = 0;
                gettimeofday(&tv_start, NULL);

                setup_timer_deep_sleep();
            }

            printf("The wake stub test cases are done.. \n");
            break;
            
        case RTCWDT_RTC_RESET:
        case POWERON_RESET:
        default:
            ESP_LOGI(tag, "Reset reason is not expected, reason=(%d)\n", (uint16_t)reason);
            
            printf("Go to deep sleep to check DEEP_SLEEP_RESET behavior. \n");
            gettimeofday(&tv_start, NULL);
            
            setup_timer_deep_sleep();
            break;    
    }
}

TEST_CASE("Deep sleep wake stub ext0 check", "[wake_stub][retry_after_reset=2]")
{
    printf("This test case checks behavior of wake stub API. \n");
    enable_wdt_info();
    RESET_REASON reason = rtc_get_reset_reason(0);

    switch (reason)
    {
        case TG0WDT_SYS_RESET:
            ESP_LOGI(tag, "TG0WDT_SYS_RESET reason=(%d)\n", (uint16_t)reason);
            
            test_wdt_info_dump(0);
            break;

        case DEEPSLEEP_RESET:
            ESP_LOGI(tag, "DEEPSLEEP_RESET reason=(%d)\n", (uint16_t)reason);

            gettimeofday(&tv_stop, NULL);

            float dt = get_time_ms();

            printf("Time since start: %u\n", (int)dt);

            printf("Wake stub enter count: %u\n", wake_count);

            printf("Wake stub sleep time since last sleep: %llu (uS)\n", (uint64_t)sleep_time);

            TEST_ASSERT(wake_count == WAKE_STUB_ENTER_COUNT);

            if (wake_count == WAKE_STUB_ENTER_COUNT) {
                wake_count = 0;
                gettimeofday(&tv_start, NULL);
                sleep_time = 0;
                setup_ext0_deep_sleep();
            }

            TEST_ASSERT(wake_count == WAKE_STUB_ENTER_COUNT);
           
            printf("The wake stub test cases are done.. \n");
            break;
            
        case RTCWDT_RTC_RESET:
        case POWERON_RESET:
        default:
            ESP_LOGI(tag, "Reset reason is not expected, reason=(%d)\n", (uint16_t)reason);
            
            printf("Go to deep sleep to check DEEP_SLEEP_RESET behavior. \n");
            gettimeofday(&tv_start, NULL);
            
            setup_ext0_deep_sleep();
            break;    
    }
}
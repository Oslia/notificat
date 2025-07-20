/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "bsp/esp-bsp.h"

#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "sdkconfig.h"
#include "network.hpp"
#include "nvs_flash.h"
#include "app_mng.hpp"
#include "alarm.hpp"
#include "weather.hpp"

static const char *TAG = "notificat";
#define EXAMPLE_ESP_WIFI_SSID      "241147923271"
#define EXAMPLE_ESP_WIFI_PASS      "52279344"
#define LOG_MEM_INFO    (1)
extern void mytest(void);
const char *base_path = "/spiflash";
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
FATFS* fs;
extern "C"
void app_main(void)
{
    time_t now;
	char strftime_buf[64];
    struct tm timeinfo;

    ESP_LOGI(TAG, "Mounting FAT filesystem");
    // To mount device we need name of device partition, define base_path
    // and allow format partition in case if it is new one and was not formatted before
    const esp_vfs_fat_mount_config_t mount_config = {
            .format_if_mount_failed = true,
            .max_files = 4,
            .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
            .disk_status_check_enable = false,
            .use_one_fat = false,
    };

    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(base_path, "storage", &mount_config, &s_wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }
    ESP_ERROR_CHECK(nvs_flash_init());

    /* Initialize display and LVGL */
	bsp_display_start();
    
    Network& network = Network::Instance();
    network.ConnectWifi(EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    //mytest();

    network.TimeSync();

    /* Set time zone JAPAN */
    setenv("TZ", "JST-9", 1);
    tzset();

#if CONFIG_BSP_DISPLAY_LVGL_AVOID_TEAR
    ESP_LOGI(TAG, "Avoid lcd tearing effect");
#if CONFIG_BSP_DISPLAY_LVGL_FULL_REFRESH
    ESP_LOGI(TAG, "LVGL full-refresh");
#elif CONFIG_BSP_DISPLAY_LVGL_DIRECT_MODE
    ESP_LOGI(TAG, "LVGL direct-mode");
#endif
#endif

    /* Set display brightness to 100% */
    //bsp_display_backlight_on();
    
    //bsp_display_lock(0);
    // lv_demo_widgets();      /* A widgets example */
    // lv_demo_music();        /* A modern, smartphone-like music player demo. */
    // lv_demo_stress();       /* A stress test for LVGL. */
    // lv_demo_benchmark();    /* A demo to measure the performance of LVGL or to compare different settings. */
    //bsp_display_unlock();
    AppMng& mng = AppMng::Instance();
    Alarm::Alarm alarm;
    Weather::Weather weather;
    mng.RegisterApp(&alarm);
    mng.RegisterApp(&weather);
    mng.Run();
#if LOG_MEM_INFO
    //static char buffer[128];    /* Make sure buffer is enough for `sprintf` */
    while (1) {
        //sprintf(buffer, "   Biggest /     Free /    Total\n"
        //        "\t  SRAM : [%8d / %8d / %8d]\n"
        //        "\t PSRAM : [%8d / %8d / %8d]",
        //        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
        //        heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        //        heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
        //        heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
        //        heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        //        heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
        //ESP_LOGI("MEM", "%s", buffer);
        //time(&now);
        //localtime_r(&now, &timeinfo);
        //strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif
    ESP_ERROR_CHECK(esp_vfs_fat_spiflash_unmount_rw_wl(base_path, s_wl_handle));
}
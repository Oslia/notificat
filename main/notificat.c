#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "sdkconfig.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "nvs_flash.h"
#include "lvgl.h"
#include "lv_demos.h"

static const char *TAG = "notificat";
const char *base_path = "/spiflash";
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

/*
 * The BSP default for taskLVGL is 7168 bytes. LVGL's music demo, especially
 * when FreeType is enabled, exceeds that during rendering.
 */
#define LVGL_TASK_STACK_SIZE 12288

void app_main(void)
{
    ESP_LOGI(TAG, "Mounting FAT filesystem");
    // Do not erase the bundled storage image or future user settings on a mount failure.
    const esp_vfs_fat_mount_config_t mount_config = {
            .format_if_mount_failed = false,
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

    /* Initialize display and LVGL with a stack suitable for the music demo. */
    bsp_display_cfg_t display_cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * CONFIG_BSP_LCD_DRAW_BUF_HEIGHT,
#if CONFIG_BSP_LCD_DRAW_BUF_DOUBLE
        .double_buffer = true,
#endif
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .sw_rotate = false,
        },
    };
    display_cfg.lvgl_port_cfg.task_stack = LVGL_TASK_STACK_SIZE;

    if (bsp_display_start_with_config(&display_cfg) == NULL) {
        ESP_LOGE(TAG, "Failed to initialize display and LVGL");
        return;
    }
    bsp_display_backlight_on();
    
    bsp_display_lock(0);
    lv_demo_music(); /* スマートフォン風音楽プレーヤーデモ */
    // lv_demo_stress();       /* LVGL ストレステストデモ */
    // lv_demo_benchmark();    /* LVGL 性能ベンチマークデモ */
    bsp_display_unlock();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

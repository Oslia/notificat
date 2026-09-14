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
#include "core/SystemBootstrap.h"
#include "ui/AppShell.hpp"
#include "ui/SplashScreen.hpp"

static const char *TAG = "notificat";
const char *base_path = "/spiflash";
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

/*
 * BSP 標準の taskLVGL は 7168 バイトだが、FreeType を含む画面描画では
 * スタックが不足するため、アプリ画面用の余裕を持たせる。
 */
#define LVGL_TASK_STACK_SIZE 12288
#define SPLASH_SCREEN_DURATION_MS 1500

void app_main(void)
{
    ESP_LOGI(TAG, "Mounting FAT filesystem");
    /* マウント失敗時に同梱リソースやユーザー設定を消去しない。 */
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

    /* アプリ画面とフォントの使用量を見込み、LVGL 用メモリに余裕を持たせる。 */
    bsp_display_cfg_t display_cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * CONFIG_BSP_LCD_DRAW_BUF_HEIGHT,
#if CONFIG_BSP_LCD_DRAW_BUF_DOUBLE
        .double_buffer = true,
#endif
        .flags = {
            .buff_dma = true,
            /* SPI 転送バッファは DMA 対応の内部 SRAM に置く必要がある。 */
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
    app_splash_show();
    bsp_display_unlock();

    const esp_err_t system_result = system_bootstrap_init();
    if (system_result != ESP_OK) {
        ESP_LOGE(TAG, "System initialization failed: %s", esp_err_to_name(system_result));
    }

    vTaskDelay(pdMS_TO_TICKS(SPLASH_SCREEN_DURATION_MS));

    bsp_display_lock(0);
    app_shell_init();
    bsp_display_unlock();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

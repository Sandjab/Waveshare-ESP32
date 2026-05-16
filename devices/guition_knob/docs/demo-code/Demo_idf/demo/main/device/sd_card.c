#include  <string.h>
#include "sd_card.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include <dirent.h>
#include "esp_log.h"
#include "pinconfig.h"

#define MOUNT_POINT "/sdcard"

static const char *TAG = "SDCARD";
static sdmmc_card_t *card;

esp_err_t sdcard_mount()
{
    esp_err_t ret;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG,"Initializing SD card");

    ESP_LOGI(TAG,"Using SDMMC peripheral");

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk = PIN_SDMMC_SCK;
    slot_config.cmd = PIN_SDMMC_CMD;
    slot_config.d0 = PIN_SDMMC_D0;
    slot_config.d1 = PIN_SDMMC_D1;
    slot_config.d2 = PIN_SDMMC_D2;
    slot_config.d3 = PIN_SDMMC_D3;

    slot_config.width = 4;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_LOGI(TAG,"Mount filesystem");
    ret = esp_vfs_fat_sdmmc_mount(mount_point,&host,&slot_config,&mount_config,&card);

    if(ret != ESP_OK)
    {
        if(ret == ESP_FAIL)
        {
            ESP_LOGE(TAG,"Failed to mount filesystem");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }
    ESP_LOGI(TAG,"Mount successful");
    return ESP_OK;
}

int sdcard_filelist(const char *path ,const char (**file)[256])
{
    DIR *dir;
    struct dirent *entry;
    static char filename[20][256] = {0};
    memset(filename,0,sizeof(filename));
    dir = opendir(path);
    if(dir == NULL)
    {
        ESP_LOGE(TAG,"Open file failed");
        return 0;
    }

    int file_cnt = 0;
    while((entry = readdir(dir)) != NULL)
    {
        printf("%s\n",entry->d_name);
        snprintf(&filename[file_cnt][0],256,"%s",entry->d_name);
        file_cnt++;
        if(file_cnt >= 20)
            break;
    }
    closedir(dir);
    ESP_LOGI(TAG,"close dir");
    *file = filename;
    return file_cnt;
}

sdmmc_card_t *get_sdmmc_card()
{
    return card;
}
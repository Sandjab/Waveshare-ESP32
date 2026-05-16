#ifndef _SD_CARD_H
#define _SD_CARD_H
#include "esp_err.h"
#include "driver/sdmmc_host.h"

esp_err_t sdcard_mount();
sdmmc_card_t *get_sdmmc_card();

int sdcard_filelist(const char *path,const char (**file)[256]);

#endif
#include <Arduino.h>
#include "USB.h"
#include "USBMSC.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "knob_pins.h"

static USBMSC msc;
static sdmmc_card_t *card = nullptr;

static int32_t onRead(uint32_t lba, uint32_t offset, void *buf, uint32_t bufsize) {
    uint32_t count = bufsize / 512;
    if (sdmmc_read_sectors(card, buf, lba, count) != ESP_OK) {
        return -1;
    }
    return bufsize;
}

static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t *buf, uint32_t bufsize) {
    uint32_t count = bufsize / 512;
    if (sdmmc_write_sectors(card, buf, lba, count) != ESP_OK) {
        return -1;
    }
    return bufsize;
}

static bool init_sdmmc() {
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk = (gpio_num_t)SD_CLK_PIN;
    slot.cmd = (gpio_num_t)SD_CMD_PIN;
    slot.d0  = (gpio_num_t)SD_D0_PIN;
    slot.d1  = (gpio_num_t)SD_D1_PIN;
    slot.d2  = (gpio_num_t)SD_D2_PIN;
    slot.d3  = (gpio_num_t)SD_D3_PIN;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_err_t err = sdmmc_host_init();
    if (err != ESP_OK) {
        Serial.printf("sdmmc_host_init failed: %s\n", esp_err_to_name(err));
        return false;
    }

    err = sdmmc_host_init_slot(host.slot, &slot);
    if (err != ESP_OK) {
        Serial.printf("sdmmc_host_init_slot failed: %s\n", esp_err_to_name(err));
        return false;
    }

    card = (sdmmc_card_t *)malloc(sizeof(sdmmc_card_t));
    if (!card) {
        Serial.println("Failed to allocate card struct");
        return false;
    }

    err = sdmmc_card_init(&host, card);
    if (err != ESP_OK) {
        Serial.printf("sdmmc_card_init failed: %s\n", esp_err_to_name(err));
        free(card);
        card = nullptr;
        return false;
    }

    Serial.println("SD card initialized:");
    sdmmc_card_print_info(stdout, card);
    Serial.printf("  Sectors: %llu\n", (unsigned long long)card->csd.capacity);
    Serial.printf("  Size: %llu MB\n", (unsigned long long)card->csd.capacity * 512 / (1024 * 1024));

    return true;
}

static void init_usb_msc() {
    msc.vendorID("Wshare");
    msc.productID("SD Reader");
    msc.productRevision("1.0");
    msc.onRead(onRead);
    msc.onWrite(onWrite);
    msc.mediaPresent(true);
    msc.begin(card->csd.capacity, 512);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== Basic_SD_OTG: USB SD Card Reader ===");

    if (!init_sdmmc()) {
        Serial.println("SD card init failed. Insert card and reset.");
        msc.mediaPresent(false);
        msc.begin(0, 512);
        USB.begin();
        return;
    }

    init_usb_msc();
    USB.begin();
    Serial.println("USB MSC ready - card should appear as removable drive.");
}

void loop() {
}

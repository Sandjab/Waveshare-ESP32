#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_netif.h"

const char *ssid = "ESP32_AP";
const char *password = "12345678";

void setup() {
    Serial.begin(115200);
    
    // 设置 ESP32 为 AP 模式
    WiFi.softAP(ssid, password);
    Serial.println("WiFi AP Started");
    
    // 打印 AP 的 IP 地址
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());
}

void loop() {
    delay(5000); // 每 5 秒检查一次连接的设备
    printConnectedDevices();
}

void printConnectedDevices() {
    wifi_sta_list_t stationList;
    esp_netif_ip_info_t ip_info;
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");

    // 获取连接的设备列表
    esp_wifi_ap_get_sta_list(&stationList);
    
    Serial.print("Connected Devices: ");
    Serial.println(stationList.num);

    if (netif == nullptr) {
        Serial.println("Failed to get AP interface!");
        return;
    }

    for (int i = 0; i < stationList.num; i++) {
        Serial.printf("Device %d MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", i + 1,
                      stationList.sta[i].mac[0], stationList.sta[i].mac[1], stationList.sta[i].mac[2],
                      stationList.sta[i].mac[3], stationList.sta[i].mac[4], stationList.sta[i].mac[5]);
        
        // 获取设备的 IP 地址（ESP32 作为 AP 无法直接获取客户端 IP）
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            Serial.printf("Device %d IP: %s\n", i + 1, ip4addr_ntoa((const ip4_addr_t*)&ip_info.ip));
        }
    }
}

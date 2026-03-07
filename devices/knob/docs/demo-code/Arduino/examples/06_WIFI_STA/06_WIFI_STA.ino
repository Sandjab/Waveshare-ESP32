#include <WiFi.h>

const char* ssid = "PDCN";          // 修改为你的WiFi名称
const char* password = "1234567890";  // 修改为你的WiFi密码
void setup()
{
  WiFi.mode(WIFI_STA); // 设置为STA模式
  WiFi.begin(ssid, password);
  
  printf("Connecting to WiFi\n");
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }
  printf("IP Address: %s\n",WiFi.localIP().toString().c_str());
}
void loop()
{
  // 你的代码
}
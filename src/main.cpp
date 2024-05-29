#include <Arduino.h>
#include <WS2812FX.h>
#include <WiFi.h>
#include <WebServer.h>
#include <pgmspace.h>

#include "WiFiUser.h"
#include "ledweb.h"
 
const int resetPin = 0;                    //设置重置按键引脚,用于删除WiFi信息
int connectTimeOut_s = 15;                 //WiFi连接超时时间，单位秒

#define WEB_SERVER WebServer
#define ESP_RESET ESP.restart()

#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

#define LED_PIN 13                       // 0 = GPIO0, 2=GPIO2
#define LED_COUNT 7

#define WIFI_TIMEOUT 30000              // checks WiFi every ...ms. Reset after this time, if WiFi cannot reconnect.
#define HTTP_PORT 80

unsigned long auto_last_change = 0;
unsigned long last_wifi_check_time = 0;
String modes = "";
uint8_t myModes[] = {}; // *** optionally create a custom list of effect/mode numbers
bool auto_cycle = false;

WS2812FX ws2812fx = WS2812FX(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
WEB_SERVER server1(HTTP_PORT);



/*
 * Build <li> string for all modes.
 */
void modes_setup() {
  modes = "";
  uint8_t num_modes = sizeof(myModes) > 0 ? sizeof(myModes) : ws2812fx.getModeCount();
  for(uint8_t i=0; i < num_modes; i++) {
    uint8_t m = sizeof(myModes) > 0 ? myModes[i] : i;
    modes += "<li><a href='#'>";
    modes += ws2812fx.getModeName(m);
    modes += "</a></li>";
  }
}

/* #####################################################
#  Webserver Functions
##################################################### */

void srv_handle_not_found() {
  server1.send(404, "text/plain", "File Not Found");
}

void srv_handle_index_html() {
  server1.send_P(200,"text/html", index_html);
}

void srv_handle_main_js() {
  server1.send_P(200,"application/javascript", main_js);
}

void srv_handle_modes() {
  server1.send(200,"text/plain", modes);
}

void srv_handle_set() {
  for (uint8_t i=0; i < server1.args(); i++){
    if(server1.argName(i) == "c") {
      uint32_t tmp = (uint32_t) strtol(server1.arg(i).c_str(), NULL, 10);
      if(tmp <= 0xFFFFFF) {
        ws2812fx.setColor(tmp);
      }
    }

    if(server1.argName(i) == "m") {
      uint8_t tmp = (uint8_t) strtol(server1.arg(i).c_str(), NULL, 10);
      uint8_t new_mode = sizeof(myModes) > 0 ? myModes[tmp % sizeof(myModes)] : tmp % ws2812fx.getModeCount();
      ws2812fx.setMode(new_mode);
      auto_cycle = false;
      Serial.print("mode is "); Serial.println(ws2812fx.getModeName(ws2812fx.getMode()));
    }

    if(server1.argName(i) == "b") {
      if(server1.arg(i)[0] == '-') {
        ws2812fx.setBrightness(ws2812fx.getBrightness() * 0.8);
      } else if(server1.arg(i)[0] == ' ') {
        ws2812fx.setBrightness(min(max(ws2812fx.getBrightness(), 5) * 1.2, 255));
      } else { // set brightness directly
        uint8_t tmp = (uint8_t) strtol(server1.arg(i).c_str(), NULL, 10);
        ws2812fx.setBrightness(tmp);
      }
      Serial.print("brightness is "); Serial.println(ws2812fx.getBrightness());
    }

    if(server1.argName(i) == "s") {
      if(server1.arg(i)[0] == '-') {
        ws2812fx.setSpeed(max(ws2812fx.getSpeed(), 5) * 1.2);
      } else if(server1.arg(i)[0] == ' ') {
        ws2812fx.setSpeed(ws2812fx.getSpeed() * 0.8);
      } else {
        uint16_t tmp = (uint16_t) strtol(server1.arg(i).c_str(), NULL, 10);
        ws2812fx.setSpeed(tmp);
      }
      Serial.print("speed is "); Serial.println(ws2812fx.getSpeed());
    }

    if(server1.argName(i) == "a") {
      if(server1.arg(i)[0] == '-') {
        auto_cycle = false;
      } else {
        auto_cycle = true;
        auto_last_change = 0;
      }
    }
  }
  server1.send(200, "text/plain", "OK");
}



void setup() {
  Serial.begin(115200);                //波特率

  LEDinit();  //LED用于显示WiFi状态
  
  Serial.println("Wifi setup");
  connectToWiFi(connectTimeOut_s);     //连接wifi，传入的是wifi连接等待时间15s

  modes_setup();
  Serial.println("WS2812FX setup");
  ws2812fx.init();
  ws2812fx.setMode(FX_MODE_STATIC);
  ws2812fx.setColor(0xFF5900);
  ws2812fx.setSpeed(1000);
  ws2812fx.setBrightness(128);
  ws2812fx.start();

  Serial.println("HTTP server setup");
  server1.on("/", srv_handle_index_html);
  server1.on("/main.js", srv_handle_main_js);
  server1.on("/modes", srv_handle_modes);
  server1.on("/set", srv_handle_set);
  server1.onNotFound(srv_handle_not_found);
  server1.begin();
  Serial.println("HTTP server started.");

  Serial.println("ready!");
}

void loop() {
  unsigned long now = millis();

  if (!digitalRead(resetPin)) //长按5秒(P0)清除网络配置信息
  {
    delay(5000);
    if (!digitalRead(resetPin)) 
    {
      Serial.println("\n按键已长按5秒,正在清空网络连保存接信息.");
      restoreWiFi();     //删除保存的wifi信息
      ESP.restart();     //重启复位esp32
      Serial.println("已重启设备.");
    }
  }

  server1.handleClient();
  ws2812fx.service();
  
  checkDNS_HTTP();                  //检测客户端DNS&HTTP请求，也就是检查配网页面那部分
  checkConnect(true);               //检测网络连接状态，参数true表示如果断开重新连接

  if(auto_cycle && (now - auto_last_change > 10000)) { // cycle effect mode every 10 seconds
    uint8_t next_mode = (ws2812fx.getMode() + 1) % ws2812fx.getModeCount();
    if(sizeof(myModes) > 0) { // if custom list of modes exists
      for(uint8_t i=0; i < sizeof(myModes); i++) {
        if(myModes[i] == ws2812fx.getMode()) {
          next_mode = ((i + 1) < sizeof(myModes)) ? myModes[i + 1] : myModes[0];
          break;
        }
      }
    }
    ws2812fx.setMode(next_mode);
    Serial.print("mode is "); Serial.println(ws2812fx.getModeName(ws2812fx.getMode()));
    auto_last_change = now;
  }
}




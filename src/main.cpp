#include <Arduino.h>
#include <WS2812FX.h>
#include <WiFi.h>
#include <WebServer.h>
#include <pgmspace.h>

#include "WiFiUser.h"
 
const int resetPin = 0;                    //设置重置按键引脚,用于删除WiFi信息
int connectTimeOut_s = 15;                 //WiFi连接超时时间，单位秒


#define WEB_SERVER WebServer
#define ESP_RESET ESP.restart()

char index_html[] PROGMEM = R"=====(
<!doctype html>
<html lang='en' dir='ltr'>
<head>
  <meta http-equiv='Content-Type' content='text/html; charset=utf-8' />
  <meta name='viewport' content='width=device-width, initial-scale=1.0' />
  <title>WS2812FX Control</title>
  <script type='text/javascript' src='main.js'></script>

  <style>
    body {
      font-family:Arial,sans-serif;
      margin:10px;
      padding:0;
      background-color:#202020;
      color:#909090;
      text-align:center;
    }

    .flex-row {
      display:flex;
      flex-direction:row;
    }

    .flex-row-wrap {
      display:flex;
      flex-direction:row;
      flex-wrap:wrap;
    }

    .flex-col {
      display:flex;
      flex-direction:column;
      align-items:center;
    }

    input[type='text'] {
      background-color: #d0d0d0;
      color:#404040;
    }

    ul {
      list-style-type: none;
    }

    ul li a {
      display:block;
      margin:3px;
      padding:10px;
      border:2px solid #404040;
      border-radius:5px;
      color:#909090;
      text-decoration:none;
    }

    ul#modes li a {
      min-width:220px;
    }

    ul.control li a {
      min-width:60px;
      min-height:24px;
    }

    ul.control {
      display:flex;
      flex-direction:row;
      justify-content: flex-end;
      align-items: center;
      padding: 0px;
    }

    ul li a.active {
      border:2px solid #909090;
    }
  </style>
</head>
<body>
  <h1>WS2812FX Control</h1>
  <div class='flex-row'>

    <div class='flex-col'>
      <div><canvas id='color-canvas' width='360' height='360'></canvas><br/></div>
      <div><input type='text' id='color-value' oninput='onColor(event, this.value)'/></div>

      <div>
        <ul class='control'>
          <li>Brightness:</li>
          <li><a href='#' onclick="onBrightness(event, '-')">&#9788;</a></li>
          <li><a href='#' onclick="onBrightness(event, '+')">&#9728;</a></li>
        </ul>

        <ul class='control'>
          <li>Speed:</li>
          <li><a href='#' onclick="onSpeed(event, '-')">&#8722;</a></li>
          <li><a href='#' onclick="onSpeed(event, '+')">&#43;</a></li>
        </ul>

        <ul class='control'>
          <li>Auto cycle:</li>
          <li><a href='#' onclick="onAuto(event, '-')">&#9632;</a></li>
          <li><a href='#' onclick="onAuto(event, '+')">&#9658;</a></li>
        </ul>
      </div>
    </div>

    <div>
      <ul id='modes' class='flex-row-wrap'>
    </div>
  </div>
</body>
</html>
)=====";

char main_js[] PROGMEM = R"=====(

var activeButton = null;
var colorCanvas = null;

window.addEventListener('DOMContentLoaded', (event) => {
  // init the canvas color picker
  colorCanvas = document.getElementById('color-canvas');
  var colorctx = colorCanvas.getContext('2d');

  // Create color gradient
  var gradient = colorctx.createLinearGradient(0, 0, colorCanvas.width - 1, 0);
  gradient.addColorStop(0,    "rgb(255,   0,   0)");
  gradient.addColorStop(0.16, "rgb(255,   0, 255)");
  gradient.addColorStop(0.33, "rgb(0,     0, 255)");
  gradient.addColorStop(0.49, "rgb(0,   255, 255)");
  gradient.addColorStop(0.66, "rgb(0,   255,   0)");
  gradient.addColorStop(0.82, "rgb(255, 255,   0)");
  gradient.addColorStop(1,    "rgb(255,   0,   0)");

  // Apply gradient to canvas
  colorctx.fillStyle = gradient;
  colorctx.fillRect(0, 0, colorCanvas.width - 1, colorCanvas.height - 1);

  // Create semi transparent gradient (white -> transparent -> black)
  gradient = colorctx.createLinearGradient(0, 0, 0, colorCanvas.height - 1);
  gradient.addColorStop(0,    "rgba(255, 255, 255, 1)");
  gradient.addColorStop(0.48, "rgba(255, 255, 255, 0)");
  gradient.addColorStop(0.52, "rgba(0,     0,   0, 0)");
  gradient.addColorStop(1,    "rgba(0,     0,   0, 1)");

  // Apply gradient to canvas
  colorctx.fillStyle = gradient;
  colorctx.fillRect(0, 0, colorCanvas.width - 1, colorCanvas.height - 1);

  // setup the canvas click listener
  colorCanvas.addEventListener('click', (event) => {
    var imageData = colorCanvas.getContext('2d').getImageData(event.offsetX, event.offsetY, 1, 1);

    var selectedColor = 'rgb(' + imageData.data[0] + ',' + imageData.data[1] + ',' + imageData.data[2] + ')'; 
    //console.log('click: ' + event.offsetX + ', ' + event.offsetY + ', ' + selectedColor);
    document.getElementById('color-value').value = selectedColor;

    selectedColor = imageData.data[0] * 65536 + imageData.data[1] * 256 + imageData.data[2];
    submitVal('c', selectedColor);
  });

  // get list of modes from ESP
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
   if (xhttp.readyState == 4 && xhttp.status == 200) {
     document.getElementById('modes').innerHTML = xhttp.responseText;
     modes = document.querySelectorAll('ul#modes li a');
     modes.forEach(initMode);
   }
  };
  xhttp.open('GET', 'modes', true);
  xhttp.send();
});

function initMode(mode, index) {
  mode.addEventListener('click', (event) => onMode(event, index));
}

function onColor(event, color) {
  event.preventDefault();
  var match = color.match(/rgb\(([0-9]*),([0-9]*),([0-9]*)\)/);
  if(match) {
    var colorValue = Number(match[1]) * 65536 + Number(match[2]) * 256 + Number(match[3]);
    //console.log('onColor:' + match[1] + "," + match[2] + "," + match[3] + "," + colorValue);
    submitVal('c', colorValue);
  }
}

function onMode(event, mode) {
  event.preventDefault();
  if(activeButton) activeButton.classList.remove('active')
  activeButton = event.target;
  activeButton.classList.add('active');
  submitVal('m', mode);
}

function onBrightness(event, dir) {
  event.preventDefault();
  submitVal('b', dir);
}

function onSpeed(event, dir) {
  event.preventDefault();
  submitVal('s', dir);
}

function onAuto(event, dir) {
  event.preventDefault();
  submitVal('a', dir);
}

function submitVal(name, val) {
  var xhttp = new XMLHttpRequest();
  xhttp.open('GET', 'set?' + name + '=' + val, true);
  xhttp.send();
}
)=====";

// #define WIFI_SSID "5akura1"
// #define WIFI_PASSWORD "zhaoyonghao0429"

//#define STATIC_IP                       // uncomment for static IP, set IP below
#ifdef STATIC_IP
  IPAddress ip(192,168,0,123);
  IPAddress gateway(192,168,0,1);
  IPAddress subnet(255,255,255,0);
#endif

// QUICKFIX...See https://github.com/esp8266/Arduino/issues/263
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


// #define LED_COUNT 66
// #define LED_PIN 12

// WS2812FX ws2812fx = WS2812FX(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);


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

// /*
//  * Connect to WiFi. If no connection is made within WIFI_TIMEOUT, ESP gets resettet.
//  */
// void wifi_setup() {
//   Serial.println();
//   Serial.print("Connecting to ");
//   Serial.println(WIFI_SSID);

//   WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
//   WiFi.mode(WIFI_STA);
//   #ifdef STATIC_IP  
//     WiFi.config(ip, gateway, subnet);
//   #endif

//   unsigned long connect_start = millis();
//   while(WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.print(".");

//     if(millis() - connect_start > WIFI_TIMEOUT) {
//       Serial.println();
//       Serial.print("Tried ");
//       Serial.print(WIFI_TIMEOUT);
//       Serial.print("ms. Resetting ESP now.");
//       ESP_RESET;
//     }
//   }

//   Serial.println("");
//   Serial.println("WiFi connected");  
//   Serial.print("IP address: ");
//   Serial.println(WiFi.localIP());
//   Serial.println();
// }







void setup() {
  Serial.begin(115200);                //波特率
  LEDinit();                           //LED用于显示WiFi状态

  connectToWiFi(connectTimeOut_s);     //连接wifi，传入的是wifi连接等待时间15s

  modes_setup();
  Serial.println("WS2812FX setup");
  ws2812fx.init();
  ws2812fx.setMode(FX_MODE_STATIC);
  ws2812fx.setColor(0xFF5900);
  ws2812fx.setSpeed(1000);
  ws2812fx.setBrightness(128);
  ws2812fx.start();

  Serial.println("Wifi setup");
  //wifi_setup();
 
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
    delay(5000);              //哈哈哈哈，这样不准确
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

  // if(now - last_wifi_check_time > WIFI_TIMEOUT) {
  //   Serial.print("Checking WiFi... ");
  //   if(WiFi.status() != WL_CONNECTED) {
  //     Serial.println("WiFi connection lost. Reconnecting...");
  //     wifi_setup();
  //   } else {
  //     Serial.println("OK");
  //   }
  //   last_wifi_check_time = now;
  // }

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




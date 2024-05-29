#include "WiFiUser.h"
 
const byte DNS_PORT = 53;                  //设置DNS端口号
const int webPort = 81;                    //设置Web端口号
 
const char* AP_SSID  = "ESP32-WS2812s";        //设置AP热点名称
//const char* AP_PASS  = "";               //这里不设置设置AP热点密码
 
const char* HOST_NAME = "ESP32_WS2812s";        //设置设备名
String scanNetworksID = "";                //用于储存扫描到的WiFi ID
 
IPAddress apIP(192, 168, 4, 1);            //设置AP的IP地址
 
String wifi_ssid = "";                     //暂时存储wifi账号密码
String wifi_pass = "";                     //暂时存储wifi账号密码
 
const int LED = 2;                         //设置LED引脚
 
DNSServer dnsServer;                       //创建dnsServer实例
WebServer server(webPort);                 //开启web服务, 创建TCP SERVER,参数: 端口号,最大连接数

char ROOT_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32-WS2812s WiFi Setup</title>
    <style>
        body {
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            background-color: #f5f5f5;
            font-family: Arial, sans-serif;
            color: #333;
        }
        .container {
            background-color: #fff;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
            max-width: 300px;
            width: 100%;
        }
        h1 {
            margin-bottom: 20px;
            font-size: 24px;
            text-align: center;
        }
        label {
            display: block;
            margin-bottom: 5px;
            font-weight: bold;
        }
        input[type="text"], input[type="pass"] {
            width: 100%;
            padding: 10px;
            margin-bottom: 15px;
            border: 1px solid #ddd;
            border-radius: 4px;
            box-sizing: border-box;
        }
        input[type="submit"] {
            width: 100%;
            padding: 10px;
            background-color: #333;
            color: #fff;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 16px;
        }
        input[type="submit"]:hover {
            background-color: #555;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>WiFi Setup</h1>
        <form action="configwifi" method="POST">
            <label for="ssid">SSID</label>
            <input type="text" name="ssid" required>
            <label for="password">Password</label>
            <input type="pass" name="pass" required>
            <input type="submit" value="Connect">
            <p><label>Nearby wifi:</label></P>
        </form>
)=====";

char ROOTOK_HTML[] PROGMEM = R"=====(
<meta charset='UTF-8'>
<style>
    body {
        font-family: Arial, sans-serif;
        background-color: #f5f5f5;
        color: #333;
        display: flex;
        justify-content: center;
        align-items: center;
        height: 100vh;
        text-align: center;
        padding: 20px;
        box-sizing: border-box;
    }
    .message-box {
        background-color: #fff;
        padding: 20px;
        border-radius: 8px;
        box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
    }
</style>
<div class="message-box">
    <p>SSID: <strong>
)=====";

char ROOTERRORSSID_HTML[] PROGMEM = R"=====(
<meta charset='UTF-8'>
<style>
    body {
        font-family: Arial, sans-serif;
        background-color: #f5f5f5;
        color: #333;
        display: flex;
        justify-content: center;
        align-items: center;
        height: 100vh;
        text-align: center;
        padding: 20px;
        box-sizing: border-box;
    }
    .message-box {
        background-color: #fff;
        padding: 20px;
        border-radius: 8px;
        box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
    }
</style>
<div class="message-box">
    <p><strong>错误：</strong>未找到WIFI SSID。</p>
</div>
)=====";

char ROOTERRORPWD_HTML[] PROGMEM = R"=====(
<meta charset='UTF-8'>
<style>
    body {
        font-family: Arial, sans-serif;
        background-color: #f5f5f5;
        color: #333;
        display: flex;
        justify-content: center;
        align-items: center;
        height: 100vh;
        text-align: center;
        padding: 20px;
        box-sizing: border-box;
    }
    .message-box {
        background-color: #fff;
        padding: 20px;
        border-radius: 8px;
        box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
    }
</style>
<div class="message-box">
    <p><strong>错误：</strong>未找到PASSWARD。</p>
</div>
)=====";

/*
 * 处理网站根目录的访问请求
 */
void handleRoot() 
{
  if (server.hasArg("selectSSID")) {
    server.send(200, "text/html", ROOT_HTML + scanNetworksID + "</div></body></html>");   //scanNetWprksID是扫描到的wifi  + scanNetworksID "
  } else {
    server.send(200, "text/html", ROOT_HTML + scanNetworksID + "</div></body></html>");   // + scanNetworksID + "</body></html>"
  }
}
 
/*
 * 提交数据后的提示页面
 */
void handleConfigWifi()               //返回http状态
{
  if (server.hasArg("ssid"))          //判断是否有账号参数
  {
    Serial.print("got ssid:");
    wifi_ssid = server.arg("ssid");   //获取html表单输入框name名为"ssid"的内容
 
    Serial.println(wifi_ssid);
  } 
  else                                //没有参数
  { 
    Serial.println("error, not found ssid");
    server.send(200, "text/html", ROOTERRORSSID_HTML); //返回错误页面
    return;
  }
  //密码与账号同理
  if (server.hasArg("pass")) 
  {
    Serial.print("got password:");
    wifi_pass = server.arg("pass");  //获取html表单输入框name名为"pwd"的内容
    Serial.println(wifi_pass);
  } 
  else 
  {
    Serial.println("error, not found password");
    server.send(200, "text/html", ROOTERRORPWD_HTML);
    return;
  }
  server.send(200, "text/html", ROOTOK_HTML + wifi_ssid + "</strong></p><p>Password: <strong>" + wifi_pass + "</strong></p><p>已取得WiFi信息，正在尝试连接，请手动关闭此页面。</p></div>"); //返回保存成功页面
  delay(2000);
  WiFi.softAPdisconnect(true);     //参数设置为true，设备将直接关闭接入点模式，即关闭设备所建立的WiFi网络。
  server.close();                  //关闭web服务
  WiFi.softAPdisconnect();         //在不输入参数的情况下调用该函数,将关闭接入点模式,并将当前配置的AP热点网络名和密码设置为空值.
  Serial.println("WiFi Connect SSID:" + wifi_ssid + "  PASS:" + wifi_pass);
 
  if (WiFi.status() != WL_CONNECTED)    //wifi没有连接成功
  {
    Serial.println("开始调用连接函数connectToWiFi()..");
    connectToWiFi(connectTimeOut_s);
  } 
  else {
    Serial.println("提交的配置信息自动连接成功..");
  }
}
 
/*
 * 处理404情况的函数'handleNotFound'
 */
void handleNotFound()           // 当浏览器请求的网络资源无法在服务器找到时通过此自定义函数处理
{           
  handleRoot();                 //访问不存在目录则返回配置页面
  //   server.send(404, "text/plain", "404: Not found");
}
 
/*
 * 进入AP模式
 */
void initSoftAP() {
  WiFi.mode(WIFI_AP);                                           //配置为AP模式
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));   //设置AP热点IP和子网掩码
  if (WiFi.softAP(AP_SSID))                                     //开启AP热点,如需要密码则添加第二个参数
  {                           
    //打印相关信息
    Serial.println("ESP-32S SoftAP is right.");
    Serial.print("Soft-AP IP address = ");
    Serial.println(WiFi.softAPIP());                                                //接入点ip
    Serial.println(String("MAC address = ")  + WiFi.softAPmacAddress().c_str());    //接入点mac
  } 
  else                                                  //开启AP热点失败
  { 
    Serial.println("WiFiAP Failed");
    delay(1000);
    Serial.println("restart now...");
    ESP.restart();                                      //重启复位esp32
  }
}
 
/*
 * 开启DNS服务器
 */
void initDNS() 
{
  if (dnsServer.start(DNS_PORT, "*", apIP))   //判断将所有地址映射到esp32的ip上是否成功
  {
    Serial.println("start dnsserver success.");
  } else {
    Serial.println("start dnsserver failed.");
  }
}
 
/*
 * 初始化WebServer
 */
void initWebServer() 
{
  if (MDNS.begin("esp32"))      //给设备设定域名esp32,完整的域名是esp32.local
  {
    Serial.println("MDNS responder started");
  }
  //必须添加第二个参数HTTP_GET，以下面这种格式去写，否则无法强制门户
  server.on("/", HTTP_GET, handleRoot);                      //  当浏览器请求服务器根目录(网站首页)时调用自定义函数handleRoot处理，设置主页回调函数，必须添加第二个参数HTTP_GET，否则无法强制门户
  server.on("/configwifi", HTTP_POST, handleConfigWifi);     //  当浏览器请求服务器/configwifi(表单字段)目录时调用自定义函数handleConfigWifi处理
                                                            
  server.onNotFound(handleNotFound);                         //当浏览器请求的网络资源无法在服务器找到时调用自定义函数handleNotFound处理
 
  server.begin();                                           //启动TCP SERVER
 
  Serial.println("WebServer started!");
}
 
/*
 * 扫描附近的WiFi，为了显示在配网界面
 */
bool scanWiFi() {
  Serial.println("scan start");
  Serial.println("--------->");
  // 扫描附近WiFi
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
    Serial.println("no networks found");
    scanNetworksID = "no networks found";
    return false;
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
      scanNetworksID += "<P>" + WiFi.SSID(i) + "</P>";
      delay(10);
    }
    return true;
  }
}
 
/*
 * 连接WiFi
 */
void connectToWiFi(int timeOut_s) {
  WiFi.hostname(HOST_NAME);             //设置设备名
  Serial.println("进入connectToWiFi()函数");
  WiFi.mode(WIFI_STA);                        //设置为STA模式并连接WIFI
  WiFi.setAutoConnect(true);                  //设置自动连接    
  
  if (wifi_ssid != "")                        //wifi_ssid不为空，意味着从网页读取到wifi
  {
    Serial.println("用web配置信息连接.");
    WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str()); //c_str(),获取该字符串的指针
    wifi_ssid = "";
    wifi_pass = "";
  } 
  else                                        //未从网页读取到wifi
  {
    Serial.println("用nvs保存的信息连接.");
    WiFi.begin();                             //begin()不传入参数，默认连接上一次连接成功的wifi
  }
 
  int Connect_time = 0;                       //用于连接计时，如果长时间连接不成功，复位设备
  while (WiFi.status() != WL_CONNECTED)       //等待WIFI连接成功
  {  
    Serial.print(".");                        //一共打印30个点点
    digitalWrite(LED, !digitalRead(LED));     
    delay(500);
    Connect_time ++;
                                       
    if (Connect_time > 2 * timeOut_s)         //长时间连接不上，重新进入配网页面
    { 
      digitalWrite(LED, LOW);
      Serial.println("");                     //主要目的是为了换行符
      Serial.println("WIFI autoconnect fail, start AP for webconfig now...");
      wifiConfig();                           //开始配网功能
      return;                                 //跳出 防止无限初始化
    }
  }
  
  if (WiFi.status() == WL_CONNECTED)          //如果连接成功
  {
    Serial.println("WIFI connect Success");
    Serial.printf("SSID:%s", WiFi.SSID().c_str());
    Serial.printf(", PSW:%s\r\n", WiFi.psk().c_str());
    Serial.print("LocalIP:");
    Serial.print(WiFi.localIP());
    Serial.print(" ,GateIP:");
    Serial.println(WiFi.gatewayIP());
    Serial.print("WIFI status is:");
    Serial.print(WiFi.status());
    digitalWrite(LED, HIGH);
    server.stop();                            //停止开发板所建立的网络服务器。
  }
}
 
/*
 * 配置配网功能
 */
void wifiConfig() 
{
  initSoftAP();   
  initDNS();        
  initWebServer();  
  scanWiFi();       
}
 
 
/*
 * 删除保存的wifi信息，这里的删除是删除存储在flash的信息。删除后wifi读不到上次连接的记录，需重新配网
 */
void restoreWiFi() {
  delay(500);
  esp_wifi_restore();  //删除保存的wifi信息
  Serial.println("连接信息已清空,准备重启设备..");
  delay(10);
  blinkLED(LED, 5, 500); //LED闪烁5次         //关于LED，不需要可删除 
  digitalWrite(LED, LOW);                    //关于LED，不需要可删除
}
 
/*
 * 检查wifi是否已经连接
 */
void checkConnect(bool reConnect) 
{
  if (WiFi.status() != WL_CONNECTED)           //wifi连接失败
  {
    if (digitalRead(LED) != LOW) 
      digitalWrite(LED, LOW);
    if (reConnect == true && WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA ) 
    {
      Serial.println("WIFI未连接.");
      Serial.println("WiFi Mode:");
      Serial.println(WiFi.getMode());
      Serial.println("正在连接WiFi...");
      connectToWiFi(connectTimeOut_s);          //连接wifi函数 
    }
  } 
  else if (digitalRead(LED) != HIGH)  
    digitalWrite(LED, HIGH);                    //wifi连接成功
}
 
/*
 * LED闪烁函数        //用不上LED可删除
 */
void blinkLED(int led, int n, int t) 
{
  for (int i = 0; i < 2 * n; i++) 
  {
    digitalWrite(led, !digitalRead(led));
    delay(t);
  }
}
 
 
/*
 * LED初始化
 */
void LEDinit()
{
  pinMode(LED, OUTPUT);                 //配置LED口为输出口
  digitalWrite(LED, LOW);               //初始灯灭
}
 
/*
 * 检测客户端DNS&HTTP请求
 */
void checkDNS_HTTP()
{
  dnsServer.processNextRequest();   //检查客户端DNS请求
  server.handleClient();            //检查客户端(浏览器)http请求
}
 
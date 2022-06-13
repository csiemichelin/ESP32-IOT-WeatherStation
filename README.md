# ESP32-IOT-WeatherStation
## 環境配置
* ubuntu平臺下面安裝VScoode。
* 在VScode安裝Espressif IDF，方便建立esp32的開發環境：
https://blog.csdn.net/weixin_45652444/article/details/118728136
* 使用NodeMCU-32S Lua WiFi 物聯網開發板 ESP32 最新版 ESP32-S的晶片
* 使用DHT22做為環境感測器，使用ESP32的DHT22開源函數庫：https://github.com/gosouth/DHT22.git
* Download Node.js:
https://www.youtube.com/watch?v=OMhMnj7SBRQ
* 利用Float charts函數庫來視覺化感測器資料：
https://www/flowcharts.org/
* F1(Views-problems) -> build -> port(/dev/ttyUSB0) -> device target選esp32 -> fash(URAT)-> F1的flash(dfu) your project -> monitor監看
## 硬體接線
DHT22模組要接到ESP32開發板的IO26腳位
## 設計動機
本專案製作以物聯網為基礎去設計天氣監控系統，現在IOT相關的嵌入式系統相當火熱，所以想藉由系統軟體設計這門課，來設計簡單的IOT project，此專案實現自動更新氣象站的web和透過Node.js做為網路伺服器來處理客戶端（web）的請求，另外也會用到socket.io來把訊系廣播給所有請求者，而socket.io運用了WebSocket的技術在瀏覽器上做到全雙工的TCP連線。
## 系統流程圖
![](https://i.imgur.com/FJnz3lr.png)
## 編寫ESP程式
新增一個名為weatherweb的專案，主程式是在main資料夾中的main.c，程式碼說明如下：
1. 初始化所有必要的標頭，如下：
```c=
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <esp_http_server.h>

#include "DHT22.h"
```
2. 接著定義主程式中的app_main()函數，定義httpd_handle_t將server作為網路伺服器的變數，再來呼叫initialize_wifi將sever變數當作參數傳入，來初始化Wifi服務：
```c=
void app_main()
{
    static httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(nvs_flash_init());
    initialize_wifi(&server); 
}
```
3. 實作initialize_wifi函數來連接指定Wifi並啟動服務，根據要連線的Wifi熱點來修改SSID與password。把event_handler函數送入esp_event_loop_init函數來監聽相關的Wifi服務事件：
```c=
static void initialize_wifi(void *arg)
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, arg));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    //更改自己得wifi名稱密碼
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "fishwifi",
            .password = "00000000",
        },
    };
```
> event_handler函數會監聽以下事件：SYSTEM_EVENT_STA_START , SYSTEM_EVENT_STA_GOT_IP , SYSTEM_EVENT_STA_DISCONNECTED。
> 接著再根據以下步驟來處理傳入的事件：
> 1. 當收到SYSTEM_EVENT_STA_START事件時，呼叫esp_wifi_connect()來連上指定的Wifi網路：
```c=
/* 監聽事件的發生 */
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    httpd_handle_t *server = (httpd_handle_t *) ctx;

    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
        //連上指定的wifi
        ESP_ERROR_CHECK(esp_wifi_connect());
        break;
```
> 2. 當收到SYSTEM_EVENT_STA_GOT_IP事件時，呼叫start_webserver()來啟動網路伺服器：
```c=
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
        ESP_LOGI(TAG, "Got IP: '%s'",ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));

        if (*server == NULL) {
            //來啟動網路伺服器
            *server = start_webserver();
        }
        break;
```
> 3. 當收到SYSTEM_EVENT_STA_DISCONNECTED事件時，呼叫stop_webserver()函數來停止網路伺服器，呼叫esp_wifi_connect()函數來重新連接Wifi:
```c=
    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
        ESP_ERROR_CHECK(esp_wifi_connect());

        if (*server) {
            //來停止網路伺服器
            stop_webserver(*server);
            *server = NULL;
        }
        break;
```
> 4. 再來實作start_webserver()與stop_webserver()函數，當網路伺服器啟動時，使用httpd_register_uri_handler()函數並送入weather_temp變數來註冊/temp HTTP的網頁client請求。當程式收到/temp HTTP請求時，就呼叫weather_temp變數中的函數：
```c=
/* 啟動 Web 服務器的函數 */
httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    //啟動 httpd server，註冊/temp請求，故把其當作httpd_register_uri_handler的參數
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        //送入weather_temp變數註冊/temp http請求
        httpd_register_uri_handler(server, &weather_temp);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}
```
> 5. stop_webserver()函數用來停止網路伺服器服務，在此也同樣會呼叫httpd_stop()來停止網路伺服器：
```c=
/* 停止 Web 服務器的函數 */
void stop_webserver(httpd_handle_t server)
{
    //停止 the httpd server
    httpd_stop(server);
}
```
> 6. 定義httpd_uri_t型態的變數weather_temp，再把/temp HTTP請求定義在.uri中，再將temperature_get_handler()函數加到.handler中：
```c=
httpd_uri_t weather_temp = {
    .uri       = "/temp",
    .method    = HTTP_GET,
    .handler   = temperature_get_handler,
    .user_ctx  = "ESP32 Weather System"
};
```
> 7. 在temperature_get_handler()函數中，呼叫getHumidity()和getTemperature()函數來讀取DHT22模組的溫度與資料：
```c=
esp_err_t temperature_get_handler(httpd_req_t *req)
{
    setDHTgpio( 26 );
    ESP_LOGI(TAG, "Request headers lost");

    char tmp_buff[256];

    int ret = readDHT();
	errorHandler(ret);

    //將溫度值存入tmp_buff中，之後會進行server,client端間的傳遞
    sprintf(tmp_buff, "%.1f" , getTemperature());	
    
    //monitor測試用
    printf( "Humidity: %.1f%% , ", getHumidity() );
    printf( "Tmp: %.1f^C\n", getTemperature() );

```
> 8. 呼叫httpd_resp_send()函數把溫度資料發送給網頁的client請求者：
```c=
    //把溫度發送給請求者
    httpd_resp_send(req, tmp_buff, strlen(tmp_buff));

    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}
```
4. 使用esp_wifi_set_mode函數將ESP32的Wifi模式設定成WIFI_MODE_STA，所有Wifi設定都會傳入到esp_wifi_set_config函數來執行ESP32的Wifi服務。呼叫esp_wifi_start()來啟動Wifi服務:
```c=
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}
```
## 編寫Node.js程式
建立weatherJS資料夾，之中需要以下檔案：
* App.js：Node.js主程式
* package.json＆package-lock.json：Node.js專案的設定檔
* weather.html：用於呈現感測器資料的HTML網頁
* flot資料夾：用來視覺化感測器資料
1. 開啟terminal輸入以下指令，在此專案中安裝 Socket.io：
```c=
$ npm install
```
2. **編寫weather.html**，在此會用到download好的Flot函數庫來視覺化感測器資料。Socket.io則負責取得感測器資料來集中廣播給所有請求者，因次要在weather.html中加入Socket.io相關的語法：
```htmlmixed=
var socket = io.connect();
        var items = [];
        var counter = 0;

        socket.on('data', function (data) {
            items.push([counter, data]);
            counter = counter + 1;
            if (items.length > 20)
                items.shift();
            $.plot($("#placeholder"), [items]);
        });
```
3. **編寫App.js程式**，先把esp32_req變數內容修改為ESP32開發板提供的IP位址。**Socket.io會連續呼叫ESP32網路伺服器**來取得感測器資料：
```javascript=
var http = require('http');
var path = require('path');
var fs = require('fs');

// 修改port
var port = process.env.PORT || 8016; //8345;
// ESP32 server
var esp32_req = "http://192.168.43.98/temp";
```
4. 使用http.createServer()函數來啟動網路伺服器並處理HTPP請求，在此會動有所有JavaScript與CSS檔案的路徑：
```javascript=
//啟動網路伺服器並處理HTPP請求
var srv = http.createServer(function (req, res) {

    var filePath = '.' + req.url;
    if (filePath == './')
        filePath = './weather.html';

    var extname = path.extname(filePath);
    var contentType = 'text/html';
    switch (extname) {
        case '.js':
            contentType = 'text/javascript';
            break;
        case '.css':
            contentType = 'text/css';
            break;
    }
```
5. 檢查被請求的檔案。如果被請求的檔案可用的話，就讀取並將其發送給client web端，否則就在HTTP標頭加入錯誤訊息：
```javascript=
    fs.exists(filePath, function(exists) {

        if (exists) {
            fs.readFile(filePath, function(error, content) {
                if (error) {
                    res.writeHead(500);
                    res.end();
                } else {
                    res.writeHead(200, {
                        'Content-Type' : contentType
                    });
                    res.end(content, 'utf-8');
                }
            });
        } else {
            res.writeHead(404);
            res.end();
        }
    });

});
```
6. 伺服器會透過listen()函數來監聽指定的port:
```javascript=
gw_srv = require('socket.io')(srv);
srv.listen(port);
console.log('Server running at http://127.0.0.1:' + port +'/');
```
7. 使用Socket.io來監聽'connection'事件。接著讀取client web端的請求：
```javascript=
gw_srv.sockets.on('connection', function(socket) {
    var dataPusher = setInterval(function () {
        //socket.volatile.emit('data', Math.random() * 100);
        http.get(esp32_req, (resp) => {
        let data = '';
```
8. 監聽'data'事件來讀取傳入的資料，以及'end'代表client web端以讀取完畢：
```javascript=
        // 已經收到一段資料
        resp.on('data', (chunk) => {
            data += chunk;
        });

        // 已經收到完整回應，將結果顯示出來
        resp.on('end', () => {
            console.log('Received data: ',data);
            socket.volatile.emit('data', data);
            //console.log(JSON.parse(data).explanation);
        });

        }).on("error", (err) => {
        console.log("Error: " + err.message);
        });
    }, 2000);
```
## Demo 
1. 使用瀏覽器開啟http://<ESP32的IP位址>/temp，可觀察此網路伺服器的資料：
![](https://i.imgur.com/YuWlWKL.png)

2. 使用以下node指令來執行App.js程式：
```c=
$ node App.js
```
![](https://i.imgur.com/Pzeux4P.png)

3. 開啟瀏覽器，並輸入Node.js程式的IP位址，在web尚可看到溫度資料的視覺化呈現（x軸代表資料count數，y軸代表溫度值）：
https://drive.google.com/file/d/1okG7BLShhsJz-yd-cKFHtGJxEU1x3C8T/view?usp=sharing
5. terminal可觀查到Node.js的程式輸出:
![](https://i.imgur.com/4kATtC7.png)

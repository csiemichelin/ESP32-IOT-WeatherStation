#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <esp_http_server.h>

#include "DHT22.h"

//html respond
static const char *TAG="APP";
static const char *WEATHER_TXT_REF =
"<html>"
"<head><title>%s</title>"
"<meta http-equiv=\"refresh\" content=\"2\" >"  //web自動化更新界面 
"</head>"
"<body>"
"<p>Temperature: %.1f </p>"
"<p>Humidity: %.1f %%</p>"
"</body>"
"</html>";

//URI 處理函數，在客戶端發起 GET /uri 請求時被調用
esp_err_t temperature_get_handler(httpd_req_t *req)
{
    setDHTgpio( 26 );
    ESP_LOGI(TAG, "Request headers lost");

    char tmp_buff[256];

    int ret = readDHT();
	errorHandler(ret);

    //WEATHER_TXT_REF可以讓web自動化更新界面    
    sprintf(tmp_buff, WEATHER_TXT_REF, (const char*) req->user_ctx, getTemperature(), getHumidity());	
    
    //monitor測試用
    printf( "Humidity: %.1f%% , ", getHumidity() );
    printf( "Tmp: %.1f^C\n", getTemperature() );

    //把溫度發送給請求者
    httpd_resp_send(req, tmp_buff, strlen(tmp_buff));

    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

httpd_uri_t weather_temp = {
    .uri       = "/temp",
    .method    = HTTP_GET,
    .handler   = temperature_get_handler,
    .user_ctx  = "ESP32 Weather System"
};
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

/* 停止 Web 服務器的函數 */
void stop_webserver(httpd_handle_t server)
{
    //停止 the httpd server
    httpd_stop(server);
}

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
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
        ESP_LOGI(TAG, "Got IP: '%s'",ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));

        if (*server == NULL) {
            //來啟動網路伺服器
            *server = start_webserver();
        }
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
        ESP_ERROR_CHECK(esp_wifi_connect());

        if (*server) {
            //來停止網路伺服器
            stop_webserver(*server);
            *server = NULL;
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

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
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main()
{
    static httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(nvs_flash_init());
    initialize_wifi(&server); 
}
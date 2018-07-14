/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "driver/uart.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
//#include "json/cJSON.h"
#include "esp_http_client.h"

/* Define the data type that will be queued. */
typedef struct xQueuePackageCtrl
{
    unsigned char ucSender;
    unsigned char ucQueueCode;
    unsigned char ucPayload_type;
    unsigned char ucPayload_size;

    union 
    {
      char  chardata[30];
      void* pointer;
    }xPayload;

} xQueuePackage;


/* ===================================================================
  Define variables
 ===================================================================*/
#define CORE_0 0 
#define CORE_1 1  
#define CUSTOM_ESP_WIFI_SSID      "BrunoAp"
#define CUSTOM_ESP_WIFI_PASS      "uimo0239"
#define IO_USERNAME  "Tsuchiya86"
#define IO_KEY       "e3470a4f71274bb09f141bf0f7bac5bb"
#define DEBUG 0
static const char *UARTTAG = "UART_events";
static const char *SYSTAG = "ESP32_system";
static const char *WIFITAG = "WIFI";
static const char *HTTPTAG = "HTTP_CLIENT";
#define PATTERN_CHR_NUM    (3)         /*!< Set the number of consecutive and identical characters received by receiver which defines a UART pattern*/
#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)



/* Define the queue parameters. */
#define QUEUE_LENGTH 5
#define QUEUE_ITEM_SIZE sizeof( xQueuePackage )

/* ===================================================================
  Define Event Group Flags
 ===================================================================*/

const int WIFI_CONNECTED_BIT = BIT0;

/* ===================================================================
  Global Variables
 ===================================================================*/
uart_config_t xUart2Config;
QueueHandle_t xUart2Queue;
QueueHandle_t xHttpQueue;
EventGroupHandle_t wifi_event_group;

/* ===================================================================
  Global Variables
 ===================================================================*/
extern const char howsmyssl_com_root_cert_pem_start[] asm("_binary_howsmyssl_com_root_cert_pem_start");
extern const char howsmyssl_com_root_cert_pem_end[]   asm("_binary_howsmyssl_com_root_cert_pem_end");

/* ===================================================================
  Function Prototyping
 ===================================================================*/
void prvSetupHardware( void );
static void vUartEventTask(void *pvParameters);
static esp_err_t event_handler(void *ctx, system_event_t *event);
static void vHttpTask(void *pvParameters);
esp_err_t _http_event_handler(esp_http_client_event_t *evt);
void wifi_init_sta(void);
void uart2_init(void);
int iAdafruitPost (char* data, char* feeds);

void app_main()
{
    //Set System log level
    esp_log_level_set(SYSTAG, ESP_LOG_INFO);
    //Set UART log level
    esp_log_level_set(UARTTAG, ESP_LOG_INFO);
    //Set Wifi log level
    esp_log_level_set(WIFITAG, ESP_LOG_INFO);

    esp_log_level_set(HTTPTAG, ESP_LOG_DEBUG);
    

    ESP_LOGI(SYSTAG, "Initialing hardware");
    prvSetupHardware();
    

    /*Task creation*/
    ESP_LOGI(SYSTAG, "Creating FreeRTOS tasks");
    xTaskCreatePinnedToCore( vHttpTask, "Http Task", configMINIMAL_STACK_SIZE+9000, NULL, 2, NULL, CORE_0 );  //https 
    xHttpQueue =  xQueueCreate(QUEUE_LENGTH , QUEUE_ITEM_SIZE);
    if (xHttpQueue == NULL)
    {
      ESP_LOGE(SYSTAG, "Unable to create the queue, try to increase the heap memory");
    }
    //xTaskCreatePinnedToCore( vUartTask, "Uart Task", configMINIMAL_STACK_SIZE+1000, NULL, 2, NULL, CORE_1 );  //Uart
    xTaskCreatePinnedToCore( vUartEventTask, "Event Task", configMINIMAL_STACK_SIZE+2048, NULL, 12, NULL, CORE_1); // Uart Event Handler

    /*Queue creation*/ 


}

static void vHttpTask(void *pvParameters)
{
    xQueuePackage queue;
    //char post_data[10];
    char * ret;

    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
    ESP_LOGI(HTTPTAG, "Connected to AP, begin http application");

    while(1)
    {
      ESP_LOGI(HTTPTAG, "Wait data to be received from UART");
      xQueueReceive(xHttpQueue, &queue, portMAX_DELAY);
        /* Post the received value on my IO adafruit account*/
      ret = strchr(queue.xPayload.chardata, ':');
      if(ret != NULL)
      {
         *ret = 0;
         ret++;
         ESP_LOGI(HTTPTAG, "feed: %s value: %s",queue.xPayload.chardata, ret);
         iAdafruitPost(ret, queue.xPayload.chardata);
      }
      
    }
}
//asdasdads
static void vUartEventTask(void *pvParameters)
{
    xQueuePackage queue;
    uart_event_t event;
    static bool pattern_detec_flag = false;
    size_t buffered_size;
    uint8_t* dtmp = (uint8_t*) malloc(RD_BUF_SIZE);
    for(;;) {
        //Waiting for UART event.
        if(xQueueReceive(xUart2Queue, (void * )&event, (portTickType)portMAX_DELAY)) {
            bzero(dtmp, RD_BUF_SIZE);
            ESP_LOGI(UARTTAG, "uart[%d] event:", UART_NUM_2);
            switch(event.type) {
                //Event of UART receving data
                /*We'd better handler data event fast, there would be much more data events than
                other types of events. If we take too much time on data event, the queue might
                be full.*/
                case UART_DATA:
                    if(pattern_detec_flag)
                    {
                      ESP_LOGI(UARTTAG, "[UART DATA]: %d", event.size);
                      uart_read_bytes(UART_NUM_2, dtmp, event.size, portMAX_DELAY);
                      ESP_LOGI(UARTTAG, "[DATA EVT]:");
                      //Wait 20 ticks for space. if available, send the received data to the http task
                      queue.ucQueueCode = 1;
                      memcpy(queue.xPayload.chardata, dtmp, 30);
                      xQueueSend(xHttpQueue, &queue, 20);
                      //uart_write_bytes(UART_NUM_2, (const char*) dtmp, event.size);
                      pattern_detec_flag = false;
                    }
                    break;
                //Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    ESP_LOGI(UARTTAG, "hw fifo overflow");
                    // If fifo overflow happened, you should consider adding flow control for your application.
                    // The ISR has already reset the rx FIFO,
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(UART_NUM_2);
                    xQueueReset(xUart2Queue);
                    break;
                //Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    ESP_LOGI(UARTTAG, "ring buffer full");
                    // If buffer full happened, you should consider encreasing your buffer size
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(UART_NUM_2);
                    xQueueReset(xUart2Queue);
                    break;
                //Event of UART RX break detected
                case UART_BREAK:
                    ESP_LOGI(UARTTAG, "uart rx break");
                    break;
                //Event of UART parity check error
                case UART_PARITY_ERR:
                    ESP_LOGI(UARTTAG, "uart parity error");
                    break;
                //Event of UART frame error
                case UART_FRAME_ERR:
                    ESP_LOGI(UARTTAG, "uart frame error");
                    break;
                //UART_PATTERN_DET
                case UART_PATTERN_DET:
                    uart_get_buffered_data_len(UART_NUM_2, &buffered_size);
                    int pos = uart_pattern_pop_pos(UART_NUM_2);
                    ESP_LOGI(UARTTAG, "[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);
                    if (pos == -1) {
                        // There used to be a UART_PATTERN_DET event, but the pattern position queue is full so that it can not
                        // record the position. We should set a larger queue size.
                        // As an example, we directly flush the rx buffer here.
                        uart_flush_input(UART_NUM_2);
                    } else {
                        uart_read_bytes(UART_NUM_2, dtmp, pos, 100 / portTICK_PERIOD_MS);
                        uint8_t pat[PATTERN_CHR_NUM + 1];
                        memset(pat, 0, sizeof(pat));
                        uart_read_bytes(UART_NUM_2, pat, PATTERN_CHR_NUM, 100 / portTICK_PERIOD_MS);
                        ESP_LOGI(UARTTAG, "read data: %s", dtmp);
                        ESP_LOGI(UARTTAG, "read pat : %s", pat);
                        pattern_detec_flag = true;
                    }
                    break;
                //Others
                default:
                    ESP_LOGI(UARTTAG, "uart event type: %d", event.type);
                    break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}


static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(WIFITAG, "got ip:%s", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_AP_STACONNECTED:
        ESP_LOGI(WIFITAG, "station:"MACSTR" join, AID=%d",
        MAC2STR(event->event_info.sta_connected.mac),
                   event->event_info.sta_connected.aid);
        break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(WIFITAG, "station:"MACSTR"leave, AID=%d",
        MAC2STR(event->event_info.sta_disconnected.mac),
                   event->event_info.sta_disconnected.aid);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}


esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(HTTPTAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(HTTPTAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(HTTPTAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(HTTPTAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(HTTPTAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // Write out data
                // printf("%.*s", evt->data_len, (char*)evt->data);
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(HTTPTAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(HTTPTAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}

void prvSetupHardware( void )
{
  //Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  wifi_init_sta();
  uart2_init();
}

void uart2_init(void)
{

  xUart2Config.baud_rate = 9600;
  xUart2Config.data_bits = UART_DATA_8_BITS;
  xUart2Config.parity =  UART_PARITY_DISABLE;
  xUart2Config.stop_bits =  UART_STOP_BITS_1;
  xUart2Config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE; 
  uart_param_config(UART_NUM_2, &xUart2Config);

  uart_set_pin(UART_NUM_2,17,16,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);

  uart_driver_install(UART_NUM_2,200,200,50,&xUart2Queue,ESP_INTR_FLAG_LEVEL2);

  /* Configure parameters of an UART driver,
   * communication pins and install the driver */
  //Set uart pattern detect function.
  uart_enable_pattern_det_intr(UART_NUM_2, '+', PATTERN_CHR_NUM, 10000, 10, 10);
  //Reset the pattern queue length to record at most 20 pattern positions.
  //uart_pattern_queue_reset(UART_NUM_2, 20);
}

void wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CUSTOM_ESP_WIFI_SSID,
            .password = CUSTOM_ESP_WIFI_PASS
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(WIFITAG, "wifi_init_sta finished.");
    ESP_LOGI(WIFITAG, "connect to ap SSID:%s password:%s",
             CUSTOM_ESP_WIFI_SSID, CUSTOM_ESP_WIFI_PASS);
}

int iAdafruitPost (char* data, char* feeds)
{
      char post_data[20];
      char api_url[70];
      
      sprintf (api_url,"https://io.adafruit.com/api/v2/Tsuchiya86/feeds/%s/data",feeds);

      esp_http_client_config_t config = {
        .url = api_url,
        .event_handler = _http_event_handler,
      };

      esp_http_client_handle_t client = esp_http_client_init(&config);

        /* Post the received value on my IO adafruit account*/
      sprintf(post_data,"{\"value\": %s}", data);
      //const char *post_data = "{\"value\": 50}";
      ESP_LOGI(HTTPTAG, "{\"value\": %s}", data);
      //esp_http_client_set_header(client, "Connection", "close");
      esp_http_client_set_method(client, HTTP_METHOD_POST);
      esp_http_client_set_post_field(client, post_data, strlen(post_data));
      esp_http_client_set_header(client, "Content-Type", "application/json" );
      esp_http_client_set_header(client, "X-AIO-Key", IO_KEY );
      esp_err_t err = esp_http_client_perform(client);
      ESP_ERROR_CHECK(err);
      if (err == ESP_OK) {
           ESP_LOGI(HTTPTAG, "HTTP POST Status = %d, content_length = %d field = %s",
                  esp_http_client_get_status_code(client),
                  esp_http_client_get_content_length(client), data);
            return 1;
      } else {
           ESP_LOGE(HTTPTAG, "HTTP POST request failed: %s", esp_err_to_name(err));
           vTaskDelete(NULL);
           return 0;
      }

      esp_http_client_cleanup(client);

}
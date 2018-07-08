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
#include "driver/uart.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

#define CORE_0 0 
#define CORE_1 1  
#define WIFISSID "Caverna do Dragao_Room"       
#define PASSWORD "BANANAchesterLANCER"   
#define DEBUG 0
static const char *TAG = "uart_events";
static const char *SYSTAG = "ESP32_system";
#define PATTERN_CHR_NUM    (3)         /*!< Set the number of consecutive and identical characters received by receiver which defines a UART pattern*/
#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)


portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;
uart_config_t xUart2Config;
QueueHandle_t xUart2Queue;

void vPrintString( const char *pcString );
void prvSetupHardware( void );
static void vUartEventTask(void *pvParameters);


void app_main()
{
    vPrintString("ESP32 system initialization..");
    prvSetupHardware();
    vPrintString("System initialized");

    /*Task creation*/
    //xTaskCreatePinnedToCore( vHttpTask, "Http Task", configMINIMAL_STACK_SIZE+8000, NULL, 2, NULL, CORE_0 );  //https 
    //xTaskCreatePinnedToCore( vUartTask, "Uart Task", configMINIMAL_STACK_SIZE+1000, NULL, 2, NULL, CORE_1 );  //Uart
    xTaskCreatePinnedToCore( vUartEventTask, "Event Task", configMINIMAL_STACK_SIZE+2048, NULL, 12, NULL, CORE_1); // Uart Event Handler

    /*Queue creation*/ 


}

static void vUartEventTask(void *pvParameters)
{
    uart_event_t event;
    static bool pattern_detec_flag = false;
    size_t buffered_size;
    uint8_t* dtmp = (uint8_t*) malloc(RD_BUF_SIZE);
    for(;;) {
        //Waiting for UART event.
        if(xQueueReceive(xUart2Queue, (void * )&event, (portTickType)portMAX_DELAY)) {
            bzero(dtmp, RD_BUF_SIZE);
            ESP_LOGI(TAG, "uart[%d] event:", UART_NUM_2);
            switch(event.type) {
                //Event of UART receving data
                /*We'd better handler data event fast, there would be much more data events than
                other types of events. If we take too much time on data event, the queue might
                be full.*/
                case UART_DATA:
                    if(pattern_detec_flag)
                    {
                      ESP_LOGI(TAG, "[UART DATA]: %d", event.size);
                      uart_read_bytes(UART_NUM_2, dtmp, event.size, portMAX_DELAY);
                      ESP_LOGI(TAG, "[DATA EVT]:");
                      uart_write_bytes(UART_NUM_2, (const char*) dtmp, event.size);
                      pattern_detec_flag = false;
                    }
                    break;
                //Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    ESP_LOGI(TAG, "hw fifo overflow");
                    // If fifo overflow happened, you should consider adding flow control for your application.
                    // The ISR has already reset the rx FIFO,
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(UART_NUM_2);
                    xQueueReset(xUart2Queue);
                    break;
                //Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    ESP_LOGI(TAG, "ring buffer full");
                    // If buffer full happened, you should consider encreasing your buffer size
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(UART_NUM_2);
                    xQueueReset(xUart2Queue);
                    break;
                //Event of UART RX break detected
                case UART_BREAK:
                    ESP_LOGI(TAG, "uart rx break");
                    break;
                //Event of UART parity check error
                case UART_PARITY_ERR:
                    ESP_LOGI(TAG, "uart parity error");
                    break;
                //Event of UART frame error
                case UART_FRAME_ERR:
                    ESP_LOGI(TAG, "uart frame error");
                    break;
                //UART_PATTERN_DET
                case UART_PATTERN_DET:
                    uart_get_buffered_data_len(UART_NUM_2, &buffered_size);
                    int pos = uart_pattern_pop_pos(UART_NUM_2);
                    ESP_LOGI(TAG, "[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);
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
                        ESP_LOGI(TAG, "read data: %s", dtmp);
                        ESP_LOGI(TAG, "read pat : %s", pat);
                        pattern_detec_flag = true;
                    }
                    break;
                //Others
                default:
                    ESP_LOGI(TAG, "uart event type: %d", event.type);
                    break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}

void prvSetupHardware( void )
{

  xUart2Config.baud_rate = 9600;
  xUart2Config.data_bits = UART_DATA_8_BITS;
  xUart2Config.parity =  UART_PARITY_DISABLE;
  xUart2Config.stop_bits =  UART_STOP_BITS_1;
  xUart2Config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE; 
  uart_param_config(UART_NUM_2, &xUart2Config);

  uart_set_pin(UART_NUM_2,17,16,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);

  uart_driver_install(UART_NUM_2,200,200,50,&xUart2Queue,ESP_INTR_FLAG_LEVEL2);

  esp_log_level_set(TAG, ESP_LOG_INFO);
  /* Configure parameters of an UART driver,
   * communication pins and install the driver */
  //Set UART log level
  esp_log_level_set(TAG, ESP_LOG_INFO);
  //Set uart pattern detect function.
  uart_enable_pattern_det_intr(UART_NUM_2, '+', PATTERN_CHR_NUM, 10000, 10, 10);
  //Reset the pattern queue length to record at most 20 pattern positions.
  //uart_pattern_queue_reset(UART_NUM_2, 20);
  
    for(uint8_t t = 4; t > 0; t--)
    {
      char rest = 200;
      uart_write_bytes(UART_NUM_2, &rest,1);
      uart_tx_chars(UART_NUM_2, "[SETUP] W\n", 10);
      uart_wait_tx_done(UART_NUM_2, portMAX_DELAY);
    }
}


void vPrintString( const char *pcString )
{
  taskENTER_CRITICAL( &myMutex );
  {
     printf( (char*)pcString );
  }
  taskEXIT_CRITICAL( &myMutex );
}

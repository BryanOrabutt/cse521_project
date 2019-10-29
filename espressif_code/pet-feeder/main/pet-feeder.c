
/*
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * Additions Copyright 2016 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
/**
 * @file pet-feeder.c
 * @brief Waits for JSON from AWS and determines whether to dispense food or now. Enters low power when not dispensing or transmitting.
 *
 * This example takes the parameters from the build configuration and establishes a connection to the AWS IoT MQTT Platform.
 * It subscribes to topic "pet-feeder/from_aws" and publishes to topic "pet-feeder/to_aws"
 *
 * Some setup is required. See example README for details.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "esp_intr_alloc.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_attr.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"


#include "../../json/cJSON/cJSON.h"

#define NOP() asm volatile ("nop")
#define WS_BASELINE 400000.0
#define WS_BASELINE_GRAMS 50.0
#define WS_EN 21
#define SRV_EN 17
#define WS_SCK 2
#define WS_DT 15
#define AMP_EN 22
#define SRV0 18
#define SRV1 19

#define PWM_CHANNEL LEDC_CHANNEL_7
#define PWM_TIMER LEDC_TIMER_3

static const char *TAG = "pet-feeder";
const static int serial_num = 123456;

/* The examples use simple WiFi configuration that you can set via
   'make menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_WIFI_SSID CONFIG_WIFI_SSID
#define EXAMPLE_WIFI_PASS CONFIG_WIFI_PASSWORD


#define SERVO_MIN_PULSEWIDTH 500 //Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH 2500 //Maximum pulse width in microsecond
#define SERVO_MAX_DEGREE 180 //Maximum angle in degree upto which servo can rotate
#define SERVO_PERIOD 20000.0
#define MAX_TIMER 32767

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

static QueueHandle_t rx_queue;
static QueueHandle_t tx_queue;

/* CA Root certificate, device ("Thing") certificate and device
 * ("Thing") key.

   Example can be configured one of two ways:

   "Embedded Certs" are loaded from files in "certs/" and embedded into the app binary.

   "Filesystem Certs" are loaded from the filesystem (SD card, etc.)

   See example README for more details.
*/
#if defined(CONFIG_EXAMPLE_EMBEDDED_CERTS)

extern const uint8_t aws_root_ca_pem_start[] asm("_binary_aws_root_ca_pem_start");
extern const uint8_t aws_root_ca_pem_end[] asm("_binary_aws_root_ca_pem_end");
extern const uint8_t certificate_pem_crt_start[] asm("_binary_certificate_pem_crt_start");
extern const uint8_t certificate_pem_crt_end[] asm("_binary_certificate_pem_crt_end");
extern const uint8_t private_pem_key_start[] asm("_binary_private_pem_key_start");
extern const uint8_t private_pem_key_end[] asm("_binary_private_pem_key_end");

#elif defined(CONFIG_EXAMPLE_FILESYSTEM_CERTS)

static const char * DEVICE_CERTIFICATE_PATH = CONFIG_EXAMPLE_CERTIFICATE_PATH;
static const char * DEVICE_PRIVATE_KEY_PATH = CONFIG_EXAMPLE_PRIVATE_KEY_PATH;
static const char * ROOT_CA_PATH = CONFIG_EXAMPLE_ROOT_CA_PATH;

#else
#error "Invalid method for loading certs"
#endif

const static float SERVO_MAX_DUTY = (float)SERVO_MAX_PULSEWIDTH/SERVO_PERIOD;
const static float SERVO_MIN_DUTY = (float)SERVO_MIN_PULSEWIDTH/SERVO_PERIOD;
static ledc_timer_config_t timer_conf;
static ledc_channel_config_t ledc_conf;

static char time_dispense = 0;
static char sample_weight = 0;
static int dispense_amount = 0;
static float weight = 0;

static TimerHandle_t heartbeat_timer;

/**
 * @brief Default MQTT HOST URL is pulled from the aws_iot_config.h
 */
char HostAddress[255] = AWS_IOT_MQTT_HOST;

/**
 * @brief Default MQTT port is pulled from the aws_iot_config.h
 */
uint32_t port = AWS_IOT_MQTT_PORT;


static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

void parse_json(void* params)
{
    cJSON* json_parser = NULL; //root of JSON key:value tree
    cJSON* object = NULL; //JSON object handle
    cJSON* item = NULL; //JSON item handle
    char msg[100]; //xQueue message handle
    char valid = 0; //Valid flag for logging
    
    
    /* Parse JSON messages from AWS.
     * Create cJSON tree.
     * Iterate over 'request' keys.
     * Iterate over 'update' keys.
     * repeat indefinitely
     */
    while(1)
    {
        //check if Queue has msg, wait one tick
        if(xQueueReceive(rx_queue, msg, (TickType_t) 1))
        {      
            ESP_LOGI(TAG, "JSON received: \n%.*s", strlen(msg), msg);
            json_parser = cJSON_Parse(msg); //create cJSON tree from message
            if(json_parser == NULL)
            {
                const char *error_ptr = cJSON_GetErrorPtr();
                if (error_ptr != NULL)
                {
                    ESP_LOGE(TAG, "Could not build JSON tree");
                    cJSON_Delete(json_parser);
                    continue;
                }
            }
            
            //Point object handle at request objects
            object = cJSON_GetObjectItemCaseSensitive(json_parser, "request");
            if (object)
            {
                ESP_LOGI(TAG, "Received request from AWS");
                cJSON_ArrayForEach(item, object)
                {
                    //set time_dispense flag for dispenser() vTask
                    if(strncmp(item->valuestring, "dispense", 10) == 0)
                    {
                        ESP_LOGI(TAG, "Dispense requested");
                        time_dispense = 1;
                        valid = 1;
                    }
                    //set sample_weight flag for weight_sensor() vTask
                    else if(strncmp(item->valuestring, "weight", 10) == 0)
                    {
                        ESP_LOGI(TAG, "Weight requested");
                        sample_weight = 1;
                        valid = 1;
                    }
                    //set invalid JSON
                    else
                    {
                        valid = 0;
                    }
                }
            }
            else
            {
                valid = 0;
            }
            
            //point object handle at update objects
            object = cJSON_GetObjectItemCaseSensitive(json_parser, "update");            
            if (cJSON_IsNumber(object) && (object->valueint >= 0))
            {
                ESP_LOGI(TAG, "Received weight update request from AWS");
           
                valid = 1;
                dispense_amount = (int)object->valueint;
            }
            else if(object)
            {
                valid = 0;
            }
            
            object = cJSON_GetObjectItemCaseSensitive(json_parser, "status");
            if(cJSON_IsNumber(object) && (object->valueint == 1))
            {
                ESP_LOGI(TAG, "Received heartbeat from AWS");
                valid = 1;
            }
            else if(object)
            {
                valid = 0;
            }
                       
            //free JSON tree
            cJSON_Delete(json_parser);
            
            //reset valid
            if(valid)
            {
                valid = 0;
                ESP_LOGI(TAG, "State: time_dispense = %d\t sample_weight = %d\t dispense_amount = %d", time_dispense, sample_weight, dispense_amount);
                xTimerReset(heartbeat_timer, 10);
            }
            //invalid JSON, log error
            else
            {
                ESP_LOGE(TAG, "Invalid request from AWS.\n");
            }
        }
       /* else
        {
            ESP_LOGI(TAG, "xQueue empty");
        }*/
    }
}


void heartbeat_timeout(TimerHandle_t xTimer)
{
    ESP_LOGW(TAG, "The dispenser has not heard from AWS in over 15 minutes. Dispensing food now...");
    time_dispense = 1;
    xTimerReset(heartbeat_timer, 10);
}

uint32_t calculate_duty(uint32_t angle)
{
    float duty = SERVO_MIN_DUTY + (((SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH) * (angle)) / (SERVO_MAX_DEGREE))/SERVO_PERIOD;
    
    duty = (float)MAX_TIMER*duty;
    uint32_t duty_int = (uint32_t)duty;
    
    return duty_int;
}

void dispense_task(void* params)
{
    volatile uint32_t timer_duty;
    char id = 'd';
    volatile int32_t count;
    while(1) 
    {
        if(time_dispense)
        {
            ESP_LOGI(TAG, "Dispensing %d grams of food", dispense_amount);
            gpio_set_level(SRV_EN, 1);
            
            for (count = 0; count < SERVO_MAX_DEGREE; count++) 
            {
                timer_duty = calculate_duty(count);
                //ledc_conf.duty = timer_duty;/*
                ESP_LOGW(TAG, "Timer val: %d", timer_duty);
                ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, PWM_CHANNEL, timer_duty, 0);
                //ledc_set_duty(LEDC_HIGH_SPEED_MODE, PWM_CHANNEL, timer_duty);
                //ledc_update_duty(LEDC_HIGH_SPEED_MODE, PWM_CHANNEL);
                //ledc_channel_config(&ledc_conf);
                vTaskDelay(2/portTICK_RATE_MS);    
            }
            
            vTaskDelay(10000/portTICK_RATE_MS);        
            
            for (count = SERVO_MAX_DEGREE; count >= 0; count--) 
            {
                timer_duty = calculate_duty(count);
                //ledc_conf.duty = timer_duty;/*
                ESP_LOGW(TAG, "Timer val: %d", timer_duty);
                ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, PWM_CHANNEL, timer_duty, 0);
                //ledc_set_duty(LEDC_HIGH_SPEED_MODE, PWM_CHANNEL, timer_duty);
                //ledc_update_duty(LEDC_HIGH_SPEED_MODE, PWM_CHANNEL);
                ////ledc_channel_config(&ledc_conf);
                vTaskDelay(2/portTICK_RATE_MS);     
            }
           
            time_dispense = 0;
            xQueueSend(tx_queue, (void*)&id, (TickType_t)0);
            gpio_set_level(SRV_EN, 0);
        }
        
        vTaskDelay(1000/portTICK_RATE_MS);
            
    }
}

unsigned long IRAM_ATTR micros()
{
    return (unsigned long) (esp_timer_get_time());
}

void IRAM_ATTR delayMicroseconds(uint32_t us)
{
    uint32_t m = micros();
    if(us){
        uint32_t e = (m + us);
        if(m > e){ //overflow
            while(micros() > e){
                NOP();
            }
        }
        while(micros() < e){
            NOP();
        }
    }
}

void weight_task(void* params)
{
    char id = 'w';
    char cycles;
    char iter;
    int reading;
    float reading_f;
    float iter_f;
    float grams;
    
    while(1)
    {
        if(sample_weight)
        {
            reading = 0;
            weight = 0;
            gpio_set_level(WS_EN, 1);
            delayMicroseconds(1);
            //vTaskDelay(1);
            
       // for(iter = 0; iter < 8; iter++)
        //{
            
            //while(gpio_get_level(WS_DT))
           // {
           //     ESP_LOGW(TAG, "GPIO Level: %d", gpio_get_level(WS_DT));
           //     delayMicroseconds(1);
            //}
            
            for(cycles = 0; cycles < 24; cycles++)
            {
                gpio_set_level(WS_SCK, 1);
                delayMicroseconds(1);
                gpio_set_level(WS_SCK, 0);
                reading |= (gpio_get_level(WS_DT)<<(23-cycles));
                delayMicroseconds(1);        
            }
        
            //vTaskDelay(10);
            
            gpio_set_level(WS_SCK, 1);
            delayMicroseconds(1);
            gpio_set_level(WS_SCK, 0);
            
            //average samples together
            reading_f = (float)reading;
            //iter_f = (float)iter;
            weight = reading_f;
            //weight = ((iter_f-1.0)/iter_f)*weight+(1.0/iter_f)*reading_f;
            //}
            
            grams = (weight - WS_BASELINE)/(WS_BASELINE_GRAMS)*100.0;
            ESP_LOGW(TAG, "Reading: %d", reading);
            ESP_LOGI(TAG, "Reading weight sensor: %f", weight);
            ESP_LOGI(TAG, "Weighr value in grams: %f", grams);
            
            sample_weight = 0;    
            gpio_set_level(WS_EN, 0);
            xQueueSend(tx_queue, (void*)&id, (TickType_t)0);        
        }
        
        vTaskDelay(1000/portTICK_RATE_MS);
    }
}

void iot_subscribe_callback_handler(AWS_IoT_Client *pClient, char *topicName, uint16_t topicNameLen,
                                    IoT_Publish_Message_Params *params, void *pData) {
    ESP_LOGI(TAG, "Subscribe callback");
    ESP_LOGI(TAG, "%.*s\t%.*s", topicNameLen, topicName, (int) params->payloadLen, (char *)params->payload);
    
    char msg[200];
    strncpy(msg, params->payload, params->payloadLen);
    strcat(msg, "\0");
    xQueueSend(rx_queue, (void*)msg, (TickType_t) 0);
	//parse_json((char*)params->payload;
}

void disconnectCallbackHandler(AWS_IoT_Client *pClient, void *data) {
    ESP_LOGW(TAG, "MQTT Disconnect");
    IoT_Error_t rc = FAILURE;

    if(NULL == pClient) {
        return;
    }

    if(aws_iot_is_autoreconnect_enabled(pClient)) {
        ESP_LOGI(TAG, "Auto Reconnect is enabled, Reconnecting attempt will start now");
    } else {
        ESP_LOGW(TAG, "Auto Reconnect not enabled. Starting manual reconnect...");
        rc = aws_iot_mqtt_attempt_reconnect(pClient);
        if(NETWORK_RECONNECTED == rc) {
            ESP_LOGW(TAG, "Manual Reconnect Successful");
        } else {
            ESP_LOGW(TAG, "Manual Reconnect Failed - %d", rc);
        }
    }
}

void aws_iot_task(void *param) {
    char cPayload[100];

    //int32_t i = 0;
    cJSON* msg_for_aws = NULL;
    cJSON* data = NULL;
    char id;
    char* str;
        

    IoT_Error_t rc = FAILURE;

    AWS_IoT_Client client;
    IoT_Client_Init_Params mqttInitParams = iotClientInitParamsDefault;
    IoT_Client_Connect_Params connectParams = iotClientConnectParamsDefault;

    IoT_Publish_Message_Params paramsQOS0;
   // IoT_Publish_Message_Params paramsQOS1;

    ESP_LOGI(TAG, "AWS IoT SDK Version %d.%d.%d-%s", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

    mqttInitParams.enableAutoReconnect = false; // We enable this later below
    mqttInitParams.pHostURL = HostAddress;
    mqttInitParams.port = port;

#if defined(CONFIG_EXAMPLE_EMBEDDED_CERTS)
    mqttInitParams.pRootCALocation = (const char *)aws_root_ca_pem_start;
    mqttInitParams.pDeviceCertLocation = (const char *)certificate_pem_crt_start;
    mqttInitParams.pDevicePrivateKeyLocation = (const char *)private_pem_key_start;

#elif defined(CONFIG_EXAMPLE_FILESYSTEM_CERTS)
    mqttInitParams.pRootCALocation = ROOT_CA_PATH;
    mqttInitParams.pDeviceCertLocation = DEVICE_CERTIFICATE_PATH;
    mqttInitParams.pDevicePrivateKeyLocation = DEVICE_PRIVATE_KEY_PATH;
#endif

    mqttInitParams.mqttCommandTimeout_ms = 20000;
    mqttInitParams.tlsHandshakeTimeout_ms = 5000;
    mqttInitParams.isSSLHostnameVerify = true;
    mqttInitParams.disconnectHandler = disconnectCallbackHandler;
    mqttInitParams.disconnectHandlerData = NULL;

#ifdef CONFIG_EXAMPLE_SDCARD_CERTS
    ESP_LOGI(TAG, "Mounting SD card...");
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 3,
    };
    sdmmc_card_t* card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card VFAT filesystem. Error: %s", esp_err_to_name(ret));
        abort();
    }
#endif

    rc = aws_iot_mqtt_init(&client, &mqttInitParams);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "aws_iot_mqtt_init returned error : %d ", rc);
        abort();
    }

    /* Wait for WiFI to show as connected */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);

    connectParams.keepAliveIntervalInSec = 10;
    connectParams.isCleanSession = true;
    connectParams.MQTTVersion = MQTT_3_1_1;
    /* Client ID is set in the menuconfig of the example */
    connectParams.pClientID = CONFIG_AWS_EXAMPLE_CLIENT_ID;
    connectParams.clientIDLen = (uint16_t) strlen(CONFIG_AWS_EXAMPLE_CLIENT_ID);
    connectParams.isWillMsgPresent = false;

    ESP_LOGI(TAG, "Connecting to AWS...");
    do {
        rc = aws_iot_mqtt_connect(&client, &connectParams);
        if(SUCCESS != rc) {
            ESP_LOGE(TAG, "Error(%d) connecting to %s:%d", rc, mqttInitParams.pHostURL, mqttInitParams.port);
            vTaskDelay(1000 / portTICK_RATE_MS);
        }
    } while(SUCCESS != rc);

    /*
     * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
     *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
     *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
     */
    rc = aws_iot_mqtt_autoreconnect_set_status(&client, true);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "Unable to set Auto Reconnect to true - %d", rc);
        abort();
    }

    const char *TOPIC_PUB = "pet-feeder/to_aws";
    const char* TOPIC_SUB = "pet-feeder/from_aws";
    const int TOPIC_PUB_LEN = strlen(TOPIC_PUB);
    const int TOPIC_SUB_LEN = strlen(TOPIC_SUB);

    ESP_LOGI(TAG, "Subscribing...");
    rc = aws_iot_mqtt_subscribe(&client, TOPIC_SUB, TOPIC_SUB_LEN, QOS0, iot_subscribe_callback_handler, NULL);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "Error subscribing : %d ", rc);
        abort();
    }

    paramsQOS0.qos = QOS0;
    paramsQOS0.payload = (void *) cPayload;
    paramsQOS0.isRetained = 0;
    
    while((NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc || SUCCESS == rc)) {

        //Max time the yield function will wait for read messages
        rc = aws_iot_mqtt_yield(&client, 100);
        if(NETWORK_ATTEMPTING_RECONNECT == rc) {
            // If the client is attempting to reconnect we will skip the rest of the loop.
            continue;
        }

        ESP_LOGI(TAG, "Stack remaining for task '%s' is %d bytes", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL));
        vTaskDelay(5000 / portTICK_RATE_MS);
        
        msg_for_aws = cJSON_CreateObject();
        data = cJSON_CreateNumber(1);
        cJSON_AddItemToObject(msg_for_aws, "heartbeat", data);
        
        while(xQueueReceive(tx_queue, &id, (TickType_t) 1))
        {
            ESP_LOGI(TAG, "Received item in txQueue: %c", id);
            if(id == 'w')
            {
                data = cJSON_CreateNumber(weight);
                cJSON_AddItemToObject(msg_for_aws, "weight", data); 
            }
            else if(id == 'd')
            {
                data = cJSON_CreateString("ready");
                cJSON_AddItemToObject(msg_for_aws, "status", data);
            }
        }
        
        str = cJSON_Print(msg_for_aws);
        snprintf(cPayload, 100, "%s", str);
        paramsQOS0.payloadLen = strlen(cPayload);
        rc = aws_iot_mqtt_publish(&client, TOPIC_PUB, TOPIC_PUB_LEN, &paramsQOS0);
        
        cJSON_Delete(msg_for_aws);
        
        if (rc == MQTT_REQUEST_TIMEOUT_ERROR) {
            ESP_LOGW(TAG, "QOS1 publish ack not received.");
            rc = SUCCESS;
        }
    }

    ESP_LOGE(TAG, "An error occurred in the main loop.");
    abort();
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            .password = EXAMPLE_WIFI_PASS,
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}


void app_main()
{
    // Initialize NVS.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    
    rx_queue = xQueueCreate(5, 200*sizeof(char));
    tx_queue = xQueueCreate(10, sizeof(char));
    
    if(rx_queue == 0)
    {
        ESP_LOGE(TAG, "Failed to create message queue");
    }

    //configure high speed PWM timer
    timer_conf.duty_resolution = LEDC_TIMER_15_BIT;
    timer_conf.freq_hz = 50;
    timer_conf.speed_mode = LEDC_HIGH_SPEED_MODE;
    timer_conf.timer_num = PWM_TIMER;
    ledc_timer_config(&timer_conf);
    
    //configure high speed PWM channel
    ledc_conf.channel = PWM_CHANNEL;
    ledc_conf.duty = 0;
    ledc_conf.gpio_num = SRV0;
    ledc_conf.intr_type = LEDC_INTR_DISABLE;
    ledc_conf.speed_mode = LEDC_HIGH_SPEED_MODE;
    ledc_conf.timer_sel = PWM_TIMER;
    ledc_channel_config(&ledc_conf);
    
    ledc_fade_func_install(ESP_INTR_FLAG_LEVEL1);
    
    gpio_config_t io_conf;
    
    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,
    io_conf.pin_bit_mask = (1ULL<<WS_EN | 1ULL<<SRV_EN | 1ULL<<WS_SCK);
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
    
    
    //bit mask of the pins
    io_conf.pin_bit_mask = (1ULL<<WS_DT);
    //set as input mode    
    io_conf.mode = GPIO_MODE_INPUT;
    //disable pull-up mode
    io_conf.pull_up_en = 1;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    gpio_config(&io_conf);
    
    gpio_set_level(WS_SCK, 0);
    gpio_set_level(WS_EN, 0);
    gpio_set_level(SRV_EN, 0);
    
	heartbeat_timer = xTimerCreate("heartbeat_timer", pdMS_TO_TICKS(900000), pdTRUE, (void*) 0, heartbeat_timeout);

    initialise_wifi();
    xTaskCreatePinnedToCore(&aws_iot_task, "aws_iot_task", 9516, NULL, 5, NULL, 1);
    
    ESP_LOGI(TAG, "Creating JSON parsing task");
    xTaskCreate(&parse_json, "parse_json_task", 5000, NULL, 4, NULL);
    
    xTaskCreate(&dispense_task, "dispenser_task", 5000, NULL, 6, NULL);
    xTaskCreate(&weight_task, "weight_task", 5000, NULL, 4, NULL);
    
}

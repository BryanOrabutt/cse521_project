/* servo motor control example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_attr.h"
#include "driver/ledc.h"

//You can get these value from the datasheet of servo you use, in general pulse width varies between 1000 to 2000 mocrosecond
#define SERVO_MIN_PULSEWIDTH 500 //Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH 2500 //Maximum pulse width in microsecond
#define SERVO_MAX_DEGREE 180 //Maximum angle in degree upto which servo can rotate
#define SERVO_PERIOD 20000.0
#define MAX_TIMER 32767

const static float SERVO_MAX_DUTY = (float)SERVO_MAX_PULSEWIDTH/SERVO_PERIOD;
const static float SERVO_MIN_DUTY = (float)SERVO_MIN_PULSEWIDTH/SERVO_PERIOD;
static ledc_timer_config_t timer_conf;
static ledc_channel_config_t ledc_conf;

uint32_t calculate_duty(uint32_t angle)
{
    float duty = SERVO_MIN_DUTY + (((SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH) * (angle)) / (SERVO_MAX_DEGREE))/SERVO_PERIOD;
    
    duty = (float)MAX_TIMER*duty;
    uint32_t duty_int = (uint32_t)duty;
    
    return duty_int;
}

void servo_task(void* params)
{
    int32_t count, timer_duty;
    while(1)
    {
        for (count = 0; count < SERVO_MAX_DEGREE; count++) 
        {
            printf("Angle of rotation: %d\n", count);
            timer_duty = calculate_duty(count);
            printf("duty: %d \n", timer_duty);
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, timer_duty);
            ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
            vTaskDelay(20/portTICK_RATE_MS);     //Add delay, since it takes time for servo to rotate, generally 100ms/60degree rotation at 5V
        }
        for (count = SERVO_MAX_DEGREE; count >= 0; count--) 
        {
            printf("Angle of rotation: %d\n", count);
            timer_duty = calculate_duty(count);
            printf("duty: %d\n", timer_duty);
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, timer_duty);
            ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
            vTaskDelay(20/portTICK_RATE_MS);     //Add delay, since it takes time for servo to rotate, generally 100ms/60degree rotation at 5V
        }
    }
}

void app_main()
{
    //configure high speed PWM timer
    timer_conf.duty_resolution = LEDC_TIMER_15_BIT;
    timer_conf.freq_hz = 50;
    timer_conf.speed_mode = LEDC_HIGH_SPEED_MODE;
    timer_conf.timer_num = LEDC_TIMER_0;
    ledc_timer_config(&timer_conf);
    
    //configure high speed PWM channel
    ledc_conf.channel = LEDC_CHANNEL_0;
    ledc_conf.duty = 0;
    ledc_conf.gpio_num = 18;
    ledc_conf.intr_type = LEDC_INTR_DISABLE;
    ledc_conf.speed_mode = LEDC_HIGH_SPEED_MODE;
    ledc_conf.timer_sel = LEDC_TIMER_0;
    ledc_channel_config(&ledc_conf);
    
    xTaskCreate(&servo_task, "servo-task", 5000, NULL, 4, NULL);

}

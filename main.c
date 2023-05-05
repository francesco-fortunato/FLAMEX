/*
 * Copyright (C) 2023 Francesco Fortunato
 * This file is subject to the terms and conditions of the MIT License.
 * See the file LICENSE in the top level directory for more details.
 */

/**
 * @{
 *
 * @file
 * @brief       FLAMEX
 *
 * @author      Francesco Fortunato <francesco.fortunato1999@gmail.com>
 *
 * @}
 */
#include "stdio.h"
#include <time.h>
#include "periph/adc.h"
#include "periph/gpio.h"
#include "periph/i2c.h"
#include "analog_util.h"
#include "xtimer.h"
#include "led.h"
#include "u8g2.h"
#include "u8x8_riotos.h"


#include "paho_mqtt.h"
#include "MQTTClient.h"

#define SSD1306_I2C_ADDR    (0x3c)
#define ADC_IN_USE    ADC_LINE(0)
#define ADC_RES       ADC_RES_12BIT
#define BUZZER_PIN GPIO23
#define DELAY         (15 * US_PER_SEC)
#define FLAME_THRESHOLD 70 // Set threshold for flame value here
#define OLED_WIDTH    128
#define OLED_HEIGHT   64


// MQTT client settings
#define BUF_SIZE                        1024

#define MQTT_BROKER_ADDR "192.168.252.13"
#define MQTT_TOPIC "flamex"
#define MQTT_VERSION_v311               4       /* MQTT v3.1.1 version is 4 */
#define COMMAND_TIMEOUT_MS              4000

#define DEFAULT_MQTT_CLIENT_ID          ""

#define DEFAULT_MQTT_USER               ""

#define DEFAULT_MQTT_PWD                ""

#define DEFAULT_MQTT_PORT               1883    // Default MQTT port

#define DEFAULT_KEEPALIVE_SEC           10      // Keepalive timeout in seconds

#define IS_RETAINED_MSG                 0


static MQTTClient client;
static Network network;

static unsigned char buf[BUF_SIZE];
static unsigned char readbuf[BUF_SIZE];

u8g2_t u8g2;
u8x8_riotos_t user_data = {
    .device_index = I2C_DEV(0),
    .pin_cs = GPIO_UNDEF,
    .pin_dc = GPIO_UNDEF,
    .pin_reset = GPIO16,
};

static int mqtt_connect(void)
{
    int ret = 0;
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = MQTT_VERSION_v311;
    data.clientID.cstring = DEFAULT_MQTT_CLIENT_ID;
    data.username.cstring = DEFAULT_MQTT_USER;
    data.password.cstring = DEFAULT_MQTT_PWD;
    data.keepAliveInterval = 60;
    data.cleansession = 1;
    
    printf("flame_sensor: Connecting to MQTT Broker from %s %d\n",
            MQTT_BROKER_ADDR, DEFAULT_MQTT_PORT);
    printf("flame_sensor: Trying to connect to %s, port: %d\n",
            MQTT_BROKER_ADDR, DEFAULT_MQTT_PORT);
    ret = NetworkConnect(&network, MQTT_BROKER_ADDR, DEFAULT_MQTT_PORT);
    if (ret < 0) {
        printf("mqtt_example: Unable to connect\n");
        return ret;
    }
    
    printf("user:%s clientId:%s password:%s\n", data.username.cstring,
             data.clientID.cstring, data.password.cstring);
    ret = MQTTConnect(&client, &data);
    if (ret < 0) {
        printf("mqtt_example: Unable to connect client %d\n", ret);
        int res = MQTTDisconnect(&client);
        if (res < 0) {
            printf("mqtt_example: Unable to disconnect\n");
        }
        else {
            printf("mqtt_example: Disconnect successful\n");
        }

        NetworkDisconnect(&network);
        return res;
    }
    else {
        printf("mqtt_example: Connection successfully\n");
    }

    printf("MQTT client connected to broker\n");
    return 0;
}

// value from 0 to 7, higher values more brighter
void setSSD1306VcomDeselect(uint8_t v)
{	
  u8x8_cad_StartTransfer(&u8g2.u8x8);
  u8x8_cad_SendCmd(&u8g2.u8x8, 0x0db);
  u8x8_cad_SendArg(&u8g2.u8x8, v << 4);
  u8x8_cad_EndTransfer(&u8g2.u8x8);
}

// p1: 1..15, higher values, more darker, however almost no difference with 3 or more
// p2: 1..15, higher values, more brighter
void setSSD1306PreChargePeriod(uint8_t p1, uint8_t p2)
{	
  u8x8_cad_StartTransfer(&u8g2.u8x8);
  u8x8_cad_SendCmd(&u8g2.u8x8, 0x0d9);
  u8x8_cad_SendArg(&u8g2.u8x8, (p2 << 4) | p1 );
  u8x8_cad_EndTransfer(&u8g2.u8x8);
}

int main(void)
{
    printf("You are running RIOT on a(n) %s board.\n", RIOT_BOARD);
    printf("This board features a(n) %s MCU.\n", RIOT_MCU);
    
    if(adc_init(ADC_IN_USE) < 0){
        printf("Failed to initialize ADC\n");
        return 1;
    } else {
        printf("ADC OK\n");
    }

    // Initialize the buzzer pin as output
    if(gpio_init(BUZZER_PIN, GPIO_OUT) < 0) {
        printf("Failed to initialize buzzer\n");
        return 1;
    } else {
        printf("Buzzer OK\n");
    }

    // Initialize the display
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0,
                                       u8x8_byte_hw_i2c_riotos,
                                       u8x8_gpio_and_delay_riotos);
    u8g2_SetUserPtr(&u8g2, &user_data);
    u8g2_SetI2CAddress(&u8g2, SSD1306_I2C_ADDR);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);  
    u8g2_ClearDisplay(&u8g2);
    // Set VCOM deselect and pre-charge period
    setSSD1306VcomDeselect(7);
    setSSD1306PreChargePeriod(1, 15);

    //u8g2_SetContrast(&u8g2, 0);

    //Timer
    xtimer_ticks32_t last = xtimer_now();
    int sample = 0;
    int flame = 0;
    float voltage = 0;

    bool fire = false;
    bool sleep = false;

    

    //ztimer_sleep(ZTIMER_MSEC, 10 * MS_PER_SEC);
    int progress=0;
    while(progress<30){
        
        u8g2_FirstPage(&u8g2);
        do {
            u8g2_SetDrawColor(&u8g2, 1);
        u8g2_SetFont(&u8g2, u8g2_font_helvB14_tf);
        u8g2_DrawStr(&u8g2, 25, 35, "FLAMEX");
        } while (u8g2_NextPage(&u8g2));
        progress++;
        
        ztimer_sleep(ZTIMER_MSEC,0.1*MS_PER_SEC);
    }
    progress = 0;
    float percentage =0.0;
    char percent_str[6];
    sprintf(percent_str, "%.1f %%", percentage);
    while(progress<87){

        u8g2_FirstPage(&u8g2);
        do {
            u8g2_SetDrawColor(&u8g2, 1);

            u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
            u8g2_DrawStr(&u8g2, 25, 25, "Loading...");

            // Draw outline of the progress bar
            u8g2_DrawFrame(&u8g2, 20, 35, 90, 10);

            // Draw the filled part of the progress bar
            u8g2_DrawBox(&u8g2, 22, 37, progress, 6);

            // Calculate the percentage and display it
            percentage = (progress+1) / 87.0 * 100.0;
            sprintf(percent_str, "%.1f %%", percentage);
            u8g2_DrawStr(&u8g2, 47, 55, percent_str);

        } while (u8g2_NextPage(&u8g2));
    
        progress += 1;
        ztimer_sleep(ZTIMER_MSEC,0.06*MS_PER_SEC);
        
    }
    
    // Initialize network
    NetworkInit(&network);

    ztimer_sleep(ZTIMER_MSEC,2*MS_PER_SEC);

    MQTTClientInit(&client, &network, COMMAND_TIMEOUT_MS, buf, BUF_SIZE,
                   readbuf,
                   BUF_SIZE);

    MQTTStartTask(&client);


    // Connect to MQTT broker
    mqtt_connect();

    while(1){
        
        
        u8g2_ClearBuffer(&u8g2); 
        sample = adc_sample(ADC_IN_USE, ADC_RES);
        voltage = adc_util_map(sample, ADC_RES, 3300, 0);
        flame = adc_util_map(sample, ADC_RES, 100, 1);

        fire = (flame > FLAME_THRESHOLD) ? true : false;
        

        if (fire) { // Flame detected
        
            gpio_set(BUZZER_PIN);
            xtimer_usleep(1000000);  // Wait for 1s
            gpio_clear(BUZZER_PIN);
            xtimer_usleep(400000);  // Wait for 400ms
        } else {
            gpio_clear(BUZZER_PIN);
        }

        printf("Voltage: %.2f mV\tFlame: %d\n", voltage, flame);

        char voltage_str[100];
        if (voltage > 10){
            sleep=false;
            u8g2_SetPowerSave(&u8g2, 0);
            sprintf(voltage_str, "%.2f mV FIREEEEE", voltage);
            u8g2_FirstPage(&u8g2);
            do {
                u8g2_SetFont(&u8g2, u8g2_font_helvR08_tf);

                u8g2_DrawStr(&u8g2, 0, 20, voltage_str);
            } while (u8g2_NextPage(&u8g2));
            ztimer_sleep(ZTIMER_USEC, US_PER_SEC);
        }
        else{
            if (!sleep){
            sleep=true;
            sprintf(voltage_str, "%.2f mV NO FIRE :)", voltage);
            u8g2_FirstPage(&u8g2);
            do {
                u8g2_SetFont(&u8g2, u8g2_font_helvR08_tf);

                u8g2_DrawStr(&u8g2, 0, 20, voltage_str);
                u8g2_SetFont(&u8g2, u8g2_font_helvR08_tf);
                u8g2_DrawStr(&u8g2, 0, 40, "Sleep mode activating. . .");
        
            } while (u8g2_NextPage(&u8g2));
            }
        }
        //u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);
        //u8g2_DrawStr(&u8g2, 0, 20, voltage_str);
        //u8g2_SendBuffer(&u8g2);

        char json[128];
        sprintf(json, "{\"id\": \"%d\", \"voltage\": \"%.2f\", \"flame\": \"%d\"}",
                        0, voltage, flame);
        char* msg = json;
        //MQTT
        // Publish flame value to MQTT broker
        MQTTMessage message;
        message.qos = QOS2;
        message.retained = IS_RETAINED_MSG;
        message.payload = msg;
        message.payloadlen = strlen(message.payload);

        char* topic = "flamex";

        int rc;
        if ((rc = MQTTPublish(&client, topic, &message)) < 0) {
            printf("mqtt_example: Unable to publish (%d)\n", rc);
        }
        else {
            printf("mqtt_example: Message (%s) has been published to topic %s "
                "with QOS %d\n",
                (char *)message.payload, topic, (int)message.qos);
        }

        xtimer_periodic_wakeup(&last, DELAY);
    
    }

    return 0;
}

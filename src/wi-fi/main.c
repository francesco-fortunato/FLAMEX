/*
 * Copyright (C) 2023 Francesco Fortunato
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
#include "time.h"
#include "thread.h"

#include "periph/adc.h"
#include "periph/gpio.h"
#include "periph/i2c.h"
#include "analog_util.h"

#include "xtimer.h"

#include "paho_mqtt.h"
#include "MQTTClient.h"

#define IR_FLAME_PIN        ADC_LINE(0)
#define GAS_PIN             ADC_LINE(2)
#define ADC_RES             ADC_RES_12BIT
#define BUZZER_PIN          GPIO23
#define RELAY_PIN           GPIO12
#define DELAY               (10 * US_PER_SEC)
#define FLAME_THRESHOLD     70 // Set threshold for flame value here

// MQTT client settings
#define BUF_SIZE                        1024

#define MQTT_BROKER_ADDR "192.168.13.13"
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

char stack1[THREAD_STACKSIZE_MAIN];
char stack2[THREAD_STACKSIZE_MAIN];
kernel_pid_t thread_pid_buzzer;
kernel_pid_t thread_pid_pump;

bool fire;
bool gas;
bool buzz;
bool pump;

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
    
    printf("MQTT: Connecting to MQTT Broker from %s %d\n",
            MQTT_BROKER_ADDR, DEFAULT_MQTT_PORT);
    printf("MQTT: Trying to connect to %s, port: %d\n",
            MQTT_BROKER_ADDR, DEFAULT_MQTT_PORT);
    ret = NetworkConnect(&network, MQTT_BROKER_ADDR, DEFAULT_MQTT_PORT);
    if (ret < 0) {
        printf("MQTT: Unable to connect\n");
        return ret;
    }
    
    printf("user:%s clientId:%s password:%s\n", data.username.cstring,
             data.clientID.cstring, data.password.cstring);
    ret = MQTTConnect(&client, &data);
    if (ret < 0) {
        printf("MQTT: Unable to connect client %d\n", ret);
        int res = MQTTDisconnect(&client);
        if (res < 0) {
            printf("MQTT: Unable to disconnect\n");
        }
        else {
            printf("MQTT: Disconnect successful\n");
        }

        NetworkDisconnect(&network);
        return res;
    }
    else {
        printf("MQTT: Connection successfully\n");
    }

    printf("MQTT client connected to broker\n");
    return 0;
}

void *buzzer_thread(void *arg)
{
    (void) arg;
    while(1) {
        if(buzz) {
            printf("BUZZ ON\n");
            gpio_set(BUZZER_PIN);
            xtimer_usleep(1000000);  // Wait for 1s
            gpio_clear(BUZZER_PIN);
            xtimer_usleep(400000);  // Wait for 400ms
        } else {
            thread_sleep();
        }
    }
    return NULL;
}

void *pump_thread(void *arg)
{
    (void) arg;
    while(1) {
        if (pump) {
            printf("POMPA PARTITA\n");
            gpio_clear(RELAY_PIN);
            xtimer_sleep(5);
        } else {
            printf("POMPA FERMATA\n");
            gpio_set(RELAY_PIN);
            thread_sleep();
        }
    }
    return NULL;
}

int main(void)
{
    printf("You are running RIOT on a(n) %s board.\n", RIOT_BOARD);
    printf("This board features a(n) %s MCU.\n", RIOT_MCU);
    
    if(adc_init(IR_FLAME_PIN) < 0){
        printf("Failed to initialize ADC for flame sensor\n");
        return 1;
    } else {
        printf("Flame sensor ADC OK\n");
    }

    if(adc_init(GAS_PIN) < 0){
        printf("Failed to initialize ADC for gas sensor\n");
        return 1;
    } else {
        printf("Gas sensor ADC OK\n");
    }

    // Initialize the buzzer pin as output
    if(gpio_init(BUZZER_PIN, GPIO_OUT) < 0) {
        printf("Failed to initialize buzzer pin\n");
        return 1;
    } else {
        printf("Buzzer pin OK\n");
    }

    // Initialize the relay pin as output
    if(gpio_init(RELAY_PIN, GPIO_OUT) < 0) {
        printf("Failed to initialize relay pin\n");
        return 1;
    } else {
        gpio_set(RELAY_PIN);
        printf("Relay pin OK\n");
    }

    thread_pid_buzzer = thread_create(stack1, sizeof(stack1),
                    THREAD_PRIORITY_MAIN + 2,
                    THREAD_CREATE_SLEEPING,
                    buzzer_thread,
                    NULL, "buzzer_thread");
    thread_pid_pump = thread_create(stack2, sizeof(stack2),
                    THREAD_PRIORITY_MAIN + 1,
                    THREAD_CREATE_SLEEPING,
                    pump_thread,
                    NULL, "pump_thread");

    //Timer
    xtimer_ticks32_t last = xtimer_now();
    int sample_fire = 0;
    int sample_gas = 0;
    int flame = 0;
    int gas_value = 0;
    float voltage_flame = 0;
    float voltage_gas = 0;

    fire = false;
    gas = false;
    pump = false;
    buzz = false;

    ztimer_sleep(ZTIMER_MSEC, 7 * MS_PER_SEC);

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
        
        sample_fire = adc_sample(IR_FLAME_PIN, ADC_RES);
        voltage_flame = adc_util_map(sample_fire, ADC_RES, 4095, 0);
        flame = adc_util_map(sample_fire, ADC_RES, 100, 1);
        
        sample_gas = adc_sample(GAS_PIN, ADC_RES);
        voltage_gas = adc_util_map(sample_gas, ADC_RES, 4095, 0);
        gas_value = adc_util_map(sample_gas, ADC_RES, 1, 100);

        fire = (flame > FLAME_THRESHOLD) ? true : false;
        gas = (gas_value > 40) ? true : false;
        
        if(gas || fire) {
            buzz = true;
            thread_wakeup(thread_pid_buzzer);
            if(gas){
                printf("GAS DETECTED\n");
            }
            else{
                printf("NO GAS DETECTED\n");
            }
            if(fire){
                printf("FIRE DETECTED\n");
                pump = true;
                thread_wakeup(thread_pid_pump);
            }
            else{
                printf("NO FIRE DETECTED\n");
                pump = false;
            }
        }
        else{
            buzz = false;
            pump = false;
        }


        printf("Voltage_fire: %.2f mV\tFlame: %d\n", voltage_flame, flame);
        printf("Voltage_gas: %.2f mV\tGAS: %d\n", voltage_gas, gas_value);

        char json[200];
        if (!pump){
            sprintf(json, "{\"id\": \"%d\", \"voltage\": \"%.2f\", \"flame\": \"%d\", \"gas\": \"%.2d\", \"pump\": \"NON_ACTIVE\"}",
                        1, voltage_flame, flame, gas_value);
        }
        else{
            sprintf(json, "{\"id\": \"%d\", \"voltage\": \"%.2f\", \"flame\": \"%d\", \"gas\": \"%.2d\", \"pump\": \"ACTIVE\"}",
                        1, voltage_flame, flame, gas_value);
        }
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

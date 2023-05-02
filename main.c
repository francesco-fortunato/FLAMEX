#include <stdio.h>
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

#define MQTT_BROKER_ADDR "192.168.54.13"
#define MQTT_TOPIC "flamex"
#define MQTT_VERSION_v311               4       /* MQTT v3.1.1 version is 4 */
#define COMMAND_TIMEOUT_MS              4000


#ifndef DEFAULT_MQTT_CLIENT_ID
#define DEFAULT_MQTT_CLIENT_ID          ""
#endif

#ifndef DEFAULT_MQTT_USER
#define DEFAULT_MQTT_USER               ""
#endif

#ifndef DEFAULT_MQTT_PWD
#define DEFAULT_MQTT_PWD                ""
#endif

/**
 * @brief Default MQTT port
 */
#define DEFAULT_MQTT_PORT               1883

/**
 * @brief Keepalive timeout in seconds
 */
#define DEFAULT_KEEPALIVE_SEC           10

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

int main(void)
{
    puts("Hello World!");

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
    u8g2_ClearBuffer(&u8g2);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);

    

    //Timer
    xtimer_ticks32_t last = xtimer_now();
    int sample = 0;
    int flame = 0;
    float voltage = 0;

    #ifdef MODULE_LWIP
    /* let LWIP initialize */
    ztimer_sleep(ZTIMER_MSEC, 1 * MS_PER_SEC);
#endif

    ztimer_sleep(ZTIMER_MSEC, 10 * MS_PER_SEC);

    // Initialize network
    NetworkInit(&network);

    MQTTClientInit(&client, &network, COMMAND_TIMEOUT_MS, buf, BUF_SIZE,
                   readbuf,
                   BUF_SIZE);
    printf("Running mqtt paho example. Type help for commands info\n");

    MQTTStartTask(&client);

    // Connect to MQTT broker
    mqtt_connect();

    //Loop
    while(1){
        sample = adc_sample(ADC_IN_USE, ADC_RES);
        voltage = adc_util_map(sample, ADC_RES, 5000, 0);
        flame = adc_util_map(sample, ADC_RES, 100, 1);

        if (flame > FLAME_THRESHOLD) { // Flame detected
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
            sprintf(voltage_str, "%.2f mV FIREEEEE", voltage);
        }
        else{
            sprintf(voltage_str, "%.2f mV NO FIRE :)", voltage);
        }
        u8g2_ClearBuffer(&u8g2);
        u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);
        u8g2_DrawStr(&u8g2, 0, 20, voltage_str);
        u8g2_SendBuffer(&u8g2);

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

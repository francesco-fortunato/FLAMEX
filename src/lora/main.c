/*
 * Copyright (C) 2023 Francesco Fortunato
 *
 * This file is subject to the terms and conditions of the MIT License.
 * See the file LICENSE in the top level directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       Example demonstrating the use of LoRaWAN with RIOT
 *
 * @authors      Francesco Fortunato <francesco.fortunato1999@gmail.com>,
 *               Valerio Francione <francione97@gmail.com>,
 *               Andrea Sepielli <andreasepielli97@gmail.com>
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

#define IR_FLAME_PIN        ADC_LINE(0)
#define GAS_PIN             ADC_LINE(2)
#define ADC_RES             ADC_RES_12BIT
#define BUZZER_PIN          GPIO23
#define RELAY_PIN           GPIO12
#define DELAY               (60 * US_PER_SEC)
#define FLAME_THRESHOLD     70 // Set threshold for flame value here

#include "periph/pm.h"
#if IS_USED(MODULE_PERIPH_RTC)
#include "periph/rtc.h"
#else
#include "timex.h"
#include "ztimer.h"
#endif
#include "xtimer.h"

#include "net/loramac.h"
#include "semtech_loramac.h"

/* Low-power mode level */
#define PM_LOCK_LEVEL       (1)

#define MAX_JOIN_RETRIES 3

extern semtech_loramac_t loramac;
#if !IS_USED(MODULE_PERIPH_RTC)
static ztimer_t timer;
#endif

#ifdef USE_OTAA
static uint8_t deveui[LORAMAC_DEVEUI_LEN];
static uint8_t appeui[LORAMAC_APPEUI_LEN];
static uint8_t appkey[LORAMAC_APPKEY_LEN];
#endif

uint8_t *msg_to_be_sent;
char* msg_received = NULL;

char stack1[THREAD_STACKSIZE_MAIN];
char stack2[THREAD_STACKSIZE_MAIN];
kernel_pid_t thread_pid_buzzer;
kernel_pid_t thread_pid_pump;

int sample_fire = 0;
int sample_gas = 0;
int flame = 0;
int gas_value = 0;
float voltage_flame = 0;
float voltage_gas = 0;

bool fire;
bool gas;
bool buzz;
bool pump;

bool joinLoRaNetwork(void) {

    int joinRetries = 0;

    while (joinRetries < MAX_JOIN_RETRIES) {
        /* Start the Over-The-Air Activation (OTAA) procedure to retrieve the
         * generated device address and to get the network and application session
         * keys.
         */
        printf("Starting join procedure (attempt %d)\n", joinRetries + 1);
        if (semtech_loramac_join(&loramac, LORAMAC_JOIN_OTAA) == SEMTECH_LORAMAC_JOIN_SUCCEEDED) {
            printf("Join procedure succeeded\n");
            return true; // Join successful, return true
        } else {
            printf("Join procedure failed\n");
            joinRetries++;
        }
    }

    printf("Exceeded maximum join retries\n");
    return false; // Join failed after maximum retries, return false
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
    // Seed the random number generator
    printf("FLAMEX: a LoRaWAN Class A low-power application\n");
    printf("===============================================\n");

    /*
     * Enable deep sleep power mode (e.g. STOP mode on STM32) which
     * in general provides RAM retention after wake-up.
     */
#if IS_USED(MODULE_PM_LAYERED)
    for (unsigned i = 1; i < PM_NUM_MODES - 1; ++i) {
        pm_unblock(i);
    }
#endif

#ifdef USE_OTAA /* OTAA activation mode */
    /* Convert identifiers and keys strings to byte arrays */
    fmt_hex_bytes(deveui, CONFIG_LORAMAC_DEV_EUI_DEFAULT);
    fmt_hex_bytes(appeui, CONFIG_LORAMAC_APP_EUI_DEFAULT);
    fmt_hex_bytes(appkey, CONFIG_LORAMAC_APP_KEY_DEFAULT);
    semtech_loramac_set_deveui(&loramac, deveui);
    semtech_loramac_set_appeui(&loramac, appeui);
    semtech_loramac_set_appkey(&loramac, appkey);

    /* Use a fast datarate, e.g. BW125/SF7 in EU868 */
    semtech_loramac_set_dr(&loramac, LORAMAC_DR_5);

    /* Join the network if not already joined */
    if (!semtech_loramac_is_mac_joined(&loramac)) {
        /* Start the Over-The-Air Activation (OTAA) procedure to retrieve the
         * generated device address and to get the network and application session
         * keys.
         */    
        if (!joinLoRaNetwork()) {
            printf("Failed to join the network\n");
            return 1;
        }

#ifdef MODULE_PERIPH_EEPROM
        /* Save current MAC state to EEPROM */
        semtech_loramac_save_config(&loramac);
#endif
    }
#endif

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
    
    fire = false;
    gas = false;
    pump = false;
    buzz = false;

    while (1) {

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
        msg_to_be_sent = (uint8_t *)json;

        printf("Sending: %s\n", msg_to_be_sent);
        uint8_t ret = semtech_loramac_send(&loramac, (uint8_t*)msg_to_be_sent, strlen(json));
        if (ret != SEMTECH_LORAMAC_TX_DONE)
        {
            printf("Cannot send message '%s', ret code: %d\n", msg_to_be_sent, ret);
            free(msg_to_be_sent);
        }

        xtimer_periodic_wakeup(&last, DELAY);
    }
}

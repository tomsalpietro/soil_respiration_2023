/**
 ************************************************************************
 * @file inc/motor.c
 * @author Thomas Salpietro 45822490
 * @date 06/04/2023
 * @brief Contains source code for the motor driver
 **********************************************************************
 * */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>
#include "motor.h"
#include "wifi.h"

LOG_MODULE_REGISTER(soil_respiration_chamber);

//IO 25 and 26
#define SW1_NODE DT_ALIAS(sw1)
#define SW2_NODE DT_ALIAS(sw2)
#define TICKS_PER_SEC               10000
#define FIVE_SEC_TIMEOUT_MS         5000
#define ONE_MIN_TIMEOUT_MS          60000
#define ONE_HOUR_TIMEOUT_MS         3600000
#define FIFTEEN_MIN_TIMEOUT_MS      900000
#define FORTY_FIVE_MIN_TIMEOUT_MS   2700000
#define FIVE_MIN_TIMEOUT_MS         300000
#define SIX_SEC_TIMEOUT_MS          6000

static const struct gpio_dt_spec motorUp = GPIO_DT_SPEC_GET_OR(SW1_NODE, gpios, {0});
static const struct gpio_dt_spec motorDown = GPIO_DT_SPEC_GET_OR(SW2_NODE, gpios, {0});

States state = INIT;
int64_t period = (int64_t)FIFTEEN_MIN_TIMEOUT_MS;


void thread_entry_motor(void) {
    int ret;
    	
    ret = gpio_pin_configure_dt(&motorUp, GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
		printk("Error %d: failed to configure UP device %s pin %d\n",
		    ret, motorUp.port->name, motorUp.pin);
		//motorUp.port = NULL;
	} else {
		printk("Set up UP at %s pin %d\n", motorUp.port->name, motorUp.pin);
	}


    ret = gpio_pin_configure_dt(&motorDown, GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
		printk("Error %d: failed to configure DOWN device %s pin %d\n",
		    ret, motorDown.port->name, motorDown.pin);
		//motorDown.port = NULL;
	} else {
		printk("Set up DOWN at %s pin %d\n", motorDown.port->name, motorDown.pin);
	}

    
    int64_t upTime = 0;

    if (!gpio_is_ready_dt(&motorUp)) {
        printk("Up not ready\r\n");
		return 0;
	}
    if (!gpio_is_ready_dt(&motorDown)) {
        printk("down not ready\r\n");
		return 0;
	}

    

    while (true) {
        while (!wifiConnected) {
            //do nothing
            k_msleep(100);
            
        }
        

        switch(state) { 
            case INIT: 
                //init state
                //turn all the way up, then turn all the way down.
                LOG_INF("Initialising Chamber...\r\n");
                gpio_pin_set_dt(&motorUp, 1);
                k_msleep(200);
                gpio_pin_set_dt(&motorDown, 0);
                //printk("Going up\r\n");
                k_msleep(6000);
                gpio_pin_set_dt(&motorUp, 0);
                k_msleep(200);
                gpio_pin_set_dt(&motorDown, 0);
                k_msleep(1000);
                //printk("going down\r\n");
                gpio_pin_set_dt(&motorDown, 1);
                k_msleep(200);
                gpio_pin_set_dt(&motorUp, 0);
                k_msleep(6000);
                gpio_pin_set_dt(&motorDown, 0);
                
                state = SENSING_BEGIN;
                gpio_pin_set_dt(&motorDown, 1);
                gpio_pin_set_dt(&motorUp, 0);
                upTime = k_uptime_get();
                break;
            case SENSING_BEGIN:
                //turn off (DOWN) all the w
                // so if it is already set, we want to check if the uptime has ticked over 5 seconds, then switch off gpio and change state
                if (k_uptime_get() - upTime > (int64_t)SIX_SEC_TIMEOUT_MS) {
                    // so its gone on longer than 5 seconds - turn it off and change state
                    gpio_pin_set_dt(&motorDown, 0);
                    k_msleep(500);
                    gpio_pin_set_dt(&motorUp, 0);
                    upTime = k_uptime_get();
                    state = SENSING;
                    LOG_INF("Begin Sensing... \r\n");
                }
                else {
                    k_msleep(1000);
                }
                
                break;
            case SENSING:
                // this should keep track of time (1 minute), after a minute has passed, change state to SENSING_END
                if (k_uptime_get() - upTime > period) {
                    //1 minute elapsed, change state to sensing end
                    LOG_INF("Done Sensing!");
                    gpio_pin_set_dt(&motorUp, 1);
                    gpio_pin_set_dt(&motorDown, 0);
                    upTime = k_uptime_get();
                    state = SENSING_END;
                } else {
                    //15 minutes still going - do nothing
                    k_msleep(1000);
                }
                break;  
            case SENSING_END:                   
                // so if it is already set, we want to check if the uptime has ticked over 5 seconds, then switch off gpio and change state
                if (k_uptime_get() - upTime > (int64_t)SIX_SEC_TIMEOUT_MS) {
                    // so its gone on longer than 5 seconds - turn it off and change state
                    gpio_pin_set_dt(&motorUp, 0);
                    k_msleep(500);
                    gpio_pin_set_dt(&motorDown, 0);
                    upTime = k_uptime_get();
                    state = SLEEP;
                } else {
                    k_msleep(1000);
                }
                

                break;
            case SLEEP:
                k_msleep(5000);
                // do nothing state - check to see if 1 hour has passed
                if ((k_uptime_get() - upTime) > ((int64_t)ONE_HOUR_TIMEOUT_MS - period - (int64_t)SIX_SEC_TIMEOUT_MS - (int64_t)SIX_SEC_TIMEOUT_MS)) {
                    //one hour has passed - change state
                    state = SENSING_BEGIN;
                    gpio_pin_set_dt(&motorDown, 1);
                    gpio_pin_set_dt(&motorUp, 0);
                    upTime = k_uptime_get();
                } else if ((k_uptime_get() - upTime) % 60000){
                    LOG_INF("Sleep time remaining: %lld minutes", ((int64_t)ONE_HOUR_TIMEOUT_MS - period - (k_uptime_get() - upTime)) / 60000);
                }
                break; 
        }
        k_msleep(100);


    }


}
/**
 ************************************************************************
 * @file inc/sensor.c
 * @author Thomas Salpietro 45822490
 * @date 02/04/2023
 * @brief Contains source code for the sensor driver
 **********************************************************************
 * */


#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <stdio.h>
#include <zephyr/logging/log.h>

#include "scd30.h"
#include "sensirion_common.h"
#include "sensor.h"
#include "motor.h"

LOG_MODULE_REGISTER(soil_respiration_sensor);

#define SCD30_ALIAS     DT_ALIAS(i2c_0)

float sensorData[3];
bool sensorTransmit = false;


void thread_sensor_entry(void) {

    // variables
    float co2, temperature, relativeHumidity;
    int16_t err;
    uint16_t dataReady, ver;
    uint16_t intervalSeconds = 5;
    uint16_t timeout = 0;


    //i2c dev
    const struct device *i2cDev = DEVICE_DT_GET(SCD30_ALIAS);

    if (!device_is_ready(i2cDev)) {
		printk("I2C: Device is not ready.\r\n");
		return;
	}


    scd30_get_driver_version(&ver, i2cDev);
    printk("Device ready. Firmware Version: %d\r\n", ver);

    //k_msleep(25000);
    // set measurment interval - 2 seconds
    scd30_set_measurement_interval(intervalSeconds, i2cDev);
    // start measurements
    scd30_start_periodic_measurement(0, i2cDev);

    while(1) {
        if (state == SENSING){
            scd30_get_data_ready(&dataReady, i2cDev);

            
            if (dataReady) {
                //get co2
                err = scd30_read_measurement(&sensorData[0], &sensorData[1], &sensorData[2], i2cDev);
                if (err != NO_ERROR) {
                    LOG_ERR("error reading measurement\r\n");
                } else {
                    LOG_INF("measured co2 concentration: %0.2f ppm, "
                    "measured temperature: %0.2f degreeCelsius, "
                    "measured humidity: %0.2f %%RH\r\n",
                    sensorData[0], sensorData[1], sensorData[2]);
                    sensorTransmit = true;
                }
            }
        } else {
            k_msleep(5000);
        }
        /*
        else if (state == SENSING_END) {
            scd30_stop_periodic_measurement(i2cDev);
            k_msleep(4000);
        } else if (state == SENSING_BEGIN) {
            scd30_start_periodic_measurement(0, i2cDev);
            k_msleep(4000);
        }*/
        

        

    }



    
}
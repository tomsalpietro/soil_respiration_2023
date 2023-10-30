/*
 * @file src/main
 * @author Thomas Salpietro 45822490
 * @date 02/04/2023
 * @brief main.c - starting threads
 * 
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/kernel.h>
#include <zephyr/net/http/client.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(soil_respiration, LOG_LEVEL_DBG);

#include "wifi.h"
#include "mqtt.h"
#include "sockets.h"
#include "motor.h"
#include "sensor.h"
#include "ble.h"

#define WIFI_STACK_SIZE     4096
#define WIFI_PRIORITY       1

#define MQTT_STACK_SIZE     8192   
#define MQTT_PRIORITY       -3

#define HTTP_STACK_SIZE 	4096
#define HTTP_PRIORITY		-2	

#define SENSOR_STACK_SIZE 	1024
#define SENSOR_PRIORITY		1	

#define MOTOR_STACK_SIZE 	2048
#define MOTOR_PRIORITY		-1	

#define BLE_STACK_SIZE		2048
#define BLE_PRIORITY		-1


K_THREAD_DEFINE(wifi_tid, WIFI_STACK_SIZE,
	thread_wifi_entry, NULL, NULL, NULL,
	WIFI_PRIORITY, 0, 100);

K_THREAD_DEFINE(sensor_tid, SENSOR_STACK_SIZE,
	thread_sensor_entry, NULL, NULL, NULL,
	SENSOR_PRIORITY, 0, 100);


K_THREAD_DEFINE(mqtt_tid, MQTT_STACK_SIZE,
	thread_mqtt_entry, NULL, NULL, NULL,
	MQTT_PRIORITY, 0, 100);

K_THREAD_DEFINE(motor_tid, MOTOR_STACK_SIZE,
	thread_entry_motor, NULL, NULL, NULL,
	MOTOR_PRIORITY, 0, 100);

K_THREAD_DEFINE(ble_tid, BLE_STACK_SIZE,
	ble_thread_entry, NULL, NULL, NULL,
	BLE_PRIORITY, 0, 100);

int main(void) {

    k_thread_start(wifi_tid);
    k_thread_start(mqtt_tid);
	k_thread_start(sensor_tid);
	k_thread_start(motor_tid);

	k_thread_start(ble_tid);

    return 0;
}




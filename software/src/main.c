/*
 * @file src/main
 * @author Thomas Salpietro 45822490
 * @date 02/04/2023
 * @brief main.c - starting threads
 * 
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/kernel.h>

#include "wifi.h"

#define WIFI_STACK_SIZE 2048
#define WIFI_PRIORITY 1

K_THREAD_DEFINE(wifi_tid, WIFI_STACK_SIZE,
	thread_wifi_entry, NULL, NULL, NULL,
	WIFI_PRIORITY, 0, 100);

int main(void) {

    k_thread_start(wifi_tid);
    return 0;
}




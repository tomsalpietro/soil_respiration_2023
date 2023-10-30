/**
 ************************************************************************
 * @file inc/wifi.h
 * @author Thomas Salpietro 45822490
 * @date 02/04/2023
 * @brief Contains Macros and definitions for the wifi driver
 **********************************************************************
 * */

#ifndef WIFI_H
#define WIFI_H

#include <zephyr/kernel.h>

//extern struct k_sem wifiSem;
//extern struct k_sem ipv4Sem;

extern bool wifiConnected;

void thread_wifi_entry(void);

#endif
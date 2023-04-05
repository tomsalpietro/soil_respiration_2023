/**
 ************************************************************************
 * @file inc/wifi.c
 * @author Thomas Salpietro 45822490
 * @date 02/04/2023
 * @brief Contains Macros and definitions for the wifi driver
 **********************************************************************
 * */

#ifndef WIFI
#define WIFI

extern struct k_sem wifiSem;
extern struct k_sem ipv4Sem;

void thread_wifi_entry(void);

#endif
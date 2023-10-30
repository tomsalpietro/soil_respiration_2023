/**
 ************************************************************************
 * @file inc/mqtt.h
 * @author Thomas Salpietro 45822490
 * @date 06/04/2023
 * @brief Contains Macros and definitions for the mqtt driver
 **********************************************************************
 * */

#ifndef MQTT_H
#define MQTT_H

int thread_mqtt_entry(void);

struct fota_JSON {
    const char *unit;
    const char  *value;
};

extern struct fota_JSON fotaResults;


#endif
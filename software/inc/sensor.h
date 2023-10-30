/**
 ************************************************************************
 * @file inc/sensor.h
 * @author Thomas Salpietro 45822490
 * @date 02/04/2023
 * @brief Contains macros for the sensor driver 
 **********************************************************************
 * */

#ifndef SENSOR_H
#define SENSOR_H

void thread_sensor_entry(void);


extern float sensorData[3];
extern bool sensorTransmit;

#endif
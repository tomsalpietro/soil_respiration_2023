/**
 ************************************************************************
 * @file inc/motor.h
 * @author Thomas Salpietro 45822490
 * @date 06/04/2023
 * @brief Contains Macros and definitions for the motor driver
 **********************************************************************
 * */

#ifndef MOTOR_H
#define MOTOR_H

void thread_entry_motor(void);

//extern bool stateSensing;
//extern bool stateInit;
//extern bool state;

typedef enum states {
    INIT, 
    SENSING_BEGIN, 
    SENSING,
    SENSING_END,
    SLEEP
} States;

extern States state;
extern int64_t period;

#endif
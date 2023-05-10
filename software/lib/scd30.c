/**
 ************************************************************************
 * @file lib/scd30.c
 * @author Thomas Salpietro 45822490
 * @date 21/04/2023
 * @brief Contains source code for the scd30 driver
 **********************************************************************
 * */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
LOG_MODULE_DECLARE(soil_respiration, LOG_LEVEL_DBG);

#include "scd30.h"
#include "sensirion_common.h"

int16_t scd30_start_periodic_measurement(uint16_t ambient_pressure_mbar, const struct device* dev) {
    if (ambient_pressure_mbar &&
        (ambient_pressure_mbar < 700 || ambient_pressure_mbar > 1400)) {
        /* out of allowable range */
        return STATUS_FAIL;
    }

    return sensirion_i2c_write_cmd_with_args(
        SCD30_I2C_ADDRESS, SCD30_CMD_START_PERIODIC_MEASUREMENT,
        &ambient_pressure_mbar, SENSIRION_NUM_WORDS(ambient_pressure_mbar), dev);
}

int16_t scd30_stop_periodic_measurement(const struct device* dev) {
    return sensirion_i2c_write_cmd(SCD30_I2C_ADDRESS,
                                   SCD30_CMD_STOP_PERIODIC_MEASUREMENT, dev);
}

int16_t scd30_read_measurement(float* co2_ppm, float* temperature,
                               float* humidity, const struct device* dev) {
    int16_t error;
    uint8_t data[3][4];

    error =
        sensirion_i2c_write_cmd(SCD30_I2C_ADDRESS, SCD30_CMD_READ_MEASUREMENT, dev);
    if (error != NO_ERROR)
        return error;

    error = sensirion_i2c_read_words_as_bytes(SCD30_I2C_ADDRESS, &data[0][0],
                                              SENSIRION_NUM_WORDS(data), dev);
    if (error != NO_ERROR)
        return error;

    *co2_ppm = sensirion_bytes_to_float(data[0]);
    *temperature = sensirion_bytes_to_float(data[1]);
    *humidity = sensirion_bytes_to_float(data[2]);

    return NO_ERROR;
}

int16_t scd30_set_measurement_interval(uint16_t interval_sec, const struct device* dev) {
    int16_t error;

    if (interval_sec < 2 || interval_sec > 1800) {
        /* out of allowable range */
        return STATUS_FAIL;
    }

    error = sensirion_i2c_write_cmd_with_args(
        SCD30_I2C_ADDRESS, SCD30_CMD_SET_MEASUREMENT_INTERVAL, &interval_sec,
        SENSIRION_NUM_WORDS(interval_sec), dev);
    sensirion_sleep_usec(SCD30_WRITE_DELAY_US);

    return error;
}

int16_t scd30_get_data_ready(uint16_t* data_ready, const struct device* dev) {
    return sensirion_i2c_delayed_read_cmd(
        SCD30_I2C_ADDRESS, SCD30_CMD_GET_DATA_READY, 3000, data_ready,
        SENSIRION_NUM_WORDS(*data_ready), dev);
}

int16_t scd30_set_temperature_offset(uint16_t temperature_offset, const struct device* dev) {
    int16_t error;

    error = sensirion_i2c_write_cmd_with_args(
        SCD30_I2C_ADDRESS, SCD30_CMD_SET_TEMPERATURE_OFFSET,
        &temperature_offset, SENSIRION_NUM_WORDS(temperature_offset), dev);
    sensirion_sleep_usec(SCD30_WRITE_DELAY_US);

    return error;
}

int16_t scd30_set_altitude(uint16_t altitude, const struct device* dev) {
    int16_t error;

    error = sensirion_i2c_write_cmd_with_args(SCD30_I2C_ADDRESS,
                                              SCD30_CMD_SET_ALTITUDE, &altitude,
                                              SENSIRION_NUM_WORDS(altitude), dev);
    sensirion_sleep_usec(SCD30_WRITE_DELAY_US);

    return error;
}

int16_t scd30_get_automatic_self_calibration(uint8_t* asc_enabled, const struct device* dev) {
    uint16_t word;
    int16_t error;

    error = sensirion_i2c_read_cmd(SCD30_I2C_ADDRESS,
                                   SCD30_CMD_AUTO_SELF_CALIBRATION, &word,
                                   SENSIRION_NUM_WORDS(word), dev);
    if (error != NO_ERROR)
        return error;

    *asc_enabled = (uint8_t)word;

    return NO_ERROR;
}

int16_t scd30_enable_automatic_self_calibration(uint8_t enable_asc, const struct device* dev) {
    int16_t error;
    uint16_t asc = !!enable_asc;

    error = sensirion_i2c_write_cmd_with_args(SCD30_I2C_ADDRESS,
                                              SCD30_CMD_AUTO_SELF_CALIBRATION,
                                              &asc, SENSIRION_NUM_WORDS(asc), dev);
    sensirion_sleep_usec(SCD30_WRITE_DELAY_US);

    return error;
}

int16_t scd30_set_forced_recalibration(uint16_t co2_ppm, const struct device* dev) {
    int16_t error;

    error = sensirion_i2c_write_cmd_with_args(
        SCD30_I2C_ADDRESS, SCD30_CMD_SET_FORCED_RECALIBRATION, &co2_ppm,
        SENSIRION_NUM_WORDS(co2_ppm), dev);
    sensirion_sleep_usec(SCD30_WRITE_DELAY_US);

    return error;
}

int16_t scd30_read_serial(char* serial, const struct device* dev) {
    int16_t error;

    error = sensirion_i2c_write_cmd(SCD30_I2C_ADDRESS, SCD30_CMD_READ_SERIAL, dev);
    if (error)
        return error;

    sensirion_sleep_usec(SCD30_WRITE_DELAY_US);
    error = sensirion_i2c_read_words_as_bytes(
        SCD30_I2C_ADDRESS, (uint8_t*)serial, SCD30_SERIAL_NUM_WORDS, dev);
    serial[2 * SCD30_SERIAL_NUM_WORDS] = '\0';
    return error;
}

int16_t scd30_get_driver_version(uint16_t* ver, const struct device* dev) {
    return sensirion_i2c_delayed_read_cmd(
        SCD30_I2C_ADDRESS, SCD30_CMD_FW_VER, 3000, ver,
        SENSIRION_NUM_WORDS(*ver), dev);
}

uint8_t scd30_get_configured_address(void) {
    return SCD30_I2C_ADDRESS;
}

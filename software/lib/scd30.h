/**
 ************************************************************************
 * @file lib/scd30.h
 * @author Thomas Salpietro 45822490
 * @date 21/04/2023
 * @brief Contains macros for the scd30 driver
 **********************************************************************
 * */

#ifndef SCD30_H
#define SCD30_H

// I2C COMMANDS
#define SCD30_I2C_ADDRESS                       0x61    //I2C device address   
#define SCD30_CMD_START_PERIODIC_MEASUREMENT    0x0010  //Start continuous measurement
#define SCD30_CMD_STOP_PERIODIC_MEASUREMENT     0x0104  //stop continuous measurement
#define SCD30_CMD_READ_MEASUREMENT              0x0300  //read measurement 
#define SCD30_CMD_SET_MEASUREMENT_INTERVAL      0x4600  //Measurement interval for continuous measurement - non-voltaile
#define SCD30_CMD_GET_DATA_READY                0x0202  //Data ready command for reading sensor buffer
#define SCD30_CMD_SET_TEMPERATURE_OFFSET        0x5403  //set temperature offset
#define SCD30_CMD_SET_ALTITUDE                  0x5102  //compensation for altitude
#define SCD30_CMD_SET_FORCED_RECALIBRATION      0x5204  //Set forced recalibration
#define SCD30_CMD_AUTO_SELF_CALIBRATION         0x5306  //set or reset automatic self-calibration
#define SCD30_CMD_READ_SERIAL                   0xD033  // read serial number
#define SCD30_CMD_SOFT_RST                      0xD304  //soft reset
#define SCD30_CMD_FW_VER                        0xD100  //return firmware version
#define SCD30_SERIAL_NUM_WORDS                  16
#define SCD30_WRITE_DELAY_US                    20000


#define SCD30_MAX_BUFFER_WORDS 24
#define SCD30_CMD_SINGLE_WORD_BUF_LEN \
    (SENSIRION_COMMAND_SIZE + SENSIRION_WORD_SIZE + CRC8_LEN)


int16_t scd30_start_periodic_measurement(uint16_t ambient_pressure_mbar, const struct device* dev);
int16_t scd30_stop_periodic_measurement(const struct device* dev);
int16_t scd30_read_measurement(float* co2_ppm, float* temperature,
                               float* humidity, const struct device* dev);
int16_t scd30_set_measurement_interval(uint16_t interval_sec, const struct device* dev);
int16_t scd30_get_data_ready(uint16_t* data_ready, const struct device* dev);
int16_t scd30_set_temperature_offset(uint16_t temperature_offset, const struct device* dev);
int16_t scd30_set_altitude(uint16_t altitude, const struct device* dev);
int16_t scd30_get_automatic_self_calibration(uint8_t* asc_enabled, const struct device* dev);
int16_t scd30_enable_automatic_self_calibration(uint8_t enable_asc, const struct device* dev);
int16_t scd30_set_forced_recalibration(uint16_t co2_ppm, const struct device* dev);
int16_t scd30_read_serial(char* serial, const struct device* dev);
int16_t scd30_get_driver_version(uint16_t* ver, const struct device* dev);
uint8_t scd30_get_configured_address(void);

#endif
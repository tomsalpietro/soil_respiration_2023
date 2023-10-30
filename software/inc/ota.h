/**
 ************************************************************************
 * @file inc/ota.h
 * @author Thomas Salpietro 45822490
 * @date 02/04/2023
 * @brief Contains macros for the ota driver 
 **********************************************************************
 * */

#ifndef MQTT_H
#define MQTT_H



/**
 * @brief Init application
 *
 * Performs MCUBoot image check
 *
 * @return 0 on success
 * @return other error code returned by MCUBoot initialization
 */
int simple_http_ota_init(void);

/**
 * @brief Init over-the-air update
 *
 * This call starts http client to retrieve file information, performs
 * download and proper flashing into slot 1.
 *
 * @return 0 on success
 * @return other error code returned by HTTP and Flash API.
 */
int simple_http_ota_run(void);

//extern char host_ip[64];

#endif /* __SIMPLE_HTTP_OTA_H__ */
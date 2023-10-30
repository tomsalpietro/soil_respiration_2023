/**
 ************************************************************************
 * @file inc/mqtt.c
 * @author Thomas Salpietro 45822490
 * @date 06/04/2023
 * @brief Contains source code for the mqtt driver
 **********************************************************************
 * */

/*
    INCLUDES
*/
#include <zephyr/kernel.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/data/json.h>

#include "wifi.h"
#include "sensor.h"
#include "ota.h"
#include "motor.h"




#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(net_mqtt_publisher, LOG_LEVEL_DBG);

#if defined(CONFIG_MQTT_LIB_WEBSOCKET)
/* Making RX buffer large enough that the full IPv6 packet can fit into it */
#define MQTT_LIB_WEBSOCKET_RECV_BUF_LEN 1280

/* Websocket needs temporary buffer to store partial packets */
static uint8_t temp_ws_rx_buf[MQTT_LIB_WEBSOCKET_RECV_BUF_LEN];
#endif


/*
    MACROS
*/
#define MQTT_CLIENT_ID		    "zephyr_client"
#define SERVER_PORT             1883
#define SERVER_ADDR             "mqtt.tago.io"


#define APP_CONNECT_TIMEOUT_MS  2000
#define APP_SLEEP_MSECS		    500
#define APP_CONNECT_TRIES	    10
#define APP_MQTT_BUFFER_SIZE	4096

#define SIMPLE_HTTP_OTA_MAJOR_VERSION 2
#define SIMPLE_HTTP_OTA_MINOR_VERSION 2

#define RC_STR(rc) ((rc) == 0 ? "OK" : "ERROR")

#define PRINT_RESULT(func, rc) \
	LOG_INF("%s: %d <%s>", (func), rc, RC_STR(rc))

#define SUCCESS_OR_EXIT(rc) { if (rc != 0) { return 1; } }
#define SUCCESS_OR_BREAK(rc) { if (rc != 0) { break; } }

static uint8_t rx_buf[APP_MQTT_BUFFER_SIZE];
static uint8_t tx_buf[APP_MQTT_BUFFER_SIZE];
static uint8_t buffer[APP_MQTT_BUFFER_SIZE];

uint8_t flagOta = 0;

#if defined(CONFIG_DNS_RESOLVER)
static struct zsock_addrinfo hints;
static struct zsock_addrinfo *haddr;
#endif

// MQTT client struct
static struct mqtt_client client_ctx;

// MQTT BROKER parameters
static struct sockaddr_storage broker;

static uint8_t otaTopic[] = "fota/";
static uint8_t periodTopic[] = "period/";
static uint8_t topic[] = "sensor/#";
static struct mqtt_topic subs_topic;
static struct mqtt_subscription_list subs_list;

static struct zsock_pollfd fds[1];
static int nfds;
static bool connected;

struct period_JSON {
    const char *unit;
    const char  *value;
};

static const struct json_obj_descr period_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct period_JSON, unit, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct period_JSON, value, JSON_TOK_STRING),
};

struct period_JSON periodResults;

struct fota_JSON {
    const char *unit;
    const char  *value;
};

static const struct json_obj_descr fota_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct fota_JSON, unit, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct fota_JSON, value, JSON_TOK_STRING),
};

struct fota_JSON fotaResults;


static void prepare_fds(struct mqtt_client *client)
{
	if (client->transport.type == MQTT_TRANSPORT_NON_SECURE) {
		fds[0].fd = client->transport.tcp.sock;
	}
	fds[0].events = ZSOCK_POLLIN;
	nfds = 1;
}


static void clear_fds(void) {
	nfds = 0;
}

static int wait(int timeout)
{
	int ret = 0;

	if (nfds > 0) {
		ret = zsock_poll(fds, nfds, timeout);
		if (ret < 0) {
			LOG_ERR("poll error: %d", errno);
		}
	}

	return ret;
}


static ssize_t handle_published_message(const struct mqtt_publish_param *pub)
{
	int ret;
	size_t received = 0u;
	const size_t message_size = pub->message.payload.len;
	const bool discarded = message_size > APP_MQTT_BUFFER_SIZE;

	LOG_INF("RECEIVED on topic \"%s\" [ id: %u qos: %u ] payload: %u / %u B",
		(const char *)pub->message.topic.topic.utf8, pub->message_id,
		pub->message.topic.qos, message_size, APP_MQTT_BUFFER_SIZE);

	while (received < message_size) {
		uint8_t *p = discarded ? buffer : &buffer[received];

		ret = mqtt_read_publish_payload_blocking(&client_ctx, p, APP_MQTT_BUFFER_SIZE);
		if (ret < 0) {
			return ret;
		}

		received += ret;
	}

	if (!discarded) {
		LOG_HEXDUMP_DBG(buffer, MIN(message_size, 256u), "Received payload:");
	}

	return discarded ? -ENOMEM : received;
}


/*
    MQTT event handler
*/
void mqtt_evt_handler(struct mqtt_client *client,
                      const struct mqtt_evt *evt)
{
    int err;
	switch (evt->type) {
	case MQTT_EVT_CONNACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT connect failed %d", evt->result);
			break;
		}

		connected = true;
		LOG_INF("MQTT client connected!");

		break;

	case MQTT_EVT_DISCONNECT:
		LOG_INF("MQTT client disconnected %d", evt->result);

		connected = false;
        clear_fds();
		break;

	case MQTT_EVT_PUBACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT PUBACK error %d", evt->result);
			break;
		}

		LOG_INF("PUBACK packet id: %u", evt->param.puback.message_id);

		break;

	case MQTT_EVT_PUBREC:
		if (evt->result != 0) {
			LOG_ERR("MQTT PUBREC error %d", evt->result);
			break;
		}

		LOG_INF("PUBREC packet id: %u", evt->param.pubrec.message_id);

		const struct mqtt_pubrel_param rel_param = {
			.message_id = evt->param.pubrec.message_id
		};

		err = mqtt_publish_qos2_release(client, &rel_param);
		if (err != 0) {
			LOG_ERR("Failed to send MQTT PUBREL: %d", err);
		}

		break;

	case MQTT_EVT_PUBCOMP:
		if (evt->result != 0) {
			LOG_ERR("MQTT PUBCOMP error %d", evt->result);
			break;
		}

		LOG_INF("PUBCOMP packet id: %u",
			evt->param.pubcomp.message_id);

		break;

	case MQTT_EVT_PINGRESP:
		LOG_INF("PINGRESP packet");
		break;

	case MQTT_EVT_PUBLISH:
		LOG_INF("PUBLISH received from TAGO");
		const struct mqtt_publish_param *pub = &evt->param.publish;
		
		handle_published_message(pub);

		
		if (!strcmp((const char *)pub->message.topic.topic.utf8, "period/")) {
			// read period payload and update
			json_obj_parse(buffer, sizeof(buffer), period_descr, ARRAY_SIZE(period_descr), &periodResults);
			LOG_INF("Sensor period changed to: %s minutes", periodResults.value);
			period = atoi(periodResults.value) * 1000 * 60;

		} else if (!strcmp((const char *)pub->message.topic.topic.utf8, "fota/") || !strcmp((const char *)pub->message.topic.topic.utf8, "fota/d/")) {
			//FOTA NOW
			
			json_obj_parse(buffer, sizeof(buffer), fota_descr, ARRAY_SIZE(fota_descr), &fotaResults);
			
			//strncpy(host_ip, fotaResults.value, strlen(fotaResults.value));
			LOG_INF("FOTA initiated...");
			flagOta = 1;
		} else {
			//do nothing;
		}


		break;

	default:
		break;
	}
}



static void broker_init(void) {

	struct sockaddr_in *broker4 = (struct sockaddr_in *)&broker;

	broker4->sin_family = AF_INET;
	broker4->sin_port = htons(SERVER_PORT);
#if defined(CONFIG_DNS_RESOLVER)
	net_ipaddr_copy(&broker4->sin_addr,
			&net_sin(haddr->ai_addr)->sin_addr);
#else
	zsock_inet_pton(AF_INET, SERVER_ADDR, &broker4->sin_addr);
#endif

}




/*
    Connect to MQTT Broker
*/
static int connect_to_broker(struct mqtt_client *client)
{
	int rc, i = 0;

	while (i++ < APP_CONNECT_TRIES && !connected) {
		rc = mqtt_connect(client);
		if (rc != 0) {
			PRINT_RESULT("mqtt_connect", rc);
			k_sleep(K_MSEC(APP_SLEEP_MSECS));
			continue;
		}

		prepare_fds(client);

		if (wait(APP_CONNECT_TIMEOUT_MS)) {
			mqtt_input(client);
		}

		if (!connected) {
			mqtt_abort(client);
		}
	}

	if (connected) {
		return 0;
	}

	return -EINVAL;
}

#if defined(CONFIG_DNS_RESOLVER)
static int get_mqtt_broker_addrinfo(void)
{
	int retries = 3;
	int rc = -EINVAL;

	while (retries--) {
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = 0;

		rc = zsock_getaddrinfo("mqtt.tago.io", "1883",
				       &hints, &haddr);
		if (rc == 0) {
			LOG_INF("DNS resolved for %s:%d",
			"mqtt.tago.io",
			1883);

			return 0;
		}

		LOG_ERR("DNS not resolved for %s:%d, retrying",
			"mqtt.tago.io",
			1883);
	}

	return rc;
}
#endif

static void subscribe_ota(struct mqtt_client *client)
{
	int err;

	/* subscribe */
	subs_topic.topic.utf8 = otaTopic;
	subs_topic.topic.size = strlen(otaTopic);
	subs_list.list = &subs_topic;
	subs_list.list_count = 1U;
	subs_list.message_id = 1U;
	LOG_INF("Subscribing to %hu topic(s)", subs_list.list_count);

	err = mqtt_subscribe(client, &subs_list);
	if (err) {
		LOG_ERR("Failed on topic %s", otaTopic);
	}
}

static void subscribe_period(struct mqtt_client *client) {
	int err;
	/* subscribe */
	subs_topic.topic.utf8 = periodTopic;
	subs_topic.topic.size = strlen(periodTopic);
	subs_list.list = &subs_topic;
	subs_list.list_count = 1U;
	subs_list.message_id = 1U;
	LOG_INF("Subscribing to %hu topic(s)", subs_list.list_count);

	err = mqtt_subscribe(client, &subs_list);
	if (err) {
		LOG_ERR("Failed on topic %s", otaTopic);
	}
}

static int publish(struct mqtt_client *client, enum mqtt_qos qos, uint8_t topic[])
{
	struct mqtt_publish_param param;
	uint8_t payload[1280];
	(void)snprintf(payload, sizeof(payload),
		       "%f,%f,%f,",
		       (double)sensorData[0], (double)sensorData[1], (double)sensorData[2]);

	param.message.topic.qos = qos;
	param.message.topic.topic.utf8 = topic;
	param.message.topic.topic.size =
			strlen(param.message.topic.topic.utf8);
	param.message.payload.data = payload;
	param.message.payload.len =
			strlen(param.message.payload.data);
	param.message_id = 69;
	param.dup_flag = 0U;
	param.retain_flag = 0U;

	return mqtt_publish(client, &param);
}

static int publish_fw(struct mqtt_client *client, enum mqtt_qos qos, uint8_t topic[])
{
	struct mqtt_publish_param param;
	uint8_t payload[1280];
	(void)snprintf(payload, sizeof(payload),
		       "%d.%d",
		       (int)SIMPLE_HTTP_OTA_MAJOR_VERSION, (int)SIMPLE_HTTP_OTA_MINOR_VERSION);

	param.message.topic.qos = qos;
	param.message.topic.topic.utf8 = topic;
	param.message.topic.topic.size =
			strlen(param.message.topic.topic.utf8);
	param.message.payload.data = payload;
	param.message.payload.len =
			strlen(param.message.payload.data);
	param.message_id = 69;
	param.dup_flag = 0U;
	param.retain_flag = 0U;

	return mqtt_publish(client, &param);
}






int thread_mqtt_entry(void) {

    int rc, val, timeout;
	int ret;
    //init client and broker
	//k_msleep(25000);
	while (!wifiConnected) {
		//do nothing
		k_msleep(1000);
	}
	printk("==MQTT START==\r\n");
	//printk("==FW Ver: %d.%d\r\n", SIMPLE_HTTP_OTA_MAJOR_VERSION, SIMPLE_HTTP_OTA_MINOR_VERSION);
	//k_msleep(15000);
#if defined(CONFIG_DNS_RESOLVER)
	rc = get_mqtt_broker_addrinfo();
	if (rc) {
		printk("Failed to resolve. Thread ended\r\n");
		return 0;
	}
#endif

    mqtt_client_init(&client_ctx);
    broker_init();

    // password and user for tago dashbaord
    struct mqtt_utf8 password;
    struct mqtt_utf8 user;

	password.utf8 = (uint8_t *)"260092b0-8ce9-45a0-9db2-402b4362e14c";
	password.size = strlen("260092b0-8ce9-45a0-9db2-402b4362e14c");

	user.utf8 = (uint8_t *)"Token";
	user.size = strlen("Token");

    /* MQTT client configuration */
    client_ctx.broker = &broker;
    client_ctx.evt_cb = mqtt_evt_handler;
    client_ctx.client_id.utf8 = (uint8_t *) MQTT_CLIENT_ID;
    client_ctx.client_id.size = sizeof(MQTT_CLIENT_ID) - 1;
    client_ctx.password = &password;
    client_ctx.user_name = &user;
    client_ctx.protocol_version = MQTT_VERSION_3_1_1;
    client_ctx.transport.type = MQTT_TRANSPORT_NON_SECURE;

    /* MQTT buffers configuration */
    client_ctx.rx_buf = rx_buf;
    client_ctx.rx_buf_size = sizeof(rx_buf);
    client_ctx.tx_buf = tx_buf;
    client_ctx.tx_buf_size = sizeof(tx_buf);


    // connect to broker - tago io
	printk("Attempting to connect to MQTT Broker\r\n");
	connect_to_broker(&client_ctx);
	
	subscribe_period(&client_ctx);
	subscribe_ota(&client_ctx);
    while (1) {
		if (state == SLEEP) {
			timeout = mqtt_keepalive_time_left(&client_ctx);
			//printk("TIMEOUT: %d\r\n", timeout);
			rc = zsock_poll(&fds[0], 1u, timeout);
			if (rc >= 0) {
				if (fds->revents & ZSOCK_POLLIN) {
					rc = mqtt_input(&client_ctx);
					if (rc != 0) {
						LOG_ERR("Failed to read MQTT input: %d", rc);
						
					} 
				} 
				if (fds->revents & (ZSOCK_POLLHUP | ZSOCK_POLLERR)) {
					LOG_ERR("Socket closed/error");
					
				}
				rc = mqtt_live(&client_ctx);
				if ((rc != 0) && (rc != -EAGAIN)) {
					LOG_ERR("Failed to live MQTT: %d", rc);
					
				}
			} else {
				LOG_ERR("poll failed: %d", rc);
				
			}
		} else {
			timeout = mqtt_keepalive_time_left(&client_ctx);
			//printk("TIMEOUT: %d\r\n", timeout);
			rc = zsock_poll(&fds[0], 1u, 1000);
			if (rc >= 0) {
				if (fds->revents & ZSOCK_POLLIN) {
					rc = mqtt_input(&client_ctx);
					if (rc != 0) {
						LOG_ERR("Failed to read MQTT input: %d", rc);
						
					} 
				} 
				if (fds->revents & (ZSOCK_POLLHUP | ZSOCK_POLLERR)) {
					LOG_ERR("Socket closed/error");
					
				}
				rc = mqtt_live(&client_ctx);
				if ((rc != 0) && (rc != -EAGAIN)) {
					LOG_ERR("Failed to live MQTT: %d", rc);
					
				}
			} else {
				LOG_ERR("poll failed: %d", rc);
				
			}


		}
		//k_msleep(500);
		if (flagOta) {
			// initiate ota
			ret = simple_http_ota_run();
			k_msleep(100);
			if (ret < 0) {
				//ota failed: try again
				flagOta = 1;
			} else {
				flagOta = 0;
				sys_reboot(1);
			}
			
			
		}
		if (sensorTransmit) {
			rc = publish(&client_ctx, MQTT_QOS_1_AT_LEAST_ONCE, topic);
			PRINT_RESULT("mqtt_publish", rc);
			sensorTransmit = false;			
		}

		//k_msleep(1000);

    }

}
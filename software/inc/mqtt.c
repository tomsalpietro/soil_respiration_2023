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
#include <errno.h>

#include "wifi.h"


#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(net_mqtt_publisher_sample, LOG_LEVEL_DBG);

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
#define APP_MQTT_BUFFER_SIZE	128

#define RC_STR(rc) ((rc) == 0 ? "OK" : "ERROR")

#define PRINT_RESULT(func, rc) \
	LOG_INF("%s: %d <%s>", (func), rc, RC_STR(rc))

#define SUCCESS_OR_EXIT(rc) { if (rc != 0) { return 1; } }
#define SUCCESS_OR_BREAK(rc) { if (rc != 0) { break; } }

static uint8_t rx_buf[APP_MQTT_BUFFER_SIZE];
static uint8_t tx_buf[APP_MQTT_BUFFER_SIZE];

// MQTT client struct
static struct mqtt_client client_ctx;

// MQTT BROKER parameters
static struct sockaddr_storage broker;

#if defined(CONFIG_SOCKS)
static struct sockaddr socks5_proxy;
#endif

static struct zsock_pollfd fds[1];
static int nfds;

//connected parameter
static bool connected = false;

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

/*
    MQTT event handler
*/
void mqtt_evt_handler(struct mqtt_client *client,
                      const struct mqtt_evt *evt)
{
    int err;
    printk("IN CALLBACK!\r\n");

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

	default:
		break;
	}
}

static void broker_init(void) {

	struct sockaddr_in *broker4 = (struct sockaddr_in *)&broker;

	broker4->sin_family = AF_INET;
	broker4->sin_port = htons(SERVER_PORT);
	zsock_inet_pton(AF_INET, SERVER_ADDR, &broker4->sin_addr);
#if defined(CONFIG_SOCKS)
	struct sockaddr_in *proxy4 = (struct sockaddr_in *)&socks5_proxy;

	proxy4->sin_family = AF_INET;
	proxy4->sin_port = htons(SOCKS5_PROXY_PORT);
	zsock_inet_pton(AF_INET, SOCKS5_PROXY_ADDR, &proxy4->sin_addr);
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


int thread_mqtt_entry(void) {

    int val;
    //init client and broker
    mqtt_client_init(&client_ctx);
    broker_init();
    // password and user for tago dashbaord
    struct mqtt_utf8 password = MQTT_UTF8_LITERAL("260092b0-8ce9-45a0-9db2-402b4362e14c");
    struct mqtt_utf8 user = MQTT_UTF8_LITERAL("Token");

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
#if defined(CONFIG_SOCKS)
	mqtt_client_set_proxy(client_ctx, &socks5_proxy,
			      socks5_proxy.sa_family == AF_INET ?
			      sizeof(struct sockaddr_in) :
			      sizeof(struct sockaddr_in6));
#endif
    k_msleep(13000);
    // connect to broker - tago io
    printk("Attempting to connect to MQTT Broker\r\n");
    connect_to_broker(&client_ctx);
    PRINT_RESULT("try_to_connect", val);
	SUCCESS_OR_EXIT(val);
    while (1) {
        k_msleep(500);
    }

}
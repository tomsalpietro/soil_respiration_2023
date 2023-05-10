/**
 ************************************************************************
 * @file inc/sockets.c
 * @author Thomas Salpietro 45822490
 * @date 02/04/2023
 * @brief Contains source code for the sockets driver for http
 **********************************************************************
 * */

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(soil_respiration, LOG_LEVEL_DBG);

#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/socketutils.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/net/http/client.h>
#include <zephyr/random/rand32.h>
#include <stdio.h>
#include <zephyr/kernel.h>

#include "sockets.h"
#include "wifi.h"
#include "sensor.h"

#define TAGOIO_SERVER 				"75.2.65.153"
//#define TAGOIO_SERVER				"api.tago.io"


#define TAGOIO_URL          		"/data"
#define HTTP_PORT           		"80"
#define DEVICE_TOKEN				"bed51d18-2744-4e8d-8959-232ab7ff5730"
#define HTTP_CONN_TIMEOUT			10
#define HTTP_PUSH_INTERVAL			10

static struct tagoio_context ctx;

static const char *tagoio_http_headers[] = {
	"Device-Token: bed51d18-2744-4e8d-8959-232ab7ff5730\r\n",
	"Content-Type: application/json\r\n",
	"_ssl: false\r\n",
	NULL
};

int tagoio_connect(struct tagoio_context *ctx)
{
	struct addrinfo *addr;
	struct addrinfo hints;
	char hr_addr[INET6_ADDRSTRLEN];
	char *port;
	int dns_attempts = 3;
	int ret = -1;

	memset(&hints, 0, sizeof(hints));

	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	if (IS_ENABLED(CONFIG_NET_IPV6)) {
		hints.ai_family = AF_INET6;
	} else if (IS_ENABLED(CONFIG_NET_IPV4)) {
		hints.ai_family = AF_INET;
	}
	port = HTTP_PORT;

	while (dns_attempts--) {
		ret = getaddrinfo(TAGOIO_SERVER, port, &hints, &addr);
		if (ret == 0) {
			break;
		}
		k_sleep(K_SECONDS(1));
	}

	if (ret < 0) {
		LOG_ERR("Could not resolve dns, error: %d", ret);
		return ret;
	}

	LOG_DBG("%s address: %s",
		(addr->ai_family == AF_INET ? "IPv4" : "IPv6"),
		net_addr_ntop(addr->ai_family,
			      &net_sin(addr->ai_addr)->sin_addr,
			      hr_addr, sizeof(hr_addr)));

	ctx->sock = socket(hints.ai_family,
			   hints.ai_socktype,
			   hints.ai_protocol);
	if (ctx->sock < 0) {
		LOG_ERR("Failed to create %s HTTP socket (%d)",
			(addr->ai_family == AF_INET ? "IPv4" : "IPv6"),
			-errno);

		freeaddrinfo(addr);
		return -errno;
	}

	if (connect(ctx->sock, addr->ai_addr, addr->ai_addrlen) < 0) {
		LOG_ERR("Cannot connect to %s remote (%d)",
			(addr->ai_family == AF_INET ? "IPv4" : "IPv6"),
			-errno);

		freeaddrinfo(addr);
		return -errno;
	}

	freeaddrinfo(addr);

	return 0;
}

int tagoio_http_push(struct tagoio_context *ctx,
		     http_response_cb_t resp_cb)
{
	struct http_request req;
	int ret;

	memset(&req, 0, sizeof(req));

	req.method		= HTTP_POST;
	req.host		= TAGOIO_SERVER;
	req.port		= HTTP_PORT;
	req.url			= TAGOIO_URL;
	req.header_fields	= tagoio_http_headers;
	req.protocol		= "HTTP/1.1";
	req.response		= resp_cb;
	req.payload		= ctx->payload;
	req.payload_len		= strlen(ctx->payload);
	req.recv_buf		= ctx->resp;
	req.recv_buf_len	= sizeof(ctx->resp);

	ret = http_client_req(ctx->sock, &req,
			      HTTP_CONN_TIMEOUT * MSEC_PER_SEC,
			      ctx);

	close(ctx->sock);
	ctx->sock = -1;

	return ret;
}




static void response_cb(struct http_response *rsp,
			enum http_final_call final_data,
			void *user_data)
{
	if (final_data == HTTP_DATA_MORE) {
		LOG_DBG("Partial data received (%zd bytes)", rsp->data_len);
	} else if (final_data == HTTP_DATA_FINAL) {
		LOG_DBG("All the data received (%zd bytes)", rsp->data_len);
	}

	LOG_DBG("Response status %s", rsp->http_status);
}

static int collect_data(void)
{
	#define lower 20000
	#define upper 100000
	#define base  1000.00f

	float temp;

	/* Generate a temperature between 20 and 100 celsius degree */
	/* SWAP THESE TWO LINES WITH GET FROM QUEUE OR SOMETHING*/
	//temp = ((sys_rand32_get() % (upper - lower + 1)) + lower);
	//temp /= base;


	(void)snprintf(ctx.payload, sizeof(ctx.payload),
		       "{\"variable\": \"co2\","
		       "\"unit\": \"ppm\",\"value\": %f}",
		       (double)sensorData[0]);

	/* LOG doesn't print float #18351 */
	LOG_INF("CO2: %d", (int) sensorData[0]);

	return 0;
}

static void next_turn(void)
{
	if (collect_data() < 0) {
		LOG_INF("Error collecting data.");
		return;
	}

	if (tagoio_connect(&ctx) < 0) {
		LOG_INF("No connection available.");
		return;
	}

	if (tagoio_http_push(&ctx, response_cb) < 0) {
		LOG_INF("Error pushing data.");
		return;
	}
}

void thread_http_entry(void)
{
	LOG_INF("TagoIO IoT - HTTP Client - Soil Respiration");

	

	while (true) {
		printk("SEMAPHORE COUNT: %d\r\n", k_sem_count_get(&wifiSem));
		if (k_sem_count_get(&wifiSem) ==  0) {
			//semaphore taken: wifi connected
			next_turn();
		} else {
			continue;
		}
		

		k_sleep(K_SECONDS(HTTP_PUSH_INTERVAL));
	}
}


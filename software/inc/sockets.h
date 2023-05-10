/**
 ************************************************************************
 * @file inc/sockets.h
 * @author Thomas Salpietro 45822490
 * @date 02/04/2023
 * @brief Contains macros for the sockets driver for http
 **********************************************************************
 * */


#ifndef HTTP_H
#define HTTP_H

#define TAGOIO_SOCKET_MAX_BUF_LEN 1280

struct tagoio_context {
	int sock;
	uint8_t payload[TAGOIO_SOCKET_MAX_BUF_LEN];
	uint8_t resp[TAGOIO_SOCKET_MAX_BUF_LEN];
};

int tagoio_connect(struct tagoio_context *ctx);
int tagoio_http_push(struct tagoio_context *ctx,
		     http_response_cb_t resp_cb);

void thread_http_entry(void);

#endif

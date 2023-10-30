/**
 ************************************************************************
 * @file inc/ble.c
 * @author Thomas Salpietro 45822490
 * @date 06/04/2023
 * @brief Contains source code for the ble driver
 **********************************************************************
 * */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_backend_ble.h>

#include "wifi.h"
#include "motor.h"

LOG_MODULE_REGISTER(ble_backend);

#define MAJOR_VERSION 4
#define MINOR_VERSION 1

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, LOGGER_BACKEND_BLE_ADV_UUID_DATA)
};

static void start_adv(void) {

	int err;

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		LOG_ERR("BLE - Advertising failed to start (err %d)", err);
		return;
	}

	LOG_INF("BLE - Advertising successfully started");
}

static void connected(struct bt_conn *conn, uint8_t err) {

	if (err) {
		LOG_ERR("BLE - Connection failed (err 0x%02x)", err);
	} else {
		LOG_INF("BLE - Connected");
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {

	LOG_INF("BLE - Disconnected (reason 0x%02x)", reason);
	start_adv();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void auth_cancel(struct bt_conn *conn) {

	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("BLE - Pairing cancelled: %s", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
	.cancel = auth_cancel,
};

void backend_ble_hook(bool status, void *ctx) {

	ARG_UNUSED(ctx);

	if (status) {
		LOG_INF("BLE Logger Backend enabled: FW Version %d.%d", MAJOR_VERSION, MINOR_VERSION);
	} else {
		LOG_INF("BLE Logger Backend disabled.");
	}
}




void ble_thread_entry(void) {

    int err;

	LOG_INF("BLE LOG Thread");
	logger_backend_ble_set_hook(backend_ble_hook, NULL);
	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return 0;
	}

    bt_conn_auth_cb_register(&auth_cb_display);

	start_adv();


    while (1) {
		//uint32_t uptime_secs = k_uptime_get_32()/1000U;

		//LOG_INF("Uptime %d secs", uptime_secs);
		k_sleep(K_MSEC(1000));
        if (!wifiConnected && state != INIT) {
            LOG_ERR("Device not connected to Wi-Fi network - requires a restart");
        }
	}
	//return 0;
    

}
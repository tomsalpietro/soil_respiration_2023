/**
 ************************************************************************
 * @file inc/wifi.c
 * @author Thomas Salpietro 45822490
 * @date 02/04/2023
 * @brief Contains source code for the wifi driver
 **********************************************************************
 * */

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_mgmt.h>
#include <errno.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(soil_respiration, LOG_LEVEL_DBG);

#define NET_SSID        "Toms Phone"
#define PSK             "tomsalpietro"

static struct net_mgmt_event_callback wifiCallback;
static struct net_mgmt_event_callback ipv4Callback;

//static K_SEM_DEFINE(wifiSem, 0, 1);
//static K_SEM_DEFINE(ipv4Sem, 0, 1);

struct k_sem wifiSem;
struct k_sem ipv4Sem;


static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;


/*
    handler for wifi connecting callback
*/
static void handle_wifi_connect_cb(struct net_mgmt_event_callback *cb) {

    const struct wifi_status *status = (const struct wifi_status *)cb->info;

    if (status->status)
    {
        printk("Connection request failed (%d)\n", status->status);
    }
    else
    {
        printk("Connected\n");
        k_sem_give(&wifiSem);
    }
}

/*
    Handler for wifi disconnecting callback
*/
static void handle_wifi_disconnect_cb(struct net_mgmt_event_callback *cb) {

    const struct wifi_status *status = (const struct wifi_status *)cb->info;

    if (status->status)
    {
        printk("Disconnection request (%d)\n", status->status);
    }
    else
    {
        printk("Disconnected\n");
        k_sem_take(&wifiSem, K_NO_WAIT);
    }
}

/*
    IPv4 handler for assigning IP adress callback
*/
static void handle_ipv4_cb(struct net_if *iface) {

    int i = 0;

    for (i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {

        char buf[NET_IPV4_ADDR_LEN];

        if (iface->config.ip.ipv4->unicast[i].addr_type != NET_ADDR_DHCP) {
            continue;
        }

        printk("IPv4 address: %s\n",
                net_addr_ntop(AF_INET,
                                &iface->config.ip.ipv4->unicast[i].address.in_addr,
                                buf, sizeof(buf)));
        printk("Subnet: %s\n",
                net_addr_ntop(AF_INET,
                                &iface->config.ip.ipv4->netmask,
                                buf, sizeof(buf)));
        printk("Router: %s\n",
                net_addr_ntop(AF_INET,
                                &iface->config.ip.ipv4->gw,
                                buf, sizeof(buf)));
        }

        k_sem_give(&ipv4Sem);
}

/*
    request wifi status
*/
void wifi_status(void) {

    struct net_if *iFace = net_if_get_default();
    
    struct wifi_iface_status status = {0};

    if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iFace, &status,	sizeof(struct wifi_iface_status)))
    {
        printk("WiFi Status Request Failed\n");
    }

    printk("\n");

    if (status.state >= WIFI_STATE_ASSOCIATED) {
        printk("SSID: %-32s\n", status.ssid);
        printk("Band: %s\n", wifi_band_txt(status.band));
        printk("Channel: %d\n", status.channel);
        printk("Security: %s\n", wifi_security_txt(status.security));
        printk("RSSI: %d\n", status.rssi);
    }
}

/*
    Connect to wifi network
*/
void wifi_connect(void) {

    struct net_if *iface = net_if_get_default();

    struct wifi_connect_req_params wifi_params = {0};

    wifi_params.ssid = NET_SSID;
    wifi_params.psk = PSK;
    wifi_params.ssid_length = strlen(NET_SSID);
    wifi_params.psk_length = strlen(PSK);
    wifi_params.channel = WIFI_CHANNEL_ANY;
    wifi_params.security = WIFI_SECURITY_TYPE_PSK;
    wifi_params.band = WIFI_FREQ_BAND_2_4_GHZ; 
    wifi_params.mfp = WIFI_MFP_OPTIONAL;

    printk("Connecting to SSID: %s\n", wifi_params.ssid);

    if (net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &wifi_params, sizeof(struct wifi_connect_req_params)))
    {
        printk("WiFi Connection Request Failed\n");
    }
}

/*
    system call to disconnect wifi connection
*/
void wifi_disconnect(void)
{
    struct net_if *iface = net_if_get_default();

    if (net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0))
    {
        printk("WiFi Disconnection Request Failed\n");
    }
}

/*
    handler for wifi events
*/
static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event, struct net_if *iface) 
{
    switch (mgmt_event) {
        case NET_EVENT_WIFI_CONNECT_RESULT:
            handle_wifi_connect_cb(cb);
            break;

        case NET_EVENT_WIFI_DISCONNECT_RESULT:
            handle_wifi_disconnect_cb(cb);
            break;

        case NET_EVENT_IPV4_ADDR_ADD:
            handle_ipv4_cb(iface);
            break;

        default:
            break;
    }
}



/*
    Thread Entry for wifi functionality
*/
void thread_wifi_entry(void) {
    
    printk("--START UP--\r\n");

    //initialise semaphores
    k_sem_init(&wifiSem, 0, 1);
    k_sem_init(&ipv4Sem, 0, 1);

    //initialise callbacks
    net_mgmt_init_event_callback(&ipv4_cb, wifi_mgmt_event_handler, NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_init_event_callback(&wifi_cb, wifi_mgmt_event_handler,
                                 NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT);

    net_mgmt_add_event_callback(&wifi_cb);
    net_mgmt_add_event_callback(&ipv4_cb);

    //connect to network
    wifi_connect();
    k_sem_take(&wifiSem, K_FOREVER);
    //k_sleep(K_SECONDS(5));

    k_sem_take(&ipv4Sem, K_FOREVER);

    printk("Ready...\n\n");
    wifi_status();
    while (1) {
        k_msleep(500);
    }

    k_sleep(K_SECONDS(15));

    wifi_disconnect();
    
}
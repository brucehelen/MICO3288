/**
******************************************************************************
* @file    wifi_station.c
* @author  William Xu
* @version V1.0.0
* @date    21-May-2015
* @brief   First MiCO application to say hello world!
******************************************************************************
*
*  The MIT License
*  Copyright (c) 2014 MXCHIP Inc.
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is furnished
*  to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in
*  all copies or substantial portions of the Software.
*
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
*  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
*  IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************
*/

#include "MICO.h"
#include "SocketUtils.h"
#include "micokit_ext.h"
#include "json_c/json.h"
#include "temp_hum_sensor\DHT11\DHT11.h"


#define wifi_station_log(M, ...) custom_log("WIFI", M, ##__VA_ARGS__)

// Server Address: 192.168.1.200:8888
/* remote ip address */
static char tcp_remote_ip[16] = "192.168.1.200";
/* remote port */
static int tcp_remote_port = 8888;
/* current WiFi status */
static WiFiEvent g_current_wifi_status;
/* reconnect TCP server timer */
static mico_timer_t timer_handle;
/* reconnect TCP server time interval */
#define RECONNECT_TIME      10000
/* send sensor data to tcp server interval, unit: second */
#define SEND_SENSOR_DATA_INTERVAL   120

void tcp_connect_thread(void *arg);
void tcp_send_thread(void *arg);
char* make_sensor_data(uint8_t temp, uint8_t hum, uint16_t light_sonsor);
void handle_recv_data(char *buf, int len);


static void micoNotify_ConnectFailedHandler(OSStatus err, void* inContext)
{
    wifi_station_log("join Wlan failed Err: %d", err);
}

static void micoNotify_WifiStatusHandler(WiFiEvent event, void* inContext)
{
    OSStatus err;

    g_current_wifi_status = event;

    switch (event)
    {
    case NOTIFY_STATION_UP:
        wifi_station_log("Station up");
        rgb_led_open(255, 0, 0);
        OLED_Clear();
        OLED_ShowString(0, 0, "WiFi OK");
        /* Create tcp thread */
        err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "Connect Thread", tcp_connect_thread, 500, NULL);
        if (err != kNoErr) wifi_station_log("Create TCP Connect Thread error[%d]", err);
        OLED_ShowString(0, 2, "TCP connecting...");
        break;
    case NOTIFY_STATION_DOWN:
        wifi_station_log("Station down");
        OLED_Clear();
        OLED_ShowString(0, 0, "WiFi Down");
        break;
    default:
        wifi_station_log("micoNotify_WifiStatusHandler status unknown[%d]", event);
        break;
    }
}

// connect to tcp server thread
void tcp_connect_thread(void *arg)
{
    OSStatus err;
    struct sockaddr_t addr;
    struct timeval_t t;
    fd_set readfds;
    int tcp_fd = -1, len;
    char *buf = NULL;

    buf = (char *)malloc(1024);
    require_action(buf, exit, err = kNoMemoryErr);

    tcp_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    require_action(IsValidSocket(tcp_fd), exit, err = kNoResourcesErr);

    wifi_station_log("tcp_fd = %d", tcp_fd);

    addr.s_ip = inet_addr(tcp_remote_ip);
    addr.s_port = tcp_remote_port;

    wifi_station_log("Connecting to server: ip=%s, port=%d", tcp_remote_ip, tcp_remote_port);
    err = connect(tcp_fd, &addr, sizeof(addr));
    require_noerr(err, exit);
    rgb_led_open(0, 255, 0);

    snprintf(buf, 1024, "%s   ", tcp_remote_ip);
    OLED_ShowString(0, 2, (uint8_t *)buf);
    snprintf(buf, 1024, "%d   ", tcp_remote_port);
    OLED_ShowString(0, 4, (uint8_t *)buf);

    /* Create tcp send thread */
    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "Send Thread", tcp_send_thread, 1024, (void *)tcp_fd);
    if (err != kNoErr) wifi_station_log("Create TCP send Thread error[%d]", err);

    t.tv_sec = 2;
    t.tv_usec = 0;

    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(tcp_fd, &readfds);

        require_action(select(tcp_fd + 1, &readfds, NULL, NULL, &t) >= 0, exit, err = kConnectionErr);

        /* recv wlan data */
        if (FD_ISSET(tcp_fd, &readfds))
        {
            len = recv(tcp_fd, buf, 1024 - 1, 0);
            require_action(len >= 0, exit, err = kConnectionErr);
            if (len == 0) {
                wifi_station_log("TCP Client is disconnected, fd: %d", tcp_fd);
                goto exit;
            }
            buf[len] = 0;
            handle_recv_data(buf, len);
        }
    }

exit:
    if (err != kNoErr) wifi_station_log("TCP client thread exit with err: %d", err);
    if (buf != NULL) free(buf);
    rgb_led_open(0, 0, 0);

    // if wifi connected OK, reconnect TCP server
    if (g_current_wifi_status == NOTIFY_STATION_UP) {
        mico_start_timer(&timer_handle);
    }
    close(tcp_fd);
    mico_rtos_delete_thread(NULL);
}

// send tcp data thread
void tcp_send_thread(void *arg)
{
    OSStatus err1;
    OSStatus err2;
    int tcp_fd = (int)arg;
    int len = 0;
    uint8_t dht11_temp_data = 0;
    uint8_t dht11_hum_data = 0;
    uint16_t light_sensor_data = 0;
    char *buf;

    wifi_station_log("tcp_send_thread, fd = %d", tcp_fd);

    for (;;) {
        err1 = DHT11_Read_Data(&dht11_temp_data, &dht11_hum_data);
        err2 = light_sensor_read(&light_sensor_data);
        if (err1 == kNoErr && err2 == kNoErr) {
            buf = make_sensor_data(dht11_temp_data, dht11_hum_data, light_sensor_data);
            len = send(tcp_fd, buf, strlen(buf), 0);
            /* free memory */
            free(buf);
            buf = NULL;
            if (len < 0) {
                wifi_station_log("tcp send error[%d]", len);
                goto exit;
            }
        }

        mico_thread_sleep(SEND_SENSOR_DATA_INTERVAL);
    }

exit:
    wifi_station_log("tcp_send_thread exit");
    if (buf != NULL) free(buf);
    mico_rtos_delete_thread(NULL);
}

/* send sensor data with josn format to server */
char* make_sensor_data(uint8_t temp, uint8_t hum, uint16_t light_sonsor)
{
    /*1:construct json object*/
    struct json_object *recv_json_object = NULL;
    recv_json_object = json_object_new_object();

    struct json_object *device_object = NULL;
    device_object = json_object_new_object();

    json_object_object_add(device_object, "Hardware", json_object_new_string("MiCOKit3288"));
    json_object_object_add(device_object, "temp", json_object_new_int(temp));
    json_object_object_add(device_object, "hub", json_object_new_int(hum));
    json_object_object_add(device_object, "light", json_object_new_int(light_sonsor));
    json_object_object_add(recv_json_object, "device_info", device_object);

    const char *p = json_object_to_json_string(recv_json_object);
    char *buf = (char *)malloc(strlen(p) + 1);
    memcpy(buf, p, strlen(p) + 1);

    /* free memory */
    json_object_put(recv_json_object);
    recv_json_object = NULL;

    /* free memory */
    json_object_put(device_object);
    device_object = NULL;

    return buf;
}

void handle_recv_data(char *buf, int len)
{
    struct json_object *deal_json_object = NULL;
    deal_json_object = json_tokener_parse(buf);

    /* parse json object */
    json_object *parse_json_object = json_object_object_get(deal_json_object, "device_info");

    /* get data one by one */
    json_object_object_foreach(parse_json_object, key, val) {
        if (!strcmp(key, "RGBSwitch")) {
            int value = json_object_get_int(val);
            wifi_station_log("val = %d", value);
        }
    }

    json_object_put(parse_json_object);
    json_object_put(deal_json_object);
}

// time out handle
void reconnect_tcp_server(void* arg)
{
    OSStatus err;

    mico_stop_timer(&timer_handle);

    OLED_ShowString(0, 2, "Connect server");

    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "Connect Thread", tcp_connect_thread, 1024, NULL);
    if (err != kNoErr) wifi_station_log("Create TCP Connect Thread error[%d]", err);
}

// application
int application_start(void)
{
    OSStatus err = kNoErr;
    network_InitTypeDef_adv_st wNetConfigAdv = { 0 };

    MicoInit();

    /* init OLED module */
    OLED_Init();
    OLED_ShowString(0, 0, "System Start");

    /* init DH11 */
    err = DHT11_Init();
    require_noerr_action(err, exit, wifi_station_log("ERROR: Unable to Init DHT11"));

    /* init light sensor */
    err = light_sensor_init();
    require_noerr_action(err, exit, wifi_station_log("ERROR: Unable to Init light sensor"));

    /* init RGB led */
    rgb_led_init();
    rgb_led_open(255, 255, 255);

    /* init reconnect to tcp server timer */
    err = mico_init_timer(&timer_handle, RECONNECT_TIME, reconnect_tcp_server, NULL);
    require_noerr_action(err, exit, wifi_station_log("ERROR: init timer error"));

    /* Register user function when wlan connection status is changed */
    err = mico_system_notify_register(mico_notify_WIFI_STATUS_CHANGED, (void *)micoNotify_WifiStatusHandler, NULL);
    require_noerr(err, exit);

    /* Register user function when wlan connection is faile in one attempt */
    err = mico_system_notify_register(mico_notify_WIFI_CONNECT_FAILED, (void *)micoNotify_ConnectFailedHandler, NULL);
    require_noerr(err, exit);

    /* Initialize wlan parameters */
    strcpy((char*)wNetConfigAdv.ap_info.ssid, "TP-LINK_797B5E");  /* wlan ssid string */
    strcpy((char*)wNetConfigAdv.key, "Bruce142336");              /* wlan key string or hex data in WEP mode */
    wNetConfigAdv.key_len = strlen("Bruce142336");                /* wlan key length */
    wNetConfigAdv.ap_info.security = SECURITY_TYPE_AUTO;          /* wlan security mode */
    wNetConfigAdv.ap_info.channel = 0;                            /* Select channel automatically */
    wNetConfigAdv.dhcpMode = DHCP_Client;                         /* Fetch Ip address from DHCP server */
    wNetConfigAdv.wifi_retry_interval = 1000;                     /* Retry interval after a failure connection */

    /* Connect WiFi Now! */
    wifi_station_log("WiFi connecting to %s...", wNetConfigAdv.ap_info.ssid);
    micoWlanStartAdv(&wNetConfigAdv);

    mico_thread_sleep(MICO_NEVER_TIMEOUT);

exit:
    mico_rtos_delete_thread(NULL);
    return err;
}

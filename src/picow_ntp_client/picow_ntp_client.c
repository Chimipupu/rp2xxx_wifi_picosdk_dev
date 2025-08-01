/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "common.h"
#include "app_ntp.h"
#include "pico/cyw43_arch.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"

uint8_t g_wifi_init_status_flg = ERROR_WIFI_NONE;

static void ntp_result(NTP_T *p_ntp_state, int status, time_t *p_result);

void print_wifi_info(void)
{
    // SSID表示
    printf("WiFi SSID : %s\n", WIFI_SSID);

    // IPアドレス表示
    ip4_addr_t ip = netif_default->ip_addr;
    printf("WiFi IP Address: %s\n", ip4addr_ntoa(&ip));
}

static void ntp_result(NTP_T *p_ntp_state, int status, time_t *p_result)
{
    if (status == 0 && p_result) {
        // UTC
        struct tm *utc = gmtime(p_result);
        printf("NTP(UTC) : %04d/%02d/%02d %02d:%02d:%02d\n",
                utc->tm_year + 1900, utc->tm_mon + 1, utc->tm_mday,
                utc->tm_hour, utc->tm_min, utc->tm_sec);
        // JST
        time_t jst = *p_result + 9 * 3600;
        struct tm *p_local = gmtime(&jst);
        printf("NTP(JST) : %04d/%02d/%02d %02d:%02d:%02d\n",
                p_local->tm_year + 1900, p_local->tm_mon + 1, p_local->tm_mday,
                p_local->tm_hour, p_local->tm_min, p_local->tm_sec);
    }

    if (p_ntp_state->ntp_resend_alarm > 0) {
        cancel_alarm(p_ntp_state->ntp_resend_alarm);
        p_ntp_state->ntp_resend_alarm = 0;
    }
    p_ntp_state->ntp_test_time = make_timeout_time_ms(NTP_TEST_TIME);
    p_ntp_state->dns_request_sent = false;
}

static int64_t ntp_failed_handler(alarm_id_t id, void *user_data);

// Make an NTP request
static void ntp_request(NTP_T *state) {
    // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
    // You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
    // these calls are a no-op and can be omitted, but it is a good practice to use them in
    // case you switch the cyw43_arch type later.
    cyw43_arch_lwip_begin();
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, NTP_MSG_LEN, PBUF_RAM);
    uint8_t *req = (uint8_t *) p->payload;
    memset(req, 0, NTP_MSG_LEN);
    req[0] = 0x1b;
    udp_sendto(state->ntp_pcb, p, &state->ntp_server_address, NTP_PORT);
    pbuf_free(p);
    cyw43_arch_lwip_end();
}

static int64_t ntp_failed_handler(alarm_id_t id, void *user_data)
{
    NTP_T* state = (NTP_T*)user_data;
    printf("ntp request failed\n");
    ntp_result(state, -1, NULL);
    return 0;
}

// Call back with a DNS result
static void ntp_dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg) {
    NTP_T *state = (NTP_T*)arg;
    if (ipaddr) {
        state->ntp_server_address = *ipaddr;
        printf("NTP IP Addr = %s, URL : %s\n", ipaddr_ntoa(ipaddr), NTP_SERVER_JP);
        ntp_request(state);
    } else {
        printf("ntp dns request failed\n");
        ntp_result(state, -1, NULL);
    }
}

// NTP data received
static void ntp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    NTP_T *state = (NTP_T*)arg;
    uint8_t mode = pbuf_get_at(p, 0) & 0x7;
    uint8_t stratum = pbuf_get_at(p, 1);

    // Check the result
    if (ip_addr_cmp(addr, &state->ntp_server_address) && port == NTP_PORT && p->tot_len == NTP_MSG_LEN &&
        mode == 0x4 && stratum != 0) {
        uint8_t seconds_buf[4] = {0};
        pbuf_copy_partial(p, seconds_buf, sizeof(seconds_buf), 40);
        uint32_t seconds_since_1900 = seconds_buf[0] << 24 | seconds_buf[1] << 16 | seconds_buf[2] << 8 | seconds_buf[3];
        uint32_t seconds_since_1970 = seconds_since_1900 - NTP_DELTA;
        time_t epoch = seconds_since_1970;
        ntp_result(state, 0, &epoch);
    } else {
        printf("invalid ntp response\n");
        ntp_result(state, -1, NULL);
    }
    pbuf_free(p);
}

// Perform initialisation
static NTP_T* ntp_init(void) {
    NTP_T *state = (NTP_T*)calloc(1, sizeof(NTP_T));
    if (!state) {
        printf("failed to allocate state\n");
        return NULL;
    }
    state->ntp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (!state->ntp_pcb) {
        printf("failed to create pcb\n");
        free(state);
        return NULL;
    }
    udp_recv(state->ntp_pcb, ntp_recv, state);
    return state;
}

// Runs ntp test forever
void run_ntp_test(void) {
    NTP_T *state = ntp_init();

    if (!state)
        return;

    while(true)
    {
        if (absolute_time_diff_us(get_absolute_time(), state->ntp_test_time) < 0 && !state->dns_request_sent) {
            // Set alarm in case udp requests are lost
            state->ntp_resend_alarm = add_alarm_in_ms(NTP_RESEND_TIME, ntp_failed_handler, state, true);

            // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
            // You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
            // these calls are a no-op and can be omitted, but it is a good practice to use them in
            // case you switch the cyw43_arch type later.
            cyw43_arch_lwip_begin();
            int err = dns_gethostbyname(NTP_SERVER_JP, &state->ntp_server_address, ntp_dns_found, state);
            cyw43_arch_lwip_end();

            state->dns_request_sent = true;
            if (err == ERR_OK) {
                ntp_request(state); // Cached result
            } else if (err != ERR_INPROGRESS) { // ERR_INPROGRESS means expect a callback
                printf("dns request failed\n");
                ntp_result(state, -1, NULL);
            }
        }
#if PICO_CYW43_ARCH_POLL
        // if you are using pico_cyw43_arch_poll, then you must poll periodically from your
        // main loop (not from a timer interrupt) to check for Wi-Fi driver or lwIP work that needs to be done.
        cyw43_arch_poll();
        // you can poll as often as you like, however if you have nothing else to do you can
        // choose to sleep until either a specified time, or cyw43_arch_poll() has work to do:
        cyw43_arch_wait_for_work_until(state->dns_request_sent ? at_the_end_of_time : state->ntp_test_time);
#else
        // if you are not using pico_cyw43_arch_poll, then WiFI driver and lwIP work
        // is done via interrupt in the background. This sleep is just an example of some (blocking)
        // work you might be doing.
        sleep_ms(1000);
#endif
    }
    free(state);
}

uint8_t hw_wifi_init(void)
{
    uint8_t ret = ERROR_WIFI_NONE;
    int ret_wifi_init, ret_wifi_connect;

    // WiFiモジュール初期化
    ret_wifi_init = cyw43_arch_init();
    if (ret_wifi_init) {
        printf("failed to initialise\n");
        ret = ERROR_WIFI_MODULE_INIT;
    }

    // WiFi STAモード起動
    cyw43_arch_enable_sta_mode();

    // WiFi接続
    ret_wifi_connect = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 10000);
    if (ret_wifi_connect) {
        printf("failed to connect\n");
        ret = ERROR_WIFI_STA_CONNECT;
    }

    // SSIDとパスワードを表示
    print_wifi_info();

    return ret;
}

int main()
{
    stdio_init_all();

    // WiFi初期化
    g_wifi_init_status_flg = hw_wifi_init();

    if (g_wifi_init_status_flg == ERROR_WIFI_NONE) {
        // NTPテスト
        run_ntp_test();
    } else {
        // WiFiエラー
        printf("[ERROR] WiFi Init Fail\n");
    }

    // WiFi切断
    cyw43_arch_deinit();

    return 0;
}
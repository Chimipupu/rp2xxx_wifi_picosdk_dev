/**
 * @file app_ntp.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-07-30
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef APP_NTP_H
#define APP_NTP_H

#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

typedef struct NTP_T_ {
    ip_addr_t ntp_server_address;
    bool dns_request_sent;
    struct udp_pcb *ntp_pcb;
    absolute_time_t ntp_test_time;
    alarm_id_t ntp_resend_alarm;
} NTP_T;

#define NTP_SERVER_POOL     "pool.ntp.org"
#define NTP_SERVER_JP       "ntp.nict.jp"
// #define NTP_SERVER_JP       "ntp.jst.mfeed.ad.jp"

#define NTP_MSG_LEN         48
#define NTP_PORT            123
#define NTP_DELTA           2208988800 // seconds between 1 Jan 1900 and 1 Jan 1970
#define NTP_TEST_TIME       (30 * 1000)
#define NTP_RESEND_TIME     (10 * 1000)

#endif // APP_NTP_H
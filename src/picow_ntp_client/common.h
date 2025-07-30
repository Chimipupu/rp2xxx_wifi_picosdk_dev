/**
 * @file common.h
 * @author Chimipupu(https://github.com/Chimipupu)
 * @brief 共通ヘッダー
 * @version 0.1
 * @date 2025-07-30
 * 
 * @copyright Copyright (c) 2025 Chimipupu All Rights Reserved.
 * 
 */
#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <string.h>

#define ERROR_WIFI_NONE           0x00 // WiFiエラーなし
#define ERROR_WIFI_MODULE_INIT    0x10 // WiFiモジュールの初期化失敗エラー
#define ERROR_WIFI_STA_CONNECT    0x20 // WiFi STA 接続失敗エラー
#define ERROR_WIFI_AP_CONNECT     0x30 // WiFi AP 接続失敗エラー

extern uint8_t g_wifi_init_status_flg;

#endif // COMMON_H
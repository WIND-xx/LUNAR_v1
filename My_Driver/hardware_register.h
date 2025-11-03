#ifndef HARDWARE_REGISTER_H
#define HARDWARE_REGISTER_H

#include <stdbool.h>
#include <stdint.h>   // Ensure this header is included for fixed-width integer types

#define MAX_RESPONSE_LEN 128
#define BLE_STATUS_DEFAULT 0
#define MUSIC_ID_DEFAULT 0

uint32_t get_remaining_seconds(void);
void set_remaining_seconds(uint32_t seconds);
void set_heating_active(bool active);
bool is_heating_active(void);
void set_music_active(bool active);
bool is_music_active(void);

bool tick_timeout(uint32_t start, uint32_t timeout);

void rf_switch(uint8_t state);
void rf_level(uint8_t level);
void rf_time(uint16_t min);   // 热敷时间转换
void shutdown(void);
void update_hardware_registers(const uint16_t* data, uint16_t num);   // Ensure uint16_t is defined

void stop_heating_task(void);
void stop_music_task(void);

bool send_at_command(const char* cmd, uint32_t timeout_ms);

int query_ble_status(void);
uint8_t query_ble_cm(void);
uint8_t query_music_id(void);

#endif   // HARDWARE_REGISTER_H

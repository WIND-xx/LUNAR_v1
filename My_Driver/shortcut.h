#pragma once
#include <stdint.h>
typedef struct
{
    uint8_t heat_level;      // 位0-1
    uint8_t music_id;        // 位2-7
    uint8_t timer_minutes;   // 位8-15
} Shortcut_struct;

extern Shortcut_struct shortcut[2];

int8_t reset_act_shortcut_id(int8_t id);

void shortcut_init(void);
void save_shortcut(uint8_t shortcut_id);
void execute_shortcut_keys(uint8_t id);

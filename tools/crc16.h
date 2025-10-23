#ifndef _CRC16_H
#define _CRC16_H
#include <stdint.h>
uint16_t _calc_check_value(const uint8_t data[], uint32_t dataLen);
uint16_t _to_uint16(const uint8_t data[]);
void _from_uint16(uint16_t value, uint8_t data[]);
#endif /* _CRC16_H */
#ifndef __UTIL_HEX_H_
#define __UTIL_HEX_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

bool is_valid_hexchar( uint8_t hex );
bool is_valid_hex( uint8_t * hex, uint8_t byteLen );

uint8_t halfbyte_to_hexchar( uint8_t halfbyte );
uint8_t halfbyte_to_hexchar_lowercase( uint8_t halfbyte );
void byte_to_hex( uint8_t * hex, uint8_t * byte, uint8_t len );
void byte_to_hex_lowercase( uint8_t * hex, uint8_t * byte, uint8_t len );

uint8_t hexchar_to_halfbyte( uint8_t hexchar );
bool hex_to_byte( uint8_t * byte, uint8_t * hex, uint8_t byteLen);

#ifdef __cplusplus
}
#endif

#endif /* __UTIL_HEX_H_ */

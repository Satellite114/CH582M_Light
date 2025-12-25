#include "util-hex.h"

#define HIGH( chr )          (( chr >> 4 ) & 0x0F)
#define LOW( chr )           (chr & 0x0F)

#define IS_HEX_INT( chr )    ( '0' <= chr && chr <= '9' )
#define IS_HEX_UPPER( chr )  ( 'A' <= chr && chr <= 'F' )
#define IS_HEX_LOWER( chr )  ( 'a' <= chr && chr <= 'f' )

static uint8_t baseUpper[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
static uint8_t baseLower[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};

bool is_valid_hexchar( uint8_t hex )
{
    return (IS_HEX_INT(hex) || IS_HEX_UPPER(hex) || IS_HEX_LOWER(hex));
}

bool is_valid_hex( uint8_t * hex, uint8_t byteLen )
{
    for (uint8_t b = 0, hexLen = 2*byteLen; b < hexLen; b++)
    {
        if ( !IS_HEX_INT(hex[b]) && !IS_HEX_UPPER(hex[b]) && !IS_HEX_LOWER(hex[b]) )
        {
            return false;
        }
    }
    return true;
}


uint8_t halfbyte_to_hexchar( uint8_t halfbyte )
{
    return baseUpper[halfbyte & 0x0f];
}

uint8_t halfbyte_to_hexchar_lowercase( uint8_t halfbyte )
{
    return baseLower[halfbyte & 0x0f];
}

static void byte_to_hex_( uint8_t * hex, uint8_t * byte, uint8_t len, bool useUppercase )
{
  uint8_t * base = useUppercase ? baseUpper : baseLower;
  for (uint8_t b = 0, h = 0; b < len; b++, h+=2)
  {
      hex[h]    = base[ HIGH(byte[b]) ];
      hex[h+1]  = base[ LOW(byte[b]) ];
  }
}

void byte_to_hex( uint8_t * hex, uint8_t * byte, uint8_t len )
{
	byte_to_hex_( hex, byte, len, true );
}

void byte_to_hex_lowercase( uint8_t * hex, uint8_t * byte, uint8_t len )
{
	byte_to_hex_( hex, byte, len, false );
}


uint8_t hexchar_to_halfbyte( uint8_t hexchar )
{
    if (IS_HEX_INT(hexchar)) return hexchar - '0';
    if (IS_HEX_UPPER(hexchar)) return hexchar - 'A' + 10;
    if (IS_HEX_LOWER(hexchar)) return hexchar - 'a' + 10;
    return 0;
}

bool hex_to_byte( uint8_t * byte, uint8_t * hex, uint8_t byteLen )
{
  for (uint8_t b = 0, h = 0; b < byteLen; b++, h++)
  {
    uint8_t hi = 0, lo = 0;
    if ( IS_HEX_INT(hex[h]) )
    {
      hi = hex[h] - '0';
    }
    else if ( IS_HEX_UPPER(hex[h]) )
    {
      hi = hex[h] - 'A' + 10;
    }
    else if ( IS_HEX_LOWER(hex[h]) )
    {
      hi = hex[h] - 'a' + 10;
    }
    else
    {
      return false;
    }

    //important increment
    h++;

    if ( IS_HEX_INT(hex[h]) )
    {
      lo = hex[h] - '0';
    }
    else if ( IS_HEX_UPPER(hex[h]) )
    {
      lo = hex[h] - 'A' + 10;
    }
    else if ( IS_HEX_LOWER(hex[h]) )
    {
      lo = hex[h] - 'a' + 10;
    }
    else
    {
      return false;
    }

    // shift for high
    byte[b] = (hi << 4) | lo;
  }

  return true;
}

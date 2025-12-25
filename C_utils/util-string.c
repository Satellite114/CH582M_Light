#include "util-string.h"

uint32_t str_to_u32( uint8_t * str )
{
  uint32_t result = 0;

  uint8_t i = 0;
  while ( !IS_NUMBER( str[i] ) && IS_WHITESPACE( str[i] ) )
  {
      i++;
  }

  while ( IS_NUMBER( str[i] ) )
  {
      result = 10 * result + str[i] - '0';
      i++;
  }

  return result;
}

int32_t str_to_s32( uint8_t * str )
{
  int32_t result = 0;
  int32_t sign = 1;

  uint8_t i = 0;
  while ( !IS_NUMBER( str[i] ) && !IS_SIGN( str[i] ) && IS_WHITESPACE( str[i] ) )
  {
      i++;
  }

  if ( IS_SIGN( str[i] ) )
  {
    if ( IS_NEG( str[i] ) )
    {
      sign = -1;
    }
    i++;
  }

  while ( IS_NUMBER( str[i] ))
  {
      result = 10 * result + str[i] - '0';
      i++;
  }

  return sign * result;
}

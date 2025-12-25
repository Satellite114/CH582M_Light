#ifndef __UTIL_STRING_H_
#define __UTIL_STRING_H_


#ifdef __cplusplus
 extern "C" {
#endif


#include <stdint.h>

#define IS_WHITESPACE( chr )  ((chr) == ' ' || (chr) == '\t' || (chr) == '\n' || (chr) == '\r')
#define IS_NUMBER( chr )      ('0' <= (chr) && (chr) <= '9')
#define IS_NEG( chr )         ((chr) == '-')
#define IS_POS( chr )         ((chr) == '+')
#define IS_SIGN( chr )        (IS_NEG(chr) || IS_POS(chr))
#define IS_COMMA( chr )		  ((chr) == '.')
#define IS_EOS( chr )		  ((chr) == '\0')


static inline void u16_to_bigendian( uint8_t * dst, uint16_t u16)
 {
 	dst[0] = (u16 >> 8) & 0xFF;
 	dst[1] = u16 & 0xFF;
 }

static inline void u16_to_le( uint8_t * dst, uint16_t u16)
 {
 	dst[1] = (u16 >> 8) & 0xFF;
 	dst[0] = u16 & 0xFF;
 }
static
inline void bigendian_to_u16( uint16_t * dst, uint8_t * src )
{
	*dst = (((uint16_t)src[0]) << 8) | (uint16_t)src[1];
}
static
inline void le_to_u16( uint16_t * dst, uint8_t * src )
{
	*dst = (((uint16_t)src[1]) << 8) | (uint16_t)src[0];
}

static
inline void s16_to_bigendian( uint8_t * dst, int16_t s16 )
{
	dst[0] = (s16 >> 8) & 0xFF;
	dst[1] = s16 & 0xFF;
}
static
inline void bigendian_to_s16( int16_t * dst, uint8_t * src )
{
	*dst = (((int16_t)src[0]) << 8) | (int16_t)src[1];
}


static
inline void u24_to_bigendian( uint8_t * dst, uint32_t u24)
{
	dst[0] = (u24 >> 16) & 0xFF;
	dst[1] = (u24 >> 8) & 0xFF;
	dst[2] = u24 & 0xFF;
}
static
inline void bigendian_to_u24( uint32_t * dst, uint8_t * src )
{
	*dst = (((uint32_t)src[0]) << 16) | (((uint32_t)src[1]) << 8) | (uint32_t)src[2];
}


static
inline void s24_to_bigendian( uint8_t * dst, int32_t s24 )
{
	dst[0] = (s24 >> 16) & 0xFF;
	dst[1] = (s24 >> 8) & 0xFF;
	dst[2] = s24 & 0xFF;
}
static
inline void bigendian_to_s24( int32_t * dst, uint8_t * src )
{
	*dst = (((int32_t)src[0]) << 16) | (((int32_t)src[1]) << 8) | (int32_t)src[2];
	//if topmost bit set -> negative
	if ( (src[0] & 0x80) == 0x80)
	{
		*dst |= 0xFF000000;
	}
}


static
inline void u32_to_bigendian( uint8_t * dst, uint32_t u32 )
{
	dst[0] = (u32 >> 24) & 0xFF;
	dst[1] = (u32 >> 16) & 0xFF;
	dst[2] = (u32 >> 8) & 0xFF;
	dst[3] = u32 & 0xFF;
}
static
inline void u32_to_le( uint8_t * dst, uint32_t u32 )
{
	dst[3] = (u32 >> 24) & 0xFF;
	dst[2] = (u32 >> 16) & 0xFF;
	dst[1] = (u32 >> 8) & 0xFF;
	dst[0] = u32 & 0xFF;
}
static
inline void bigendian_to_u32( uint32_t * dst, uint8_t * src )
{
	*dst = (((uint32_t)src[0]) << 24) | (((uint32_t)src[1]) << 16) | (((uint32_t)src[2]) << 8) | (uint32_t)src[3];
}
static
inline void le_to_u32( uint32_t * dst, uint8_t * src )
{
	*dst = (((uint32_t)src[3]) << 24) | (((uint32_t)src[2]) << 16) | (((uint32_t)src[1]) << 8) | (uint32_t)src[0];
}


static
inline void s32_to_bigendian( uint8_t * dst, int32_t s32 )
{
	dst[0] = (s32 >> 24) & 0xFF;
	dst[1] = (s32 >> 16) & 0xFF;
	dst[2] = (s32 >> 8) & 0xFF;
	dst[3] = s32 & 0xFF;
}
static
inline void bigendian_to_s32( int32_t * dst, uint8_t * src)
{
	*dst = (((int32_t)src[0]) << 24) | (((int32_t)src[1]) << 16) | (((int32_t)src[2]) << 8) | (int32_t)src[3];
}
static
inline void float_to_bigendian( uint8_t * dst, float f )
{
	uint8_t * b = (uint8_t*)&f;
#if __FLOAT_WORD_ORDER__ == __ORDER_BIG_ENDIAN__
	dst[0] = b[0];
	dst[1] = b[1];
	dst[2] = b[2];
	dst[3] = b[3];
#else
	dst[0] = b[3];
	dst[1] = b[2];
	dst[2] = b[1];
	dst[3] = b[0];
#endif
}
static
inline void bigendian_to_float( float * f, uint8_t * src )
{
	uint8_t * b = (uint8_t*)&f;
#if __FLOAT_WORD_ORDER__ == __ORDER_BIG_ENDIAN__
	b[0] = src[0];
	b[1] = src[1];
	b[2] = src[2];
	b[3] = src[3];
#else
	b[0] = src[3];
	b[1] = src[2];
	b[2] = src[1];
	b[3] = src[0];
#endif
}



int32_t str_to_s32( uint8_t * str );
uint32_t str_to_u32( uint8_t * str );


#ifdef __cplusplus
}
#endif

#endif /* __UTIL_STRING_H_ */

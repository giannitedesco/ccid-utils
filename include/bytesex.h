#ifndef _BYTESEX_H
#define _BYTESEX_H

/* Byte-swapping macros. Note that constant versions must actually be passed
 * literals as arguments! This is due to pre-processor badness. You have been
 * warned!
 */
#define const_bswap16(x) \
	((uint16_t)( \
		(((uint16_t)(x) & (uint16_t)0x00ffU) << 8) | \
	 	(((uint16_t)(x) & (uint16_t)0xff00U) >> 8)))

#define const_bswap32(x) \
	((uint32_t)( \
		(((uint32_t)(x) & (uint32_t)0x000000ffUL) << 24) | \
		(((uint32_t)(x) & (uint32_t)0x0000ff00UL) <<  8) | \
		(((uint32_t)(x) & (uint32_t)0x00ff0000UL) >>  8) | \
		(((uint32_t)(x) & (uint32_t)0xff000000UL) >> 24)))

static inline uint16_t sys_bswap16(uint16_t x)
{
	return const_bswap16(x);
}

static inline uint32_t sys_bswap32(uint32_t x)
{
	return const_bswap32(x);
}

static inline uint32_t sys_bswapf(uint32_t x)
{
	return const_bswap32(((float)x));
}

#if __BYTE_ORDER == __BIG_ENDIAN && !_OS__WIN32
#define sys_lefloat(x) sys_bswapf(x)
#define sys_le32(x) sys_bswap32(x)
#define sys_le16(x) sys_bswap16(x)
#define sys_befloat(x) (x)
#define sys_be32(x) (x)
#define sys_be16(x) (x)
#define const_le32(x) const_bswap32(x)
#define const_le16(x) const_bswap16(x)
#define const_be32(x) (x)
#define const_be16(x) (x)
#elif __BYTE_ORDER == __LITTLE_ENDIAN
#define sys_lefloat(x) (x)
#define sys_le32(x) (x)
#define sys_le16(x) (x)
#define sys_befloat(x) sys_bswapf(x)
#define sys_be32(x) sys_bswap32(x)
#define sys_be16(x) sys_bswap16(x)
#define const_le32(x) (x)
#define const_le16(x) (x)
#define const_be32(x) const_bswap32(x)
#define const_be16(x) const_bswap16(x)
#else
#error "PDP-11 huh?"
#endif

#endif /* _BYTESEX_H */

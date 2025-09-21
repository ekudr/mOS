
#ifndef __STDINT_H__
#define __STDINT_H__


typedef unsigned char	    __u8;
typedef signed char	        __s8;
typedef unsigned short	    __u16;
typedef signed short	    __s16;
typedef unsigned int	    __u32;
typedef signed int	        __s32;
typedef unsigned long long  __u64;
typedef signed long long    __s64;




typedef __u8	uint8_t;
typedef __u16	uint16_t;
typedef __u32	uint32_t;
typedef __u64	uint64_t;

typedef __u8	_uint8_t;
typedef __s8	_int8_t;
typedef __u16	_uint16_t;
typedef __s16	_int16_t;
typedef __u32	_uint32_t;
typedef __s32	_int32_t;
typedef __u64	_uint64_t;
typedef __s64	_int64_t;

typedef _uint8_t    uint8_t;
typedef _int8_t     int8_t;
typedef _uint16_t   uint16_t;
typedef _int16_t    int16_t;
typedef _uint32_t   uint32_t;
typedef _int32_t    int32_t;
typedef _uint64_t   uint64_t;
typedef _int64_t    int64_t;

typedef _uint8_t    uint8;
typedef _int8_t     int8;
typedef _uint16_t   uint16;
typedef _int16_t    int16;
typedef _uint32_t   uint32;
typedef _int32_t    int32;
typedef _uint64_t   uint64;
typedef _int64_t    int64;

typedef __u8	u8;
typedef __u16	u16;
typedef __u32	u32;
typedef __u64	u64;

typedef _int64_t    _intmax_t;
typedef _uint64_t   _uintmax_t;

typedef unsigned long int   uintptr_t;
typedef long int			intptr_t;


/* Greatest-width integer types */

typedef _intmax_t           intmax_t;
typedef _uintmax_t          uintmax_t;

#endif  /* __STDINT_H__ */
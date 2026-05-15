#ifndef __STDDEF_H__
#define __STDDEF_H__

#ifndef __cplusplus
typedef enum {false,true} bool;
#endif

#ifndef NULL
  /* SDCC is sensitive to NULL pointer type conversions, and C++ defines
   * NULL as zero
   */

#  if defined(SDCC) || defined(__SDCC) || defined(__cplusplus)
#    define NULL (0)
#  else
#    define NULL ((void*)0)
#  endif
#endif


#undef offsetof
#define offsetof(TYPE, MEMBER)	__builtin_offsetof(TYPE, MEMBER)

typedef unsigned long   _size_t;
typedef _size_t         size_t;


typedef long ptrdiff_t;


#endif /* __STDDEF_H__ */
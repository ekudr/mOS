#ifndef __CAP_H__
#define __CAP_H__

#include <cap_types.h>

typedef uint32_t cap_rights_t;

int create_capability(cap_type_t type, cap_rights_t rights);

#endif /* __CAP_H__ */
#include <stdint.h>

typedef unsigned long   _size_t;
typedef _size_t         size_t;


typedef long ptrdiff_t;

namespace std {
	using ::ptrdiff_t;
	using ::size_t;
#if __cplusplus >= 201103L
//	using ::max_align_t;
	using nullptr_t = decltype(nullptr);
#endif
}

#ifdef __cplusplus
extern "C" {
#endif
void *malloc(size_t size);
void free(void *ptr);
void *mmap(void *addr, uint64_t len, uint64_t flags, void *paddr);
#ifdef __cplusplus
}
#endif

void* operator new(size_t size) {
    return malloc(size);
}

void* operator new[](size_t size) {
    return malloc(size);
}

void operator delete(void* p) {
    free(p);
}

void operator delete[](void* p) {
    free(p);
}

void operator delete(void* ptr, std::size_t size) noexcept {
    ::operator delete(ptr);
}
// Clang will always generate calls to certain built-in libc functions.
// This file redirects those to the Dinosaur Planet versions.

typedef unsigned int size_t;

void *_memset(void *dest, int ch, size_t count);
void *_memcpy(void *dst, const void *src, size_t size);

__attribute__((weak)) void *memset(void *dst, int ch, size_t len) {
    return _memset(dst, ch, len);
}

__attribute__((weak)) void *memcpy(void *dst, const void *src, size_t size) {
    return _memcpy(dst, src, size);
}

// TODO: dino planet doesn't have a memmove equivalent

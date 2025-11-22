#ifndef __RECOMPDATA_H__
#define __RECOMPDATA_H__

#include "patch_helpers.h"

typedef unsigned long collection_key_t;

typedef unsigned long U32ValueHashmapHandle;

DECLARE_FUNC(U32ValueHashmapHandle, recomputil_create_u32_value_hashmap, void);
DECLARE_FUNC(void, recomputil_destroy_u32_value_hashmap, U32ValueHashmapHandle handle);
DECLARE_FUNC(int, recomputil_u32_value_hashmap_contains, U32ValueHashmapHandle handle, collection_key_t key);
DECLARE_FUNC(int, recomputil_u32_value_hashmap_insert, U32ValueHashmapHandle handle, collection_key_t key, unsigned long value);
DECLARE_FUNC(int, recomputil_u32_value_hashmap_get, U32ValueHashmapHandle handle, collection_key_t key, unsigned long* out);
DECLARE_FUNC(int, recomputil_u32_value_hashmap_erase, U32ValueHashmapHandle handle, collection_key_t key);
DECLARE_FUNC(unsigned long, recomputil_u32_value_hashmap_size, U32ValueHashmapHandle handle);

typedef unsigned long U32MemoryHashmapHandle;

DECLARE_FUNC(U32MemoryHashmapHandle, recomputil_create_u32_memory_hashmap, unsigned long element_size);
DECLARE_FUNC(void, recomputil_destroy_u32_memory_hashmap, U32MemoryHashmapHandle handle);
DECLARE_FUNC(int, recomputil_u32_memory_hashmap_contains, U32MemoryHashmapHandle handle, collection_key_t key);
DECLARE_FUNC(int, recomputil_u32_memory_hashmap_create, U32MemoryHashmapHandle handle, collection_key_t key);
DECLARE_FUNC(void*, recomputil_u32_memory_hashmap_get, U32MemoryHashmapHandle handle, collection_key_t key);
DECLARE_FUNC(int, recomputil_u32_memory_hashmap_erase, U32MemoryHashmapHandle handle, collection_key_t key);
DECLARE_FUNC(unsigned long, recomputil_u32_memory_hashmap_size, U32MemoryHashmapHandle handle);

#endif

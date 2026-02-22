#ifndef __RECOMPDATA_H__
#define __RECOMPDATA_H__

#include "patch_helpers.h"

// This file contains imports for utility data collection types.
// hashmaps: Maps arbitrary unique numeric keys to elements.
//   Amortized O(1) performance for insertion, lookup, and deletion.
// hashsets: Tracks a collection of unique numeric keys.
//   Amortized O(1) performance for insertion, lookup, and deletion.
// slotmaps: Like hashmaps, but the keys are generated instead of provided.
//   Amortized O(1) performance for insertion, lookup, and deletion. More performant than hashmaps.

// Hashmaps and slotmaps types are available that hold either 32-bit elements or data elements of a fixed element size.  

typedef unsigned long collection_key_t;

/////////////////////////
// u32 -> u32 hashmaps //
/////////////////////////

typedef unsigned long U32ValueHashmapHandle;

// Creates a hashmap that maps u32 keys to u32 values.
// Returns the handle for that hashmap.
DECLARE_FUNC(U32ValueHashmapHandle, recomputil_create_u32_value_hashmap, void);

// Destroys a u32 -> u32 hashmap.
DECLARE_FUNC(void, recomputil_destroy_u32_value_hashmap, U32ValueHashmapHandle handle);

// Checks if a u32 -> u32 hashmap contains the given key.
// Returns 1 if the key exists in the map, otherwise returns 0.
DECLARE_FUNC(int, recomputil_u32_value_hashmap_contains, U32ValueHashmapHandle handle, collection_key_t key);

// Inserts a value into a u32 -> u32 hashmap. The value is always assigned even if the key already exists.
// Returns 1 if the key was created (i.e. did not exist in the map), otherwise returns 0.
DECLARE_FUNC(int, recomputil_u32_value_hashmap_insert, U32ValueHashmapHandle handle, collection_key_t key, unsigned long value);

// Attempts to retrieve a value from a u32 -> u32 hashmap.
// Returns 1 if the key exists in the map and writes the element's value to *out, otherwise returns 0 and does not modify the contents in *out.
DECLARE_FUNC(int, recomputil_u32_value_hashmap_get, U32ValueHashmapHandle handle, collection_key_t key, unsigned long* out);

// Erases a key from a u32 -> u32 hashmap. Does nothing if the key doesn't exist in the hashmap.
// Returns 1 if the key was erased (i.e. it existed in the map), otherwise returns 0.
DECLARE_FUNC(int, recomputil_u32_value_hashmap_erase, U32ValueHashmapHandle handle, collection_key_t key);

// Gets the size of a u32 -> u32 hashmap.
// Returns the number of elements in the map.
DECLARE_FUNC(unsigned long, recomputil_u32_value_hashmap_size, U32ValueHashmapHandle handle);

//////////////////////////
// u32 -> data hashmaps //
//////////////////////////

typedef unsigned long U32MemoryHashmapHandle;

// Creates a hashmap that maps u32 keys to data of a given element size.
// Returns the handle for that hashmap.
DECLARE_FUNC(U32MemoryHashmapHandle, recomputil_create_u32_memory_hashmap, unsigned long element_size);

// Destroys a u32 -> data hashmap.
DECLARE_FUNC(void, recomputil_destroy_u32_memory_hashmap, U32MemoryHashmapHandle handle);

// Checks if a u32 -> data hashmap contains the given key.
// Returns 1 if the key exists in the map, otherwise returns 0.
DECLARE_FUNC(int, recomputil_u32_memory_hashmap_contains, U32MemoryHashmapHandle handle, collection_key_t key);

// Creates an element in a u32 -> data hashmap with the given key. Does nothing if the key already exists.
// Returns 1 if the element was created (i.e. the key did not already existed), otherwise returns 0.
DECLARE_FUNC(int, recomputil_u32_memory_hashmap_create, U32MemoryHashmapHandle handle, collection_key_t key);

// Attempts to retrieve a value from a u32 -> data hashmap.
// Returns the element's pointer if the key exists in the map, otherwise returns null.
DECLARE_FUNC(void*, recomputil_u32_memory_hashmap_get, U32MemoryHashmapHandle handle, collection_key_t key);

// Erases a key from a u32 -> data hashmap. Does nothing if the key doesn't exist in the hashmap.
// Returns 1 if the key was erased (i.e. it existed in the map), otherwise returns 0.
DECLARE_FUNC(int, recomputil_u32_memory_hashmap_erase, U32MemoryHashmapHandle handle, collection_key_t key);

// Gets the size of a u32 -> data hashmap.
// Returns the number of elements in the map.
DECLARE_FUNC(unsigned long, recomputil_u32_memory_hashmap_size, U32MemoryHashmapHandle handle);

///////////////////
// data slotmaps //
///////////////////

typedef unsigned long MemorySlotmapHandle;

// Creates a slotmap with data elements of a given element size.
// Returns the handle for that slotmap.
DECLARE_FUNC(MemorySlotmapHandle, recomputil_create_memory_slotmap, unsigned long element_size);

// Destroys a memory slotmap.
DECLARE_FUNC(void, recomputil_destroy_memory_slotmap, MemorySlotmapHandle handle);

// Checks if a memory slotmap contains the given key.
// Returns 1 if the key exists in the map, otherwise returns 0.
DECLARE_FUNC(int, recomputil_memory_slotmap_contains, MemorySlotmapHandle handle, collection_key_t key);

// Creates an element in a memory slotmap.
// Returns the created element's key.
DECLARE_FUNC(collection_key_t, recomputil_memory_slotmap_create, MemorySlotmapHandle handle);

// Attempts to retrieve an element from a u32 slotmap.
// Returns 1 if the key exists in the map and writes the element's pointer to *out, otherwise returns 0 and does not modify the contents in *out.
DECLARE_FUNC(int, recomputil_memory_slotmap_get, MemorySlotmapHandle handle, collection_key_t key, void** out);

// Erases a key from a memory slotmap.
// Returns 1 if the key was erased (i.e. it exited in the map), otherwise returns 0.
DECLARE_FUNC(int, recomputil_memory_slotmap_erase, MemorySlotmapHandle handle, collection_key_t key);

// Gets the size of a memory slotmap.
// Returns the number of elements in the map.
DECLARE_FUNC(unsigned long, recomputil_memory_slotmap_size, MemorySlotmapHandle handle);

#endif

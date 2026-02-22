#pragma once

/**
 * A mini library that allows converting strings into sequential unique IDs.
 *
 * The recomp hashmap doesn't support string keys, so this "string database"
 * can be used to convert strings into integer keys to be used in a recomp hashmap.
 * The generated IDs are just sequential incrementing numbers and are *not* something
 * like a UUID or unique hash.
 */

#include "PR/ultratypes.h"

typedef struct StringDBNode {
    u32 hash;
    struct StringDBNode *next;
    char str[];
} StringDBNode;

typedef struct {
    StringDBNode *head;
} StringDB;

typedef u32 StringID;

void stringdb_init(StringDB *db);
void stringdb_free(StringDB *db);
StringID stringdb_get_or_add(StringDB *db, const char *str, const char **outStr);

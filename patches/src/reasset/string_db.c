#include "string_db.h"

#include "patches.h"

#include "PR/ultratypes.h"
#include "PR/os.h"
#include "libc/string.h"

// http://www.cse.yorku.ca/~oz/hash.html
static u32 __sdbm_hash(const char *str) {
    u32 hash = 0;
    s32 c;

    while ((c = *str++)) {
        hash = c + (hash << 6) + (hash << 16) - hash;
    }

    return hash;
}

static s32 __string_eq(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return FALSE;
        }

        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

static s32 __stringdb_iter(StringDB *db, const char *str, u32 hash, 
        StringID *outID, StringDBNode **outNode, StringDBNode **outPrevNode) {
    StringDBNode *node = db->head;
    StringDBNode *prevNode = NULL;
    StringID id = 0;
    s32 found = FALSE;
    while (node != NULL) {
        if (node->hash == hash && __string_eq(node->str, str)) {
            found = TRUE;
            break;
        }
        id++;
        prevNode = node;
        node = node->next;
    }

    if (outID != NULL) {
        *outID = id;
    }
    if (outNode != NULL) {
        *outNode = node;
    }
    if (outPrevNode != NULL) {
        *outPrevNode = prevNode;
    }

    return found;
}

void stringdb_init(StringDB *db) {
    db->head = NULL;
}

void stringdb_free(StringDB *db) {
    StringDBNode *node = db->head;
    while (node != NULL) {
        StringDBNode *toFree = node;
        node = node->next;

        recomp_free(toFree);
    }

    db->head = NULL;
}

StringID stringdb_get_or_add(StringDB *db, const char *str, const char **outStr) {
    u32 hash = __sdbm_hash(str);

    StringID id;
    StringDBNode *node;
    StringDBNode *prevNode;
    s32 found = __stringdb_iter(db, str, hash, &id, &node, &prevNode);

    if (!found) {
        s32 strLength = strlen(str);
        node = recomp_alloc(sizeof(StringDBNode) + strLength + 1);
        node->hash = hash;
        bcopy(str, node->str, strLength + 1);

        if (prevNode == NULL) {
            db->head = node;
        } else {
            prevNode->next = node;
        }
    }

    if (outStr != NULL) {
        *outStr = node->str;
    }

    return id;
}

#include "reasset_namespace.h"

#include "patches.h"
#include "recompdata.h"

#include "reasset.h"
#include "reasset/string_db.h"

#include "PR/ultratypes.h"
#include "libc/string.h"

static StringDB sStrDB;
static U32MemoryHashmapHandle sNamespaceHashmap; // dict[namespace, namespaceData]

void reasset_namespace_init(void) {
    stringdb_init(&sStrDB);
    sNamespaceHashmap = recomputil_create_u32_memory_hashmap(sizeof(ReAssetNamespaceData));
}

static ReAssetNamespace namespace_get_or_add(const char *name, ReAssetNamespaceData **outData) {
    const char *safeName = NULL;
    StringID strid = stringdb_get_or_add(&sStrDB, name, &safeName);
    // Namespaces are only keyed by name, so use the string ID as the namespace handle.
    // +1 because namespace 0 is reserved for base assets.
    ReAssetNamespace namespace = strid + 1;

    _Bool existed = !recomputil_u32_memory_hashmap_create(sNamespaceHashmap, namespace);

    ReAssetNamespaceData *data = recomputil_u32_memory_hashmap_get(sNamespaceHashmap, namespace);
    if (!existed) {
        // First time referencing this namespace, initialize it
        data->name = safeName;

        reasset_log("[reasset] Namespace created: %s (%d)\n", safeName, namespace);
    }

    if (outData != NULL) {
        *outData = data;
    }

    return namespace;
}

_Bool reasset_namespace_lookup_name(ReAssetNamespace namespace, const char **outName) {
    if (namespace == REASSET_BASE_NAMESPACE) {
        if (outName != NULL) {
            *outName = "<BASE>";
        }

        return TRUE;
    }
    
    ReAssetNamespaceData *data = recomputil_u32_memory_hashmap_get(sNamespaceHashmap, namespace);
    if (outName != NULL) {
        *outName = data == NULL ? "<NULL>" : data->name;
    }

    return data != NULL;
}

RECOMP_EXPORT ReAssetNamespace reasset_namespace(const char *name) {
    s32 nameLen = strlen(name);
    reasset_assert(nameLen > 0 && nameLen <= 16,
        "[reasset:reasset_namespace] Invalid namespace name: \"%s\". Namespace names must be at least one character long and less than or equal to 16 characters long.");

    return namespace_get_or_add(name, /*outData=*/NULL);
}

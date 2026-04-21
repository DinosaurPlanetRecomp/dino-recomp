#pragma once

#include "reasset/reasset_namespace.h"
#include "reasset/list.h"

#include "PR/ultratypes.h"

typedef enum BitSaveType {
    BITSAVETYPE_NEVER = 0,
    BITSAVETYPE_CHECKPOINT = 1,
    BITSAVETYPE_ALWAYS = 2,
    BITSAVETYPE_MAP = 3
} BitSaveType;

typedef struct {
    u16 start;
    u16 end;
} PackedBitRange;

typedef struct {
    ReAssetNamespace namespace;
    PackedBitRange ranges[3];
} PackedBitRanges;

void reasset_bits_init(void);
void reasset_bits_repack(void);
void reasset_bits_cleanup(void);
/// Returns list[PackedBitRanges]
List* reasset_bits_get_packed_namespaces(void);
_Bool reasset_bits_get_packed_namespace(ReAssetNamespace namespace, PackedBitRanges **outRanges);

void reasset_bits_orphan_init(void);
List* reasset_bits_orphan_get_packed_namespaces(void);
PackedBitRanges* reasset_bits_orphan_add(ReAssetNamespace namespace, BitSaveType saveType, s32 length);

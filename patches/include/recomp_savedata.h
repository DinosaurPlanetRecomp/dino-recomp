#pragma once

#include "PR/ultratypes.h"
#include "dlls/engine/29_gplay.h"

typedef struct {
    u8 namespaceNameLength;
    u8 reserved;
    u16 sizeBytes;
} RecompSaveExtensionHeader;

typedef struct {
    u8 bitstring;
    u8 extension;
    u16 startOffset;
    u16 endOffset;
} RecompSaveBitstring;

typedef struct {
    s16 resolvedIdentifier;
    u8 extension; // identifies namespace
    s32 localID;  // namespaced ID
} RecompSaveMusicActionLink;

// size: 0x10
typedef struct {
/*00*/ char magic[2]; // must be 'RC'
/*02*/ u16 version;
/*04*/ u8 numExtensions;
/*05*/ u8 numBitstrings;
/*06*/ u8 numObjUIDs;
/*07*/ u8 numMusicActions;
/*08*/ u8 numLActions;
/*09*/ u8 numEnvfxActions;
/*0A*/ u8 reserved1;
/*0B*/ u8 reserved2;
/*0C*/ u8 reserved3;
/*0D*/ u8 reserved4;
/*0E*/ u8 reserved5;
/*0F*/ u8 reserved6;
} RecompSaveDataHeader;

// size: 0x4000
typedef struct {
    union {
        // Ensure recomp data is exactly 0x1800 bytes into the save data.
        // This starts off right after the flash checksum.
        u8 _basePad[0x1800];
        GplaySaveFlash base;
    };
    union {
        // Take the remaining available space as recomp data. The actual
        // size of the recomp data varies, so just leave room for the
        // largest possible size.
        u8 _recompPad[0x4000 - 0x1800];
        RecompSaveDataHeader recomp;
    };
} RecompFlashData;

void recomp_savedata_save(RecompFlashData *flash, s32 slotno);
void recomp_savedata_load(RecompFlashData *flash, s32 slotno);

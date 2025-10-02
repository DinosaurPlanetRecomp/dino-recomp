#include "custom_memory.h"

#include "patches.h"
#include "recomp_funcs.h"

#include "PR/ultratypes.h"
#include "PR/os.h"
#include "sys/memory.h"
#include "sys/interrupt_util.h"
#include "macros.h"

extern s32 get_stack_();

// @recomp: Increase size of free queue (420 -> 600)
static MemoryFreeQueueElement recomp_gMemoryFreeQueue[600];
#define gMemoryFreeQueue recomp_gMemoryFreeQueue

s32 memMonVal3;

RECOMP_PATCH void mmInit(void) {
    // Start pool memory at the end of .bss
    u32 startAddr = (u32)&bss_end;

    // Clear to 0xFF
    s32 *mem = (s32*)startAddr;
    while ((u32)mem < osMemSize)
        *mem++ = -1;

    // Initialize memory pools
    gNumMemoryPools = 0;

    if (osMemSize != EXPANSION_SIZE)
    {
        mmInitPool((void *)startAddr, MEM_POOL_AREA_NO_EXPANSION - startAddr, 1200);
    }
    else
    {
        mmInitPool((void *)MEM_POOL_AREA_00, RAM_END - MEM_POOL_AREA_00,          400);
        mmInitPool((void *)MEM_POOL_AREA_01, MEM_POOL_AREA_00 - MEM_POOL_AREA_01, 800);
        mmInitPool((void *)startAddr,        MEM_POOL_AREA_02 - startAddr,        1200);
        
        // @recomp: New pool taking up the remainder of patch memory
        startAddr = (u32)&PATCH_BSS_END;

        s32 *mem = (s32*)startAddr;
        while ((u32)mem < PATCH_RAM_END)
            *mem++ = -1;
    
        mmInitPool((void *)startAddr, PATCH_RAM_END - startAddr, 3000);
    }

    mmSetDelay(2);

    gMemoryFreeQueueLength = 0;
}

RECOMP_PATCH void *mmAlloc(s32 size, s32 tag, const char *name) {
    void *ptr;

    // @recomp: Fail predictably on negative allocations
    if (size < 0) {
        recomp_error_message_box(recomp_sprintf_helper(
            "Attempted to allocate %d bytes! Tag: %d",
            size, tag));

        *((volatile int *)0) = 0;
    }

    if (size == 0) {
        get_stack_();
        ptr = NULL;
        return ptr;
    }

    if ((size >= 4500) || (osMemSize != EXPANSION_SIZE)) {
        // >= 4500 bytes -> pool 0
        ptr = mmAllocR(0, size, tag, name);
        if (ptr == NULL) {
            get_stack_();
            // @bug ? If no expansion pack is in, memory pool 1 won't exist
            ptr = mmAllocR(1, size, tag, name);
        }
    } else if (size >= 1024) {
        // >= 1024 bytes -> pool 1
        ptr = mmAllocR(1, size, tag, name);
        if (ptr == NULL) {
            get_stack_();
            ptr = mmAllocR(2, size, tag, name);
        }
    } else {
        // < 1024 bytes -> pool 2
        ptr = mmAllocR(2, size, tag, name);
    }

    // @recomp: Fallback to extra patch memory pool
    if (ptr == NULL) {
        ptr = mmAllocR(3, size, tag, name);
    }

    if (ptr == NULL) {
        get_stack_();

        // @recomp: If all pools are full, crash with an error instead of returning null
        recomp_error_message_box(recomp_sprintf_helper(
            "All memory pools full! Failed to allocate %d bytes. Tag: %d",
            size, tag));

        *((volatile int *)0) = 0;
    }
    return ptr;
}

// @recomp: Patch to use new free queue
RECOMP_PATCH void mmSetDelay(s32 delay) {
    s32 intFlags;

    intFlags = interrupts_disable();
    gMemoryFreeDelay = delay;
    if (delay == 0) {
        while (gMemoryFreeQueueLength > 0) {
            mmFreeNow(gMemoryFreeQueue[--gMemoryFreeQueueLength].address);
        }
    }
    interrupts_enable(intFlags);
}

// @recomp: Patch to use new free queue
RECOMP_PATCH void mmFreeEnqueue(void *address) {
    // @recomp: Rather than overflow the queue, free the address immediately if the free queue is full.
    //          This seems to be a recoverable scenario, so we shouldn't intentionally crash here.
    if (gMemoryFreeQueueLength >= ((s32)ARRAYCOUNT(gMemoryFreeQueue) - 1)) {
        recomp_eprintf("Memory free queue is full! Freeing pointer %p now instead!\n");
        mmFreeNow(address);
        return;
    }

    gMemoryFreeQueue[gMemoryFreeQueueLength].address = address;
    gMemoryFreeQueue[gMemoryFreeQueueLength].ticksLeft = gMemoryFreeDelay;
    gMemoryFreeQueueLength++;
}

// @recomp: Patch to use new free queue
RECOMP_PATCH void mmFreeTick(void) {
    s32 _pad;
    s32 _pad2;
    s32 i;
    s32 intFlags;
    MemoryPoolSlot* slot;
    s32 nextIndex;

    intFlags = interrupts_disable();
    
    for (i = 0; i < gMemoryFreeQueueLength;) {
        gMemoryFreeQueue[i].ticksLeft--;
        if (gMemoryFreeQueue[i].ticksLeft == 0) {
            mmFreeNow(gMemoryFreeQueue[i].address);
            // Replace with last element
            gMemoryFreeQueue[i].address = gMemoryFreeQueue[gMemoryFreeQueueLength - 1].address;
            gMemoryFreeQueue[i].ticksLeft = gMemoryFreeQueue[gMemoryFreeQueueLength - 1].ticksLeft;
            gMemoryFreeQueueLength--;
        } else {
            i++;
        }
    }
    
    interrupts_enable(intFlags);
    
    // Update memory monitors
    memMonVal0 = 0;
    memMonVal2 = 0;
    memMonVal1 = 0;
    memMonVal3 = 0; // @recomp: mem mon
    
    slot = gMemoryPools[0].slots;
    do {
        if (slot->flags != SLOT_FREE) {
            memMonVal0 += slot->size;
        }
        nextIndex = slot->nextIndex;
        if (nextIndex != MEMSLOT_NONE) {
            slot = &gMemoryPools->slots[nextIndex];
        }
    } while (nextIndex != MEMSLOT_NONE);
    
    if ((s32) gNumMemoryPools >= 2) {
        slot = gMemoryPools[1].slots;
        do {
            if (slot->flags != SLOT_FREE) {
                memMonVal1 += slot->size;
            }
            nextIndex = slot->nextIndex;
            if (nextIndex != MEMSLOT_NONE) {
                slot = &gMemoryPools[1].slots[nextIndex];
            }
        } while (nextIndex != MEMSLOT_NONE);
        
        slot = gMemoryPools[2].slots;
        do {
            if (slot->flags != SLOT_FREE) {
                memMonVal2 += slot->size;
            }
            nextIndex = slot->nextIndex;
            if (nextIndex != MEMSLOT_NONE) {
                slot = &gMemoryPools[2].slots[nextIndex];
            }
        } while (nextIndex != MEMSLOT_NONE);

        // @recomp mem mon
        slot = gMemoryPools[3].slots;
        do {
            if (slot->flags != SLOT_FREE) {
                memMonVal3 += slot->size;
            }
            nextIndex = slot->nextIndex;
            if (nextIndex != MEMSLOT_NONE) {
                slot = &gMemoryPools[3].slots[nextIndex];
            }
        } while (nextIndex != MEMSLOT_NONE);
    }
}

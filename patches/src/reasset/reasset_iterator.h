#pragma once

#include "list.h"

#include "PR/ultratypes.h"

typedef u32 ReAssetIterator;

void reasset_iterator_init(void);
void reasset_iterator_clear_all(void);
ReAssetIterator reasset_iterator_create(U32List *list);

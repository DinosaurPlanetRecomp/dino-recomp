#pragma once

#include "reasset_id.h"
#include "list.h"

#include "PR/ultratypes.h"

typedef u32 ReAssetIterator;

void reasset_iterator_init(void);
void reasset_iterator_clear_all(void);
ReAssetIterator reasset_iterator_create(U32List *list);
ReAssetBool reasset_iterator_destroy(ReAssetIterator iterator);
ReAssetBool reasset_iterator_next(ReAssetIterator iterator, ReAssetID *outID);

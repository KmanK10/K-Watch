#pragma once

#include <stdlib.h>

#define MALLOC_CAP_SPIRAM    (1 << 0)
#define MALLOC_CAP_INTERNAL  (1 << 1)
#define MALLOC_CAP_DMA       (1 << 2)

static inline void *heap_caps_aligned_alloc(size_t alignment, size_t size, int caps)
{
    (void)caps;
    return _aligned_malloc(size, alignment);
}

static inline void *heap_caps_malloc(size_t size, int caps)
{
    (void)caps;
    return malloc(size);
}

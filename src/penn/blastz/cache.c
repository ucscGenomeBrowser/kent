#if defined(__GNUC__) && defined(__i386__)
static inline void cache_preload(const void *addr)
{
    asm volatile("" : : "r" (addr));
}

static inline void cache_prefetch_t0(const void *addr)
{
    asm volatile("prefetcht0 (%0)" :: "r" (addr));
}

static inline void cache_prefetch_nt(const void *addr)
{
    asm volatile("prefetchnta (%0)" :: "r" (addr));
}
#else
#define cache_prefetch(p)
#define cache_prefetch_t0(p)
#define cache_prefetch_nt(p)
#endif

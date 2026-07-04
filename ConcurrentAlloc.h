#pragma once

#include "ThreadCache.h"
#include "comm.h"
static ObjectPool<ThreadCache> tls_pool;

// 并发分配内存块
static void *ConcurrentAlloc(size_t size) {
    // 若分配大小大于ThreadCache大小，则直接从PageCache分配内存块
    if (size > MAX_THREAD_CACHE_SIZE) {
        // 如果此时申请257KB
        size_t alignsize = SizeClass::RoundUp(size);
        size_t nPage = alignsize >> PAGE_SHIFT;

        PageCache::GetInstance()->Lock();

        Span *span = PageCache::GetInstance()->NewSpan(nPage);
        span->_objSize = size;

        PageCache::GetInstance()->Unlock();

        void *ptr = (void *)(span->_page_id << PAGE_SHIFT);
        return ptr;
    }
    // 若ThreadCache结构体指针为空，则创建一个线程缓存
    if (TLS_pthread_cache == nullptr) {
        TLS_pthread_cache = tls_pool.New();
    }

    // 使用ThreadCache分配内存块
    return TLS_pthread_cache->Allocate(size);
}

// 并发释放内存块
static void ConcurrentFree(void *ptr) {
    assert(ptr);
    Span *span = PageCache::GetInstance()->MapSpan(ptr);

    size_t size = span->_objSize;
    
    if (size > MAX_THREAD_CACHE_SIZE) {
        // size_t alignsize = SizeClass::RoundUp(size);

        PageCache::GetInstance()->Lock();
        PageCache::GetInstance()->ReleaseSpanToPageCache(span);
        PageCache::GetInstance()->Unlock();
    } else {
        // 若ThreadCache结构体指针为空，则创建一个线程缓存
        if (TLS_pthread_cache == nullptr) {
            TLS_pthread_cache = tls_pool.New();
        }
        assert(size <= MAX_THREAD_CACHE_SIZE);
        assert(size > 0);


        TLS_pthread_cache->Deallocate(ptr, size);
    }
    // 使用ThreadCache释放内存块
}

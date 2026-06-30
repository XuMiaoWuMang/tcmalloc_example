#pragma once

#include "comm.h"
#include "ThreadCache.h"

// 并发分配内存块
static void* ConcurrentAlloc(size_t size)
{
    // 若ThreadCache结构体指针为空，则创建一个线程缓存
    if(TLS_pthread_cache == nullptr)
    {
        TLS_pthread_cache = new ThreadCache;
    }
    // 使用ThreadCache分配内存块
    return TLS_pthread_cache->Allocate(size);
}

// 并发释放内存块
static void ConcurrentFree(void* ptr, size_t size)
{
    assert(ptr);
        static int call_count = 0;
        
    // 使用ThreadCache释放内存块
    TLS_pthread_cache->Deallocate(ptr, size);
}

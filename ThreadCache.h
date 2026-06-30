#pragma once
#include "comm.h"
#include <pthread.h>
#include <string.h>
// 线程缓存
// 用哈希桶管理
class ThreadCache {
  public:
    void *Allocate(size_t size);             // 从线程缓存中分配内存
    void Deallocate(void *ptr, size_t size); // 从线程缓存中释放内存
    void *FetchFromCentralCache(size_t index,
                                size_t size);          // 从中央缓存中获取内存
    void ListTooLong(FreeList &freeList, size_t size); // 处理自由链表过长的情况

  private:
    FreeList _freeList[N_FREELIST]; // 哈希桶管理内存块
};

// 通过TLS实现线程缓存，不同线程无法访问对方的缓存。
// 最关键的是不需要锁也能并发获取ThreadCache对象。
#if defined(__WIN32) || defined(_WIN64)
static __declspec(thread) ThreadCache *TLS_pthread_cache = nullptr;
#elif defined(__x86_64__) || defined(__i386__)
static __thread ThreadCache *TLS_pthread_cache = nullptr;
#endif
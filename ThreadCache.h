#pragma once
#include "CentralCache.h"
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
extern thread_local ThreadCache *TLS_pthread_cache;
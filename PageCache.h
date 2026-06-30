#pragma once
#include "comm.h"
class PageCache {
  public:
    // 获取PageCache的单例对象
    static PageCache *GetInstance() { return &_Instance; }

    Span *NewSpan(size_t batchNum);
    void Lock() { _mutex.lock(); }
    void Unlock() { _mutex.unlock(); }

    // 用于快速查找span, key是page_id, value是span
    Span *MapSpan(void *obj);

    // 释放Span
    void ReleaseSpanToPageCache(Span *span);

  private:
    // 是管理span的数组，索引代表的是对象大小的索引，默认是513。
    SpanList _spanLists[N_PAGE_CACHE];
    // 用于快速查找span, key是page_id, value是span
    std::unordered_map<PAGE_ID, Span *> _spanMap;

    // 不能是桶锁，因为需要进行不同页数的span的合并与拆分
    std::mutex _mutex;
    // 饿汉模式
  private:
    PageCache() {};
    PageCache(const PageCache &) = delete;
    PageCache &operator=(const PageCache &) = delete;

    static PageCache _Instance;
};

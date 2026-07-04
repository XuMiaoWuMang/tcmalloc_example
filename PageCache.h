#pragma once
#include "ObjectPool.h"
#include "comm.h"
#include "RadixTree.h"
class PageCache {
  public:
    // 获取PageCache的单例对象
    static PageCache *GetInstance() { return &_Instance; }
    // 获取span的大小
    size_t GetSpanSize(void *ptr) { return MapSpan(ptr)->_objSize; }

    Span *NewSpan(size_t batchNum);
    void Lock() { _mutex.lock(); }
    void Unlock() { _mutex.unlock(); }

    // 用于快速查找span, key是page_id, value是span
    Span *MapSpan(void *obj);

    // 释放Span
    void ReleaseSpanToPageCache(Span *span);

    // void debugPrint() {
    //     for (int i = 1; i < N_PAGE_CACHE; i++) {
    //         if (!_spanLists[i].Empty()) {
    //             Span *ptr = _spanLists[i].Begin();
    //             cout << "spanList[" << i << "]: " << endl;
    //             while (ptr != _spanLists[i].End()) {
    //                 cout << " page_id: " << ptr->_page_id << "->";
    //                 ptr = ptr->_next;
    //             }
    //             cout << endl;
    //         }
    //     }
    // }

  private:
    // 是管理span的数组，索引代表的是对象大小的索引，默认是513。
    SpanList _spanLists[N_PAGE_CACHE];
    // 用于快速查找span, key是page_id, value是span
    // std::unordered_map<PAGE_ID, Span *> _spanMap;
// 使用基数树替换哈希表，减少锁竞争，提高并发性能
#if defined(_WIN32) || defined(__i386__)
    TCMalloc_PageMap2<32 - PAGE_SHIFT> _spanMap;
#elif defined(_WIN64) || defined(__x86_64__)
    TCMalloc_PageMap3<64 - PAGE_SHIFT> _spanMap;
#endif

    // 用于管理span的定长内存池
    ObjectPool<Span> _spanPool;

    // 不能是桶锁，因为需要进行不同页数的span的合并与拆分
    std::mutex _mutex;
    // 饿汉模式
  private:
    PageCache() {};
    PageCache(const PageCache &) = delete;
    PageCache &operator=(const PageCache &) = delete;

    static PageCache _Instance;
};

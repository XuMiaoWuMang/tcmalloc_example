#pragma once
#include "comm.h"
#include "PageCache.h"

class CentralCache {
  public:
    // 获取单例模式的实例
    static CentralCache *GetInstance() { return &_Instance; }
    // 从对应的哈希桶中获取内存块范围
    size_t FetchRangeObj(void *&start, void *&end, size_t batchNum,
                         size_t size);
    // 获取对应的且有空闲的Span
    Span *GetOneSpan(SpanList &spanList, size_t size);

    // 释放对应的内存块范围
    void ReleaseRangeObj(void *start, size_t size);

    // 释放对应的Span
    void ReleaseSpan(Span *span);

    void debugPrint() {
        for (int i = 0; i < N_FREELIST; i++) {
            if (!_spanLists[i].Empty()) {
                Span *ptr = _spanLists[i].Begin();
                cout << "spanList[" << i + 1 << "]: " << endl;
                while (ptr != _spanLists[i].End()) {
                    cout << " page_id[" << ptr->_page_id << "] ->UseCount: " << ptr->_useCount << endl;
                    ptr = ptr->_next;
                }
                cout << endl;
            }
        }
    }

  private:
    SpanList _spanLists[N_FREELIST]; // 哈希桶
    static CentralCache _Instance;   // 静态的单例实例

    // 默认构造函数，单例模式禁用拷贝构造和赋值重载
  private:
    CentralCache() {}
    CentralCache(const CentralCache &) = delete;
    CentralCache &operator=(const CentralCache &) = delete;
};
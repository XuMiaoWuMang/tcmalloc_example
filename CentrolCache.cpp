#include "CentrolCache.h"
#include "PageCache.h"
CentralCache CentralCache::_Instance;

Span *CentralCache::GetOneSpan(SpanList &spanList, size_t size) {
    // cout << "GetOneSpan begin..." << endl;
    // 查询是否有空闲的Span
    for (Span *span = spanList.Begin(); span != spanList.End();
         span = span->_next) {
        // cout << "span: " << span << " _freelist: " << span->_freelist <<
        // endl;
        if (span->_freelist) {
            return span;
        }
    }
    // 此处需要解锁，因为如果ThreadCache释放了对应的Span，会因为锁导致无法添加到SpanList中
    spanList._mutex.unlock();
    // 如果没有空闲的Span，向PageCache申请新的Span
    PageCache::GetInstance()->Lock();
    // cout << "NewSpan begin..." << endl;
    Span *span =
        PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
    // cout << "NewSpan end..." << endl;
    span->_isUsed = true;

    PageCache::GetInstance()->Unlock();

    // 不需要对 CentralCache 立即加桶锁
    // spanList._mutex.lock();

    // 计算Span的起始地址和大小
    char *start = (char *)(span->_page_id << PAGE_SHIFT);
    size_t bytes = span->_n << PAGE_SHIFT;
    char *end = start + bytes;
    // printf("start: 0x%x, end: 0x%x\n", start, end);

    // 切割span成多个自由链表。
    // 先使用一次头插，再将其他的块都尾插。
    // 走到这里的freeList一定为空
    span->_freelist = start;

    char *next = nullptr;
    for (next = start + size; next < end; start = next, next += size) {
        // printf("start: 0x%x, next: 0x%x\n", start, next);
        nextObj(start) = next;
    }
    nextObj(start) = nullptr; // 最后一个有效位置 null terminate

    // 在此处 CentralCache 加桶锁
    spanList._mutex.lock();
    spanList.PushFront(span);
    // cout << "GetOneSpan end..." << endl;
    return span;
}
size_t CentralCache::FetchRangeObj(void *&start, void *&end, size_t batchNum,
                                   size_t size) {

    assert(batchNum);
    assert(size);
    // cout << "FetchRangeObj begin..." << endl;
    size_t index = SizeClass::Index(size);
    _spanLists[index]._mutex.lock();

    Span *span = GetOneSpan(_spanLists[index], size);
    assert(span);
    assert(span->_freelist);

    start = span->_freelist;
    end = start;

    // 无论够不够，都尽力返回
    size_t actualNum = 1;
    while (actualNum < batchNum && nextObj(end)) {
        end = nextObj(end);
        ++actualNum;
    }

    span->_freelist = nextObj(end);
    span->_useCount += actualNum;
    nextObj(end) = nullptr;

    _spanLists[index]._mutex.unlock();
    // cout << "FetchRangeObj end..." << endl;
    return actualNum;
}
void CentralCache::ReleaseRangeObj(void *start, size_t size) {
    assert(start);
    assert(size);
    size_t index = SizeClass::Index(size);
    _spanLists[index]._mutex.lock();

    while (start) {
        void *next = nextObj(start);

        // 找到obj对应的Span对象
        Span *span = PageCache::GetInstance()->MapSpan(start);

        nextObj(start) = span->_freelist;
        span->_freelist = start;
        span->_useCount--;

        if (span->_useCount == 0) {
            // 归还Span
            _spanLists[index].Erase(span);
            span->_next = nullptr;
            span->_freelist = nullptr;
            span->_prev = nullptr;
            span->_isUsed = false;

            // 先解桶锁
            // 因为此时需要将span给PageCache，对于CentralCache来说，这个span已经不存在了，接下来是PageCache进行操作，所以可以解锁
            _spanLists[index]._mutex.unlock();

            PageCache::GetInstance()->Lock();
            PageCache::GetInstance()->ReleaseSpanToPageCache(span);
            PageCache::GetInstance()->Unlock();

            // 再加桶锁
            _spanLists[index]._mutex.lock();
        }

        start = next;
    }
    _spanLists[SizeClass::Index(size)]._mutex.unlock();
}
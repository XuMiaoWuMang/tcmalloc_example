#include "PageCache.h"
PageCache PageCache::_Instance;

Span *PageCache::NewSpan(size_t NumPage) {

    assert(NumPage > 0);
    // cout << "NewSpan begin..." << endl;

    // 如果申请页数过大，直接向系统申请空间
    if (NumPage > 64) {
        void *ptr = SystemAlloc(NumPage);
        Span *span = _spanPool.New();
        span->_n = NumPage;
        span->_page_id = (PAGE_ID)ptr >> PAGE_SHIFT;
        span->_isUsed = true;

        // for (PAGE_ID i = 0; i < span->_n; i++) {
        //     _spanMap[span->_page_id + i] = span;
        // }
        _spanMap.set(span->_page_id, span);
        return span;
    }

    // 先检查PageCache中是否有空闲的Span,如果有则返回并逻辑删除spanLists对应的span
    if (!_spanLists[NumPage].Empty()) {
        // cout << "有空闲的Span" << endl;
        Span *span = _spanLists[NumPage].PopFront();
        span->_isUsed = true;

        // if (span->_n > 1) {
        //     _spanMap[span->_page_id + span->_n - 1] = span;
        // }
        for (PAGE_ID i = 0; i < span->_n; i++) {
            _spanMap.set(span->_page_id + i, span);
        }

        return span;
    }

    // 如果没有，则查询是否存在页数更大的Span
    for (size_t i = NumPage + 1; i < N_PAGE_CACHE; i++) {
        // 如果有，则切分它
        if (!_spanLists[i].Empty()) {
            // 切割span，一份给CenCtralCache，一份给对应页数的SpanList。
            Span *span = _spanLists[i].PopFront(); // 给PageCache的Span
            Span *newSpan = _spanPool.New();       // 给CentralCache的Span
            // cout << "i: " << i << endl;
            // 依旧头切
            newSpan->_n = NumPage;
            newSpan->_page_id = span->_page_id;
            newSpan->_isUsed = true;

            span->_n -= NumPage;
            span->_page_id += NumPage;

            _spanLists[span->_n].PushFront(span);

            // 存储span的首位页的地址，方便PageCache回收时合并
            _spanMap.set(span->_page_id, span);

            if (span->_n > 1) {
                _spanMap.set(span->_page_id + span->_n - 1, span);
            }

            // 更新spanMap，方便CentralCache后续查找
            for (PAGE_ID j = 0; j < newSpan->_n; j++) {
                _spanMap.set(newSpan->_page_id + j, newSpan);
            }
            return newSpan;
        }
    }

    // 此时如果还没有，需要向系统申请Span
    // 向系统申请N_PAGE_CACHE - 1（默认256页）页内存
    Span *span = _spanPool.New();
    span->_n = N_PAGE_CACHE - 1;

    void *ptr = SystemAlloc(span->_n);
    span->_page_id = (PAGE_ID)ptr >> PAGE_SHIFT;

    _spanLists[span->_n].PushFront(span);
    return NewSpan(NumPage);
}

Span *PageCache::MapSpan(void *obj) {
    PAGE_ID page_id = ((PAGE_ID)obj >> PAGE_SHIFT);

    auto it = _spanMap.get(page_id);
    if (it == nullptr) {
        assert(false);
        return nullptr;
    }
    return (Span *)it;
}
void PageCache::ReleaseSpanToPageCache(Span *span) {
    // cout << "ReleaseSpanToPageCache begin..." << endl;
    // 所有释放的span，中间页的映射都需要删除，方便后续合并

    if (span->_n > N_PAGE_CACHE - 1) {
        // 如果释放的页数大于N_PAGE_CACHE - 1，则直接释放给系统
        SystemFree((void *)(span->_page_id << PAGE_SHIFT), span->_n);
        _spanMap.set(span->_page_id, nullptr);
        _spanPool.Delete(span);
        return;
    }
    if (span->_n > 3) {
        for (int i = 1; i < span->_n - 1; i++) {
            _spanMap.set(span->_page_id + i, nullptr);
        }
    }
    span->_isUsed = false;

    PAGE_ID next_page_id = 0;
    Span *nextSpan = nullptr;
    while (1) {
        // 后向合并，如果有空闲的页号，next_page_id合并到当前页中
        next_page_id = span->_page_id + span->_n;
        auto tmp = (Span *)_spanMap.get(next_page_id);
        if (tmp == nullptr) {
            break;
        }
        nextSpan = tmp;
        if (nextSpan->_isUsed) {
            break;
        }
        if (nextSpan->_n + span->_n >= N_PAGE_CACHE) {
            break;
        }
        // 合并next_page_id
        // 从spanList中删除next_page_id
        _spanLists[nextSpan->_n].Erase(nextSpan);
        nextSpan->_prev = nullptr;
        nextSpan->_next = nullptr;

        // 清除 nextSpan 在基数树中的所有页映射（nextSpan 即将被删除）
        for (PAGE_ID i = 0; i < nextSpan->_n; i++) {
            _spanMap.set(nextSpan->_page_id + i, nullptr);
        }

        span->_n += nextSpan->_n; // 增加当前页的页数

        _spanPool.Delete(nextSpan);
    }
    // 此时后向合并完毕

    // 开始前向合并
    PAGE_ID prev_page_id = 0;
    Span *prevSpan = nullptr;
    while (1) {
        prev_page_id = span->_page_id - 1;
        auto tmp = (Span *)_spanMap.get(prev_page_id);
        if (tmp == nullptr) {
            break;
        }
        prevSpan = tmp;
        if (prevSpan->_isUsed) {
            break;
        }
        if (prevSpan->_n + span->_n >= N_PAGE_CACHE) {
            break;
        }
        // 合并prev_page_id

        // 先把prevSpan从spanList中删除，再合并span
        _spanLists[prevSpan->_n].Erase(prevSpan);
        prevSpan->_prev = nullptr;
        prevSpan->_next = nullptr;

        // 清除 prevSpan 在基数树中的所有页映射（prevSpan 即将被删除）
        for (PAGE_ID i = 0; i < prevSpan->_n; i++) {
            _spanMap.set(prevSpan->_page_id + i, nullptr);
        }

        // 修改当前span的页号为prevSpan的首頁
        span->_page_id = prevSpan->_page_id;
        // 增加当前span的页数
        span->_n += prevSpan->_n;

        _spanPool.Delete(prevSpan);
    }
    // 此时前向合并完毕

    // 添加合并后的 span 到 spanList
    _spanLists[span->_n].PushFront(span);
    // 空闲 span 只需首尾页映射，用于后续合并查找
    _spanMap.set(span->_page_id, span);
    if (span->_n > 1) {
        _spanMap.set(span->_page_id + span->_n - 1, span);
    }
    // cout << "ReleaseSpanToPageCache end..." << endl;
}
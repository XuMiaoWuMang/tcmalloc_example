#include "PageCache.h"
#include "comm.h"

PageCache PageCache::_Instance;

Span *PageCache::NewSpan(size_t NumPage) {

    assert(NumPage <= N_PAGE_CACHE - 1 && NumPage > 0);
    // cout << "NewSpan begin..." << endl;

    // cout << "NumPage: " << NumPage << endl;
    // 先检查PageCache中是否有空闲的Span,如果有则返回并逻辑删除对应的span
    if (!_spanLists[NumPage].Empty()) {
        // cout << "有空闲的Span" << endl;
        return _spanLists[NumPage].PopFront();
    }

    // 如果没有，则查询是否存在页数很大的Span
    for (size_t i = NumPage + 1; i < N_PAGE_CACHE; i++) {
        // 如果有，则切分它
        if (!_spanLists[i].Empty()) {
            // 切割span，一份给CenCtralCache，一份给对应页数的SpanList。
            Span *span = _spanLists[i].PopFront(); // 给PageCache的Span
            Span *newSpan = new Span;              // 给CentralCache的Span
            // cout << "i: " << i << endl;
            // 依旧头切
            newSpan->_n = NumPage;
            newSpan->_page_id = span->_page_id;

            span->_n -= NumPage;
            span->_page_id += NumPage;

            _spanLists[span->_n].PushFront(span);
            // 存储span的首位页的地址，方便PageCache回收时合并

            _spanMap[span->_page_id] = span;
            _spanMap[span->_page_id + span->_n - 1] = span;

            // 更新spanMap，方便CentralCache后续查找
            for (PAGE_ID j = 0; j < newSpan->_n; j++) {
                _spanMap[newSpan->_page_id + j] = newSpan;
            }
            return newSpan;
        }
    }

    // 此时如果还没有，需要向系统申请Span
    // 向系统申请N_PAGE_CACHE - 1（默认256页）页内存
    Span *span = new Span;
    span->_n = N_PAGE_CACHE - 1;

    void *ptr = SystemAlloc(span->_n);
    span->_page_id = (PAGE_ID)ptr >> PAGE_SHIFT;

    _spanLists[span->_n].PushFront(span);
    return NewSpan(NumPage);
}
Span *PageCache::MapSpan(void *obj) {
    // 通过地址计算PAGE_ID
    PAGE_ID page_id = ((PAGE_ID)obj >> PAGE_SHIFT);
    auto span =
        _spanMap.find(page_id) != _spanMap.end() ? _spanMap[page_id] : nullptr;
    if (span == nullptr) {
        assert(false);
        return nullptr;
    }
    return span;
}
void PageCache::ReleaseSpanToPageCache(Span *span) {
    PAGE_ID next_page_id = 0;
    Span *nextSpan = nullptr;
    while (1) {
        // 后向合并，如果有空闲的页号，next_page_id合并到当前页中
        next_page_id = span->_page_id + span->_n;
        if (_spanMap.find(next_page_id) != _spanMap.end()) {
            if (!_spanMap[next_page_id]->_isUsed) {
                if (_spanMap[next_page_id]->_n + span->_n < N_PAGE_CACHE) {
                    nextSpan = _spanMap[next_page_id];
                    // 合并next_page_id
                    // // 从spanList中删除next_page_id
                    _spanLists[nextSpan->_n].Erase(nextSpan);
                    nextSpan->_prev = nullptr;
                    nextSpan->_next = nullptr;
                    span->_n += nextSpan->_n; // 增加当前页的页数

                    _spanMap[next_page_id + nextSpan->_n - 1] = _spanMap
                        [span->_page_id]; // 更新_spanMap里next_page_id的尾部地址为当前首页的地址

                    _spanMap.erase(
                        next_page_id); // 删除_spanMap里next_page_id的首部页的映射
                    _spanMap.erase(
                        span->_page_id + span->_n -
                        1); // 删除_spanMap里span->_page_id的尾部页的映射
                }
            }
        } else {
            break;
        }
    }
    // 此时后向合并完毕
    PAGE_ID prev_page_id = 0;
    Span *prevSpan = nullptr;
    while (1) {
        prev_page_id = span->_page_id - 1;
        // 前向合并，如果有空闲的页号，合并到prev_page_id中
        if (_spanMap.find(prev_page_id) != _spanMap.end()) {
            // _spanMap[prev_page_id] = nullptr;
            if (!prevSpan->_isUsed) {
                // 合并prev_page_id
                if (prevSpan->_n + span->_n < N_PAGE_CACHE) {
                    prevSpan = _spanMap[prev_page_id];

                    // 先把prevSpan从spanList中删除，再合并span->_n
                    _spanLists[prevSpan->_n].Erase(prevSpan);
                    prevSpan->_prev = nullptr;
                    prevSpan->_next = nullptr;
                    prevSpan->_n += span->_n; // 增加prev_page_id的页数
                    _spanMap[span->_page_id + span->_n - 1] =
                        prevSpan; // 修改当前span的尾部页的映射
                    _spanMap.erase(
                        span->_page_id); // 删除_spanMap里span->_page_id的首部页的映射
                    _spanMap.erase(
                        prev_page_id + span->_n -
                        1); // 删除_spanMap里prev_page_id的尾部页的映射

                    // // 从spanList中删除prev_page_id
                    // _spanLists[prevSpan->_n].Erase(prevSpan);
                }
            }
        } else {
            break;
        }
    }
    // 此时前向合并完毕

    // 添加合并的Span到spanList
    // 如果前一个pageid在_spanMap里不存在，则说明前向合并成功，反之，则失败
    if (prevSpan == nullptr) {
        // 如果前向合并失败，则添加当前span到spanList
        _spanLists[span->_n].PushFront(span);
    } else {
        // 如果前向合并成功，则添加合并后的prevSpan到spanList
        _spanLists[prevSpan->_n].PushFront(prevSpan);
    }
    // cout << "ReleaseSpanToPageCache done" << endl;
}
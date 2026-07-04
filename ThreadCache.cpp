#include "ThreadCache.h"

#include "comm.h"

// 通过TLS实现线程缓存，不同线程无法访问对方的缓存。
// 最关键的是不需要锁也能并发获取ThreadCache对象。
thread_local ThreadCache *TLS_pthread_cache = nullptr;

// 从CentralCache中获取内存块
void *ThreadCache::FetchFromCentralCache(size_t index, size_t size) {
    // cout << "FetchFromCentralCache begin..." << endl;

    assert(index < 208);
    // 慢启动约束，如果申请的内存块很大，则控制申请的内存块数量最多为2，如果申请的内存块很小，则控制申请的内存块数量最多为512。
    // 但是刚开始都是从1开始，每一次多增加1。
#if defined(_WIN32) || defined(_WIN64)
    size_t batchNum =
        min(SizeClass::NumMoveSize(size), _freeList[index].MaxSize());
#else
    size_t batchNum =
        std::min(SizeClass::NumMoveSize(size), _freeList[index].MaxSize());
#endif

    void *start = nullptr;
    void *end = nullptr;
    size_t actualNum =
        CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);
    assert(actualNum >= 1);

    if (actualNum == 1) {
        assert(start == end);
    } else {
        _freeList[index].PushRange(nextObj(start), end, actualNum - 1);
    }
    // 如果freelist的大小等于批量大小，那么就说明freelist的大小并未到达上限，可以增加freelist的大小。
    // 调用次数越多，一次返回的内存块数量就越多，返回的内存块数量越多，需要的调用次数越少。
    // 所以调用次数越多，调用次数越少。
    if (actualNum >= batchNum) {
#if defined(_WIN32) || defined(_WIN64)
        _freeList[index].MaxSize() = min(_freeList[index].MaxSize() + 1,
            SizeClass::NumMoveSize(size));
#else
        _freeList[index].MaxSize() = std::min(_freeList[index].MaxSize() + 1,
            SizeClass::NumMoveSize(size));
#endif
    }
    // cout << "FetchFromCentralCache end..." << endl;
    return start;
}
// 使用ThreadCache获取内存块
void *ThreadCache::Allocate(size_t size) {
    assert(size > 0);
    // cout << "Allocate begin..." << endl;
    int alignsize = SizeClass::RoundUp(size); // 获取对齐后的大小
    int index = SizeClass::Index(size);       // 获取哈希桶的索引

    // 检查索引是否超出范围
    if (index >= 208) {
        std::cerr << "Error: Index out of range in ThreadCache::Allocate"
                  << std::endl;
        return nullptr;
    }
    if (!_freeList[index].Empty()) {
        // 若freelist不为空，则直接返回freelist的头
        // cout << "Allocate end..." << endl;
        void *tmp = _freeList[index].Pop();
        // bzero(tmp, alignsize);
        return tmp;
    } else {
        // 若freelist为空，则从控制缓存中获取
        // cout << "Allocate end..." << endl;
        void *tmp = FetchFromCentralCache(index, alignsize);
        // bzero(tmp, alignsize);
        return tmp;
    }
    // cout << "Allocate end..." << endl;
}
// 释放ThreadCache的内存块
void ThreadCache::Deallocate(void *ptr, size_t size) {
    assert(ptr);

    // cout << "Deallocate begin..." << endl;
    size_t alignsize = SizeClass::RoundUp(size);
    int index = SizeClass::Index(size);

    _freeList[index].Push(ptr);

    // 当freelist的大小大于一次批量最大大小时，需要释放部分内存块到控制缓存中
    if (_freeList[index].Size() > _freeList[index].MaxSize()) {
        _freeList[index].verify(index);  // 先验证，再 ListTooLong
        ListTooLong(_freeList[index], size);
        _freeList[index].verify(index);  // 验证 ListTooLong 后的状态
    }
    // cout << "Deallocate end..." << endl;
}
// 处理freelist过长的情况, 释放部分内存块到控制缓存中
void ThreadCache::ListTooLong(FreeList &list, size_t size) {
    // cout << "ListTooLong begin..." << endl;
    void *start = nullptr;
    void *end = nullptr;

    list.PopRange(start, end, list.MaxSize());
    if (start) {
        CentralCache::GetInstance()->ReleaseRangeObj(start, size);
    }
    // cout << "ListTooLong end..." << endl;
}

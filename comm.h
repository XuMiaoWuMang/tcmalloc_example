#pragma once
#include <assert.h>
#include <iostream>
#include <mutex>
#include <sys/mman.h>
#include <time.h>
#include <unordered_map>
#include <vector>
using std::cout;
using std::endl;

#ifdef __x86_64__
typedef unsigned long long PAGE_ID;
#elif __i386__
typedef size_t PAGE_ID;
#elif _WIN64
typedef unsigned long long PAGE_ID;
#elif _WIN32
typedef size_t PAGE_ID;
#endif

static const size_t MAX_THREAD_CACHE_SIZE = 64 * 4 * 1024; // 线程缓存最大大小
static const size_t N_FREELIST = 208;                      // 线程缓存桶数量
static const size_t N_PAGE_CACHE = 257;                    // 页面缓存桶数量
static const size_t PAGE_SHIFT =
    12; // 页的大小移位，例如1页=4MB，则PAGE_SHIFT=12

// 获取下一个对象的地址（实际是读取前八个字节的内容）
// // 若为32位机器，则读取前四个字节的内容。
static void *&nextObj(void *obj) { return *(void **)obj; }
inline static void *SystemAlloc(size_t kpage) {
    // 申请4kB内存页
#if defined(__WIN32) || defined(_WIN64)
    void *ptr = VirtualAlloc(0, kpage << PAGE_SHIFT, MEM_COMMIT | MEM_RESERVE,
                             PAGE_READWRITE);
#elif defined(__x86_64__) || defined(__i386__)
    void *ptr = mmap(0, kpage << PAGE_SHIFT, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
    if (ptr == nullptr)
        throw std::bad_alloc();
    return ptr;
}
// 释放 kpage*4kB 内存页
inline static void SystemFree(void *ptr, size_t kpage) {
#if defined(__WIN32) || defined(_WIN64)
    VirtualFree(ptr, kpage << PAGE_SHIFT, MEM_RELEASE);
#elif defined(__x86_64__) || defined(__i386__)
    munmap(ptr, kpage << PAGE_SHIFT);
#endif
}
class FreeList {
  public:
    FreeList() : _head(nullptr), _size(0), _maxSize(256) {};

    // 头插：将对象插入到freelist的头
    void Push(void *obj) {
        assert(obj);
        // if((PAGE_ID)nextObj(obj) < (PAGE_ID)(1 << PAGE_SHIFT))
        // {
        //     int a = 0;
        // }

        nextObj(obj) = _head;
        _head = obj;
        _size++;
    }

    void PushRange(void *start, void *end, size_t num) {
        assert(start);
        assert(end);

        nextObj(end) = _head;
        _head = start;

        _size += num;
    }
    // 头删：返回对应对象的指针
    void *Pop() {
        assert(_head);

        void *obj = _head;
        _head = nextObj(_head);
        _size--;
        nextObj(obj) = nullptr;
        return obj;
    }

    // 批量删除num个对象，返回对应对象的指针范围，start指向第一个删除对象，end指向最后一个删除对象
    void PopRange(void *&start, void *&end, size_t num) {
        assert(_head);
        assert(num <= _size);

        start = end = _head;
        for (size_t i = 0; i < num - 1; i++) {
            end = nextObj(end);
        }

        _head = nextObj(end);
        nextObj(end) = nullptr;

        _size -= num;
    }
    // 返回freelist的大小
    size_t Size() { return _size; }
    // 判断freelist是否为空
    bool Empty() { return _head == nullptr; }

    size_t &MaxSize() { return _maxSize; }

  private:
    size_t _size = 0;
    size_t _maxSize = 256;
    void *_head = nullptr;
};

class SizeClass {
    // 整体控制在最多10%左右的内碎⽚浪费
    // [1,128]              8byte对⻬           freelist[0,16)      16个
    // [128+1,1024]         16byte对⻬          freelist[16,72)     56个
    // [1024+1,8*1024]      128byte对⻬         freelist[72,128)    56个
    // [8*1024+1,64*1024]   1024byte对⻬        freelist[128,184)   56个
    // [64*1024+1,256*1024] 8*1024byte对⻬      freelist[184,208)   24个
  public:
    // 返回不同等级对齐后的大小
    // 例如：size = 123, align = 8, 则返回 128
    static size_t _RoundUp(size_t size, size_t align) {
        return (size + align - 1) & ~(align - 1);
    }

    // 返回对齐后的大小
    static size_t RoundUp(size_t size) {
        if (size <= 128) {
            return _RoundUp(size, 8);
        } else if (size <= 1024) {
            return _RoundUp(size, 16);
        } else if (size <= 8 * 1024) {
            return _RoundUp(size, 128);
        } else if (size <= 64 * 1024) {
            return _RoundUp(size, 1024);
        } else if (size <= 256 * 1024) {
            return _RoundUp(size, 8 * 1024);
        } else {
            return _RoundUp(size, 1 << PAGE_SHIFT);
        }
    }

    // 返回任意等级的桶的下标
    static inline size_t _Index(size_t size, size_t align_shift) {
        return ((size + (1 << align_shift) - 1) >> align_shift) - 1;
    }
    // 返回桶的下标
    static inline size_t Index(size_t size) {
        assert(size);
        int index_arr[] = {16, 56, 56, 56};
        if (size <= 128) {
            return _Index(size, 3);
        } else if (size <= 1024) {
            return _Index(size - 128, 4) + index_arr[0];
        } else if (size <= 8 * 1024) {
            return _Index(size - 1024, 7) + index_arr[0] + index_arr[1];
        } else if (size <= 64 * 1024) {
            return _Index(size - 8 * 1024, 10) + index_arr[0] + index_arr[1] +
                   index_arr[2];
        } else if (size <= 256 * 1024) {
            return _Index(size - 64 * 1024, 13) + index_arr[0] + index_arr[1] +
                   index_arr[2] + index_arr[3];
        } else {
            assert(false);
            return 0;
        }
    }
    // 返回合适的批量大小
    // 例如：size = 256KB, 则返回 2
    static size_t NumMoveSize(size_t size) {
        assert(size);
        size_t num = MAX_THREAD_CACHE_SIZE / size;

        if (num < 2) {
            num = 2;
        } else if (num > 512) {
            num = 512;
        }
        return num;
    }

    // 返回合适的申请的页数(是给Thread Cache用于计算慢启动的申请页数)
    static size_t NumMovePage(size_t size) {
        // 计算批量大小
        size_t batchSize = NumMoveSize(size);
        size_t nPage = batchSize * size;
        nPage >>= PAGE_SHIFT; // 计算页数

        // 最少也要申请1页
        if (nPage == 0)
            nPage = 1;

        return nPage;
    }
};
// 带头双向链表
class Span {
  public:
    PAGE_ID _page_id = 0; // 页号
    size_t _n = 0;        // 页数量

    Span *_next = nullptr; // 双向链表结构
    Span *_prev = nullptr;

    size_t _useCount = 0; // 被ThreadCache使用的块数
    size_t _objSize = 0;  // 对象大小

    void *_freelist = nullptr; // 切好的小块内存的自由链表

    bool _isUsed = false; // 正在被使用
};

// 管理多个页
class SpanList {
  public:
    SpanList() {
        _head = new Span;

        _head->_next = _head;
        _head->_prev = _head;
    }
    Span *Begin() { return _head->_next; }
    Span *End() { return _head; }
    // 插入newSpan到span后面
    void Insert(Span *span, Span *newSpan) {
        assert(span);
        newSpan->_next = span->_next;
        newSpan->_prev = span;
        span->_next->_prev = newSpan;
        span->_next = newSpan;
    }

    // 逻辑删除指定span，不释放，因为需要保留还给Page Cache
    void Erase(Span *span) {
        assert(span);
        assert(span != _head);

        Span *next = span->_next;
        Span *prev = span->_prev;
        prev->_next = next;
        next->_prev = prev;
    }

    // 头插
    void PushFront(Span *span) { Insert(Begin(), span); }
    // 头删
    Span *PopFront() {
        assert(!Empty());
        Span *span = Begin();
        Erase(span);
        return span;
    }
    // 判断链表是否为空
    bool Empty() { return Begin() == End(); }

  public:
    std::mutex _mutex;     // 桶锁
    Span *_head = nullptr; // 带头双向链表头指针
};

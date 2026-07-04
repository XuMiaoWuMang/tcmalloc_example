#pragma once
#include "comm.h"
#include <algorithm>
#include <assert.h>
#include <iostream>
#include <time.h>
#include <vector>
using std::cout;
using std::endl;



template <class T> class ObjectPool {
  public:
    ObjectPool() = default;
    ~ObjectPool() {
        // 可选：清理 freelist 中的对象（调用析构）
        void *cur = _freelist;
        while (cur) {
            void *next = nextObj(cur);
            // 如果 T 有非平凡析构，需调用 ~T()，但此处类型未知，可忽略
            cur = next;
        }
        // 释放所有已分配的大块内存
        for (void *block : _blocks) {
            SystemFree(block, 1); // 假设每块固定大小 1<<PAGE_SHIFT
        }
        _blocks.clear();
    }
    T *New() {
        std::lock_guard<std::mutex> lock(_mutex);
        T *obj = nullptr;

        // 先从freelist中取
        if (_freelist != nullptr) {
            void *next = nextObj(_freelist);
            obj = (T *)_freelist;     // 取出freelist的头指针
            nextObj(obj) = nullptr;   // 将obj指向nullptr
            _freelist = (void *)next; // 更新freelist的头指针
        } else {

            // 走到这里有两种可能：
            // 1. 是因为第一次使用
            // 2. 是因为内存池剩余空间无法满足需求
            if (_remainSize < sizeof(T)) {
                _remainSize =
                    sizeof(T) > (1 << PAGE_SHIFT)
                        ? (sizeof(T) + (1 << PAGE_SHIFT)) & ~(1 << PAGE_SHIFT)
                        : (1 << PAGE_SHIFT);

                // 内存池剩余空间无法满足需求，需要重新申请内存
                _memory = (char *)SystemAlloc(
                    _remainSize >> PAGE_SHIFT); // 一次性申请1*4kB内存页
                _blocks.push_back(_memory);     // 记录
            }

            obj = (T *)_memory; // 取出当前内存池剩余空间的指针
            size_t objSize = sizeof(T) < sizeof(void *)
                                 ? sizeof(void *)
                                 : sizeof(T); // 取出对象的大小
            _memory += objSize;               // 更新当前内存池剩余空间的指针
            _remainSize -= objSize;           // 更新内存池剩余空间大小
        }
        // 定位new，显示调用T的构造函数
        new (obj) T;
        return obj;
    }
    void Delete(T *obj) {
        std::lock_guard<std::mutex> lock(_mutex);

        // 调用T的析构函数
        obj->~T();

        // 这个只能在32位机器上使用，因为int是4字节，如果在64位机器上就不行，int4字节无法写入8字节指针。
        // *(int*)obj = nullptr;

        // 解决办法是使用二级指针，由于指针大小和机器位数同步，32位机器上指针都是4字节的，64位机器上指针都是8字节的。
        // *(void **)obj = nullptr;

        // 两种插入方式：尾插和头插
        // 尾插：遍历freelist，找到一个结点指向的地址是nullptr;
        // void* p = nullptr;
        // for(p = _freelist; *p != nullptr; p = *(void **)p);
        // *p = *(void **)obj;
        // *(void **)obj = nullptr;

        // 头插：直接将obj插入到freelist的头
        nextObj(obj) = _freelist;
        _freelist = (void *)obj;
    }

  private:
    char *_memory = nullptr;     // 指向当前内存池剩余空间的指针
    void *_freelist = nullptr;   // 还回来的内存的指针的头指针
    size_t _remainSize = 0;      // 内存池剩余空间大小
    std::mutex _mutex;           // 互斥锁，用于保护内存池的并发访问
    std::vector<void *> _blocks; // 记录所有 SystemAlloc 的页
};
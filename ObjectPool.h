#pragma once
#include "ConcurrentAlloc.h"
#include "ThreadCache.h"
#include "comm.h"
#include <assert.h>
#include <iostream>
#include <time.h>
#include <vector>
using std::cout;
using std::endl;

#if defined(__WIN32) || defined(_WIN64)
#include <Windows.h>
#elif defined(__x86_64__) || defined(__i386__)
#include <stdlib.h>
#endif

template <class T> class ObjectPool {
  public:
    ObjectPool() = default;
    ~ObjectPool() = default;
    T *New() {
        T *obj = nullptr;

        // 先从freelist中取
        if (_freelist != nullptr) {
            void *next = *(void **)_freelist;
            obj = (T *)_freelist;     // 取出freelist的头指针
            *(void **)obj = nullptr;  // 将freelist的头指针指向nullptr
            _freelist = (void *)next; // 更新freelist的头指针
            return obj;
        }

        // 有两种可能：1. 是因为第一次使用，2. 是因为内存池剩余空间无法满足需求
        if (_remainSize < sizeof(T)) {
            // 内存池剩余空间无法满足需求，需要重新申请内存
            _memory = (char *)SystemAlloc(1); // 一次性申请1*4kB内存页
            _remainSize = 1 << 12;            // 更新内存池剩余空间大小
        }

        obj = (T *)_memory; // 取出当前内存池剩余空间的指针
        size_t objSize = sizeof(T) < sizeof(void *)
                             ? sizeof(void *)
                             : sizeof(T); // 取出对象的大小
        _memory += objSize;               // 更新当前内存池剩余空间的指针
        _remainSize -= objSize;           // 更新内存池剩余空间大小

        // 定位new，显示调用T的构造函数
        new (obj) T;
        return obj;
    }
    void Delete(T *obj) {
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
        *(void **)obj = _freelist;
        _freelist = *(void **)obj;
    }

  private:
    char *_memory = nullptr;   // 指向当前内存池剩余空间的指针
    void *_freelist = nullptr; // 还回来的内存的指针的头指针
    size_t _remainSize = 0;    // 内存池剩余空间大小
};
struct TreeNode {
    int _val;
    TreeNode *_left;
    TreeNode *_right;
    TreeNode() : _val(0), _left(nullptr), _right(nullptr) {}
};
void TestObjectPool() {
    // 申请释放的轮次
    const size_t Rounds = 3;
    // 每轮申请释放多少次
    const size_t N = 100000;
    size_t begin1 = clock();
    std::vector<TreeNode *> v1;
    v1.reserve(N);
    for (size_t j = 0; j < Rounds; ++j) {
        for (int i = 0; i < N; ++i) {
            v1.push_back(new TreeNode());
        }
        for (int i = 0; i < N; ++i) {
            delete v1[i];
        }
        v1.clear();
    }
    size_t end1 = clock();
    cout << "new cost time:" << end1 - begin1 << endl;

    size_t begin2 = clock();
    std::vector<TreeNode *> v2;
    v2.reserve(N);
    for (size_t j = 0; j < Rounds; ++j) {
        cout << "j: " << j << endl;
        for (int i = 0; i < N; ++i) {
            v2.push_back((TreeNode *)ConcurrentAlloc(sizeof(TreeNode)));
        }
        for (int i = 0; i < N; ++i) {
            cout << "i: " << i << endl;
            ConcurrentFree(v2[i], sizeof(TreeNode));
        }
        v2.clear();
    }
    size_t end2 = clock();
    cout << "object pool cost time:" << end2 - begin2 << endl;
}

// OBJECT_POOL_H

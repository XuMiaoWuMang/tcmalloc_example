#pragma once
#include "comm.h"
// 使用基数树替换哈希表，减少锁竞争，提高并发性能
// 模板参数为页号范围的二进制位数，如32位下，一页为4KB，则BITS = 32 - 12 = 20
// 所以需要2^20 * 2^2 = 2^22 字节的空间（直接分配的内存）
template <int BITS> class TCMalloc_PageMap1 {
  private:
    static const int LENGTH = 1 << BITS;
    void **_array;

  public:
    explicit TCMalloc_PageMap1() {
        // *_array = reinterpret_cast<void **>((*allocator)(sizeof(void *) <<
        // BITS));
        size_t size =
            SizeClass::_RoundUp(LENGTH * sizeof(void *), 1 << PAGE_SHIFT);
        _array = SystemAlloc(size >> PAGE_SHIFT);
    }

    void *get(PAGE_ID k) {
        if ((k >> BITS) > 0) {
            return nullptr;
        }
        return _array[k];
    }
    void set(PAGE_ID k, void *span) {
        _array[k] = span;
        return;
    }
};
// 两层的基数树
// 32位机器下，一页为4KB，则BITS = 32 - 12 = 20
// 根结点: 2^6 * 4字节 = 2 ^ 8 字节
// 叶子结点: 2^14 * 4字节 = 2 ^ 16 字节
// 毫无疑问，二层的基数树最多需要不到 2 ^ 17 字节的空间
template <int BITS> class TCMalloc_PageMap2 {
  private:
    static const int ROOT_BITS = 6;
    static const int ROOT_LENGTH = 1 << ROOT_BITS;

    static const int LEAF_BITS = BITS - ROOT_BITS;
    static const int LEAF_LENGTH = 1 << LEAF_BITS;
    struct Leaf {
        void *values[LEAF_LENGTH];
    };
    Leaf *_root[ROOT_LENGTH] = {nullptr};
    ObjectPool<Leaf> _leafPool;

  public:
    explicit TCMalloc_PageMap2() {
        // allocator_ = allocator;
        // PreallocateMoreMemory();
    }
    void *get(PAGE_ID k) {
        PAGE_ID index1 = k >> LEAF_BITS;
        PAGE_ID index2 = k & (LEAF_LENGTH - 1);
        if ((k >> BITS) > 0 || _root[index1] == nullptr) {
            return nullptr;
        }
        return _root[index1]->values[index2];
    }

    // 信任用户的输入
    void set(PAGE_ID k, void *span) {
        PAGE_ID index1 = k >> LEAF_BITS;
        PAGE_ID index2 = k & (LEAF_LENGTH - 1);
        Ensure(k, 1);
        _root[index1]->values[index2] = span;
        return;
    }

    // 确保页号范围[start, start+n)的内存页已分配
    bool Ensure(PAGE_ID start, size_t n) {
        for (PAGE_ID i = start; i < start + n;) {
            PAGE_ID index1 = i >> LEAF_BITS;
            if (index1 >= ROOT_LENGTH) {
                return false;
            }

            if (_root[index1] == nullptr) {
                // size_t size =
                //     _RoundUp(LEAF_LENGTH * sizeof(void *), 1 << PAGE_SHIFT);
                // _root[index1] = (Leaf *)SystemAlloc(size >> PAGE_SHIFT);
                // if (_root[index1] == nullptr) {
                //     return false;
                // }
                // memset(_root[index1], 0, size);
                _root[index1] = _leafPool.New();
            }

            // 由于是二级基数树，所以需要跳过当前页号的低LEAF_BITS位
            // 上一个if就是判断第二层是否需要分配内存
            i = ((i >> LEAF_BITS) + 1) << LEAF_BITS;
        }
    }

    void PreallocateMoreMemory() { Ensure(0, 1 << BITS); }
};
// 三层的基数树
// 三层基数树一般用于64位机器下，如果只是32位，可以使用二层基数树或者一层基数树即可
// 64 - 12 = 52 bit | (52 + 2) / 3 = 18 bit
// 根节点: 2^18 * 2^2 = 2^20 字节的空间
// 内部节点: 2^18 * 2^2 = 2^20 字节的空间
// 叶子节点: 2^16 * 2^2 = 2^18 字节的空间
template <int BITS> class TCMalloc_PageMap3 {
    // 均分成3份，所以需要先对齐
    static const int INTERIOR_BITS = (BITS + 2) / 3;
    // 结点索引范围: [0, INTERIOR_LENGTH)
    static const int INTERIOR_LENGTH = 1 << INTERIOR_BITS;
    // 叶子节点的bit位数
    static const int LEAF_BITS = BITS - 2 * INTERIOR_BITS;
    // 叶子节点索引范围: [0, LEAF_LENGTH)
    static const int LEAF_LENGTH = 1 << LEAF_BITS;

    // 内部节点
    struct Node {
        Node *ptrs[INTERIOR_LENGTH];
    };
    // 叶子节点
    struct Leaf {
        void *values[LEAF_LENGTH];
    };
    Node *_root;
    ObjectPool<Node> _nodePool;
    ObjectPool<Leaf> _leafPool;

    // Memory allocator
    Node *NewNode() {
        // Node *result = reinterpret_cast<Node *>((*allocator_)(sizeof(Node)));
        // if (result != NULL) {
        //     memset(result, 0, sizeof(*result));
        // }
        // return result;
        return _nodePool.New();
    }

  public:
    explicit TCMalloc_PageMap3() {
        // allocator_ = allocator;
        _root = _nodePool.New();
    }
    void *get(PAGE_ID k) {
        PAGE_ID index1 = k >> (LEAF_BITS + INTERIOR_BITS);
        PAGE_ID index2 = k >> LEAF_BITS & (INTERIOR_LENGTH - 1);
        PAGE_ID index3 = k & (LEAF_LENGTH - 1);
        if (((k >> BITS) > 0) || _root->ptrs[index1] == nullptr ||
            _root->ptrs[index1]->ptrs[index2] == nullptr) {
            return nullptr;
        }
        return reinterpret_cast<Leaf *>(_root->ptrs[index1]->ptrs[index2])
            ->values[index3];
    }

    void set(PAGE_ID k, void *span) {
        assert(k >> BITS == 0);
        PAGE_ID index1 = k >> (LEAF_BITS + INTERIOR_BITS);
        PAGE_ID index2 = k >> LEAF_BITS & (INTERIOR_LENGTH - 1);
        PAGE_ID index3 = k & (LEAF_LENGTH - 1);
        Ensure(k, 1);
        reinterpret_cast<Leaf *>(_root->ptrs[index1]->ptrs[index2])
            ->values[index3] = span;
        return;
    }

    // 确保页号范围[start, start+n)的内存页已分配
    // 第一次不需要把第三层的内存分配好，分配前两层，后续如果又用到再分配
    bool Ensure(PAGE_ID start, size_t n) {
        for (PAGE_ID i = start; i < start + n;) {
            PAGE_ID index1 = i >> (LEAF_BITS + INTERIOR_BITS);
            PAGE_ID index2 = i >> LEAF_BITS & (INTERIOR_LENGTH - 1);
            // 越界检查
            if (index1 >= INTERIOR_LENGTH) {
                return false;
            }
            // 检查第二层是否分配
            if (_root->ptrs[index1] == nullptr) {
                // size_t size =
                //     _RoundUp(INTERIOR_LENGTH * sizeof(Node *), 1 <<
                //     PAGE_SHIFT);
                // _root[index1] = (Node *)SystemAlloc(size >> PAGE_SHIFT);
                // if (_root[index1] == nullptr) {
                //     return false;
                // }
                // memset(_root[index1], 0, size);
                _root->ptrs[index1] = _nodePool.New();
            }

            // 检查第三层是否分配
            if (_root->ptrs[index1]->ptrs[index2] == nullptr) {
                // size_t size =
                //     _RoundUp(LEAF_LENGTH * sizeof(void *), 1 << PAGE_SHIFT);
                // _root[index1]->ptrs[index2] =
                //     (void *)SystemAlloc(size >> PAGE_SHIFT);
                // if (_root[index1]->ptrs[index2] == nullptr) {
                //     return false;
                // }
                // memset(_root[index1]->ptrs[index2], 0, size);
                _root->ptrs[index1]->ptrs[index2] =
                    reinterpret_cast<Node *>(_leafPool.New());
            }
            // 由于是三级基数树，所以需要跳过当前页号的低LEAF_BITS位
            // 上一个if就是判断第二层是否需要分配内存
            i = ((i >> LEAF_BITS) + 1) << LEAF_BITS;
        }
        return true;
    }
    // void PreallocateMoreMemory() { Ensure(0, 1 << BITS); }
};

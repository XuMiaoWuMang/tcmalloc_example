#include "ConcurrentAlloc.h"
#include "ObjectPool.h"
#include "comm.h"
#include <atomic>
#include <thread>
#include <unistd.h>

// static int num = 0;
void Alloc1() {
    // 申请释放的轮次
    const size_t Rounds = 30;
    // 每轮申请释放多少次
    const size_t N = 1000000;
    std::vector<TreeNode *> v1;
    v1.reserve(N);
    size_t begin1, end1;
    begin1 = clock();
    for (size_t j = 0; j < Rounds; ++j) {

        for (int i = 0; i < N; ++i) {
            v1.push_back((TreeNode *)new TreeNode());
        }

        for (int i = 0; i < N; ++i) {
            delete v1[i];
        }

        v1.clear();
    }
    end1 = clock();
    cout << "New cost time:" << end1 - begin1 << endl;
}
void Alloc2() {
    // 申请释放的轮次
    const size_t Rounds = 30;
    // 每轮申请释放多少次
    const size_t N = 1000000;

    std::vector<TreeNode *> v2;
    v2.reserve(N);
    size_t begin2;
    size_t end2;
    begin2 = clock();
    for (size_t j = 0; j < Rounds; ++j) {

        // cout << "j: " << j << endl;
        for (int i = 0; i < N; ++i) {
            v2.push_back((TreeNode *)ConcurrentAlloc(sizeof(TreeNode)));
        }

        // CentralCache::GetInstance()->debugPrint();

        for (int i = 0; i < N; ++i) {
            // cout << "i: " << i << endl;
            ConcurrentFree(v2[i]);
        }

        // CentralCache::GetInstance()->debugPrint();
        // CentralCache::GetInstance()->debugPrint();

        v2.clear();
    }
    end2 = clock();
    cout << "Concurrent cost time:" << end2 - begin2 << endl;
}
void TLStest() {
    std::thread t1(Alloc1);
    std::thread t2(Alloc2);
    t1.join();
    t2.join();
}
void testAlloc() {
    // 申请释放的轮次
    const size_t Rounds = 1;
    // 每轮申请释放多少次
    const size_t N = 10000000;
    std::vector<TreeNode *> v1;
    v1.reserve(N);
    size_t begin1, end1;
    begin1 = clock();
    for (size_t j = 0; j < Rounds; ++j) {
        for (int i = 0; i < N; ++i) {
            v1.push_back((TreeNode *)new TreeNode());
        }
        for (int i = 0; i < N; ++i) {
            delete v1[i];
        }
        v1.clear();
    }
    end1 = clock();
    cout << "new cost time:" << end1 - begin1 << endl;

    std::vector<TreeNode *> v2;
    v2.reserve(N);
    size_t begin2;
    size_t end2;
    begin2 = clock();
    for (size_t j = 0; j < Rounds; ++j) {
        // cout << "j: " << j << endl;
        for (int i = 0; i < N; ++i) {
            v2.push_back((TreeNode *)ConcurrentAlloc(sizeof(TreeNode)));
        }

        // CentralCache::GetInstance()->debugPrint();

        for (int i = 0; i < N; ++i) {
            // cout << "i: " << i << endl;
            ConcurrentFree(v2[i]);
        }

        // CentralCache::GetInstance()->debugPrint();
        // CentralCache::GetInstance()->debugPrint();

        v2.clear();
    }
    end2 = clock();

    cout << "Concurrent cost time:" << end2 - begin2 << endl;
}
void BenchmarkMalloc(size_t ntimes, size_t nworks, size_t rounds) {
    std::vector<std::thread> vthread(nworks);
    std::atomic<size_t> malloc_costtime = 0;
    std::atomic<size_t> free_costtime = 0;
    for (size_t k = 0; k < nworks; ++k) {
        vthread[k] = std::thread([&, k] {
            std::vector<void *> v;
            v.reserve(ntimes);
            for (size_t j = 0; j < rounds; ++j) {
                size_t begin1 = clock();
                for (size_t i = 0; i < ntimes; i++) {
                    // v.push_back(malloc(16));
                    v.push_back(malloc((8 + i) % 8192 + 1));
                }
                size_t end1 = clock();
                size_t begin2 = clock();
                for (size_t i = 0; i < ntimes; i++) {
                    free(v[i]);
                }
                size_t end2 = clock();
                v.clear();
                malloc_costtime += (end1 - begin1);
                free_costtime += (end2 - begin2);
            }
        });
    }
    for (auto &t : vthread) {
        t.join();
    }
    printf("%zu个线程并发执⾏%zu轮次，每轮次malloc %zu次: 花费：%zu ms\n",
           nworks, rounds, ntimes,
           malloc_costtime.load() * 1000 / CLOCKS_PER_SEC);
    printf("%zu个线程并发执⾏%zu轮次，每轮次free %zu次: 花费：%zu ms\n", nworks,
           rounds, ntimes, free_costtime.load() * 1000 / CLOCKS_PER_SEC);
    printf("%zu个线程并发malloc&free %zu次，总计花费：%zu ms\n", nworks,
           nworks * rounds * ntimes,
           (malloc_costtime.load() + free_costtime.load()) * 1000 /
               CLOCKS_PER_SEC);
}
// 单轮次申请释放次数 线程数 轮次
void BenchmarkConcurrentMalloc(size_t ntimes, size_t nworks, size_t rounds) {
    std::vector<std::thread> vthread(nworks);
    std::atomic<size_t> malloc_costtime = 0;
    std::atomic<size_t> free_costtime = 0;
    for (size_t k = 0; k < nworks; ++k) {
        vthread[k] = std::thread([&] {
            std::vector<void *> v;
            v.reserve(ntimes);
            for (size_t j = 0; j < rounds; ++j) {
                size_t begin1 = clock();
                for (size_t i = 0; i < ntimes; i++) {
                    // v.push_back(ConcurrentAlloc(16));
                    v.push_back(ConcurrentAlloc((8 + i) % 8192 + 1));
                }
                size_t end1 = clock();
                size_t begin2 = clock();
                for (size_t i = 0; i < ntimes; i++) {
                    ConcurrentFree(v[i]);
                }
                size_t end2 = clock();
                v.clear();

                malloc_costtime += (end1 - begin1);
                free_costtime += (end2 - begin2);
            }
        });
    }
    for (auto &t : vthread) {
        t.join();
    }
    printf("%zu个线程并发执⾏%zu轮次，每轮次concurrent alloc %zu次: 花费：%zu "
           "ms\n",
           nworks, rounds, ntimes,
           malloc_costtime.load() * 1000 / CLOCKS_PER_SEC);
    printf("%zu个线程并发执⾏%zu轮次，每轮次concurrent dealloc %zu次: "
           "花费：%zu ms\n",
           nworks, rounds, ntimes,
           free_costtime.load() * 1000 / CLOCKS_PER_SEC);
    printf("%zu个线程并发concurrent alloc&dealloc %zu次，总计花费：%zu ms\n",
           nworks, nworks * rounds * ntimes,
           (malloc_costtime.load() + free_costtime.load()) * 1000 /
               CLOCKS_PER_SEC);
}
int Benchmarktest() {

    size_t n = 10000;
    cout << "=========================================================="
         << endl;
    BenchmarkConcurrentMalloc(n, 10, 10);
    cout << endl << endl;
    BenchmarkMalloc(n, 10, 10);
    cout << "=========================================================="
         << endl;
    return 0;
}
void test() {
    void *p1 = ConcurrentAlloc(128 * 1024);
    void *p2 = ConcurrentAlloc(127 * 1024);
    void *p3 = ConcurrentAlloc(256 * 1024);
    void *p4 = ConcurrentAlloc(257 * 4 * 1024);

    cout << "p1: " << p1 << endl;
    ConcurrentFree(p1);
    cout << "p2: " << p2 << endl;
    ConcurrentFree(p2);
    cout << "p3: " << p3 << endl;
    ConcurrentFree(p3);
    cout << "p4: " << p4 << endl;
    ConcurrentFree(p4);

    // PageCache::GetInstance()->debugPrint();
}

int main() {
    // TLStest();
    // testAlloc();
    // TestObjectPool();
    // test();
    Benchmarktest();
    return 0;
}

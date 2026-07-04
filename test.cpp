#include "ConcurrentAlloc.h"
#include "ObjectPool.h"
#include "comm.h"
#include <atomic>
#include <thread>

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
    BenchmarkConcurrentMalloc(n, 5, 3);
    cout << endl << endl;
    BenchmarkMalloc(n, 5, 3);
    cout << "=========================================================="
         << endl;
    return 0;
}
void test() {
    void *p1 = ConcurrentAlloc(128 * 1024);
    void *p2 = ConcurrentAlloc(127 * 1024);
    void *p3 = ConcurrentAlloc(256 * 1024);
    void *p4 = ConcurrentAlloc(257 * 4 * 1024);
    void *p5 = ConcurrentAlloc(257 * 4 * 1024);
    void *p6 = ConcurrentAlloc(257 * 4 * 1024);
    void *p7 = ConcurrentAlloc(257 * 4 * 1024);
    void *p8 = ConcurrentAlloc(257 * 4 * 1024);
    void *p9 = ConcurrentAlloc(257 * 4 * 1024);
    void *p10 = ConcurrentAlloc(257 * 4 * 1024);
    void *p11 = ConcurrentAlloc(257 * 4 * 1024);
    void *p12 = ConcurrentAlloc(257 * 4 * 1024);
    void *p13 = ConcurrentAlloc(257 * 4 * 1024);

    cout << "p1: " << p1 << endl;
    ConcurrentFree(p1);
    cout << "p2: " << p2 << endl;
    ConcurrentFree(p2);
    cout << "p3: " << p3 << endl;
    ConcurrentFree(p3);
    cout << "p4: " << p4 << endl;
    ConcurrentFree(p4);
    cout << "p5: " << p5 << endl;
    ConcurrentFree(p5);
    cout << "p6: " << p6 << endl;
    ConcurrentFree(p6);
    cout << "p7: " << p7 << endl;
    ConcurrentFree(p7);
    cout << "p8: " << p8 << endl;
    ConcurrentFree(p8);
    cout << "p9: " << p9 << endl;
    ConcurrentFree(p9);
    cout << "p10: " << p10 << endl;
    ConcurrentFree(p10);
    cout << "p11: " << p11 << endl;
    ConcurrentFree(p11);
    cout << "p12: " << p12 << endl;
    ConcurrentFree(p12);
    cout << "p13: " << p13 << endl;
    ConcurrentFree(p13);
}
int main()
{
    // test();
    Benchmarktest();
}

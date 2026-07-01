#include "ConcurrentAlloc.h"
#include "ObjectPool.h"
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
    TLStest();
    // testAlloc();
    // TestObjectPool();
    // test();

    return 0;
}

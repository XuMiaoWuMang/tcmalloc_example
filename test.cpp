#include "ConcurrentAlloc.h"
#include "ObjectPool.h"
#include <unistd.h>
// static int num = 0;
void Alloc1() {
    size_t begin1 = clock();
    for (size_t i = 0; i < 100000; ++i) {
        void *ptr = ConcurrentAlloc(7);
    }
    size_t end1 = clock();
    cout << "Alloc1 done" << endl;
    cout << "Alloc1 time: " << end1 - begin1 << endl;
}
void Alloc2() {
    size_t begin2 = clock();
    for (size_t i = 0; i < 100000; ++i) {
        void *ptr = new char(6);
        // cout << "num: " << num << endl;
    }
    size_t end2 = clock();
    cout << "Alloc2 done" << endl;
    cout << "Alloc2 time: " << end2 - begin2 << endl;
}
void TLStest() {
    std::thread t1(Alloc1);
    std::thread t2(Alloc2);
    t1.join();
    t2.join();
}
void testAlloc() {
    for (size_t i = 0; i < 1000; ++i) {
        void *ptr = ConcurrentAlloc(6);
        // cout << "ptr: " << ptr << " num: " << num << endl;
    }
    cout << "testAlloc done" << endl;
}
void test() {
    void *p1 = ConcurrentAlloc(6);
    void *p2 = ConcurrentAlloc(5);
    void *p3 = ConcurrentAlloc(7);
    // void *p4 = ConcurrentAlloc(4);
    // void *p5 = ConcurrentAlloc(8);
    cout << "p1: " << p1 << endl;
    cout << "p2: " << p2 << endl;
    cout << "p3: " << p3 << endl;
    // cout << "p4: " << p4 << endl;
    // cout << "p5: " << p5 << endl;
    ConcurrentFree(p1, 6);
    ConcurrentFree(p2, 5);
    ConcurrentFree(p3, 7);
    // ConcurrentFree(p4, 4);
    // ConcurrentFree(p5, 8);
    cout << "p1: " << p1 << endl;
    cout << "p2: " << p2 << endl;
    cout << "p3: " << p3 << endl;
    // cout << "p4: " << p4 << endl;
    // cout << "p5: " << p5 << endl;
    cout << "test done" << endl;
    sleep(10);
}
int main() {
    // TLStest();
    // testAlloc();
    TestObjectPool();
    // test();

    return 0;
}

// 线程池-最终版.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include<future>

#include "threadpool.h"

using namespace std;
int sum1(int a, int b) {
    return a + b;
}
int sum2(int a, int b, int c) {
    return a + b + c;
}
int main()
{
    //packaged_task<int(int, int)> task(sum1);
    //future<int> res = task.get_future();
    //task(10, 20);

    //cout << res.get() << endl;

    Threadpool pool;
    pool.setMode(PoolMode::MODE_CACHED);
    pool.start(4);
    future<int> r1 = pool.submitTask(sum1, 1, 1000);
    future<int> r2 = pool.submitTask(sum2, 1, 1000, 1000);
    future<int> r3 = pool.submitTask(sum1, 1, 1000);
    future<int> r4 = pool.submitTask(sum1, 1, 1000);
    future<int> r5 = pool.submitTask(sum1, 1, 1000);
    cout << r1.get() << endl;
    cout << r2.get() << endl;
    cout << r3.get() << endl;
    cout << r4.get() << endl;


}
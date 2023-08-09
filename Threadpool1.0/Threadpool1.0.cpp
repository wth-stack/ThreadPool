// 线程池2022.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include<threadpool.h>
#include<chrono>
#include<thread>

using ULong = unsigned long long;

class MyTask : public Task {

public:
    MyTask(ULong begin, ULong end)
        :begin_(begin)
        , end_(end)
    {}
    Any run() {
        //  std::cout << "tid: " << std::this_thread::get_id() << "begin()" << std::endl;
        // std::this_thread::sleep_for(std::chrono::seconds(2));
        // std::cout << "tid: " << std::this_thread::get_id() << "end" << std::endl;
        ULong sum = 0;
        for (ULong i = begin_; i <= end_; i++) {
            sum += i;
        }
        std::cout << "tid: " << std::this_thread::get_id() << "end" << std::endl;
        return sum;
    }
private:
    ULong begin_;
    ULong end_;
};

int main()
{
    {
        Threadpool pool;
        pool.setMode(PoolMode::MODE_CACHED);
        pool.start(4);
        Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
        pool.submitTask(std::make_shared<MyTask>(1, 100000000));
        pool.submitTask(std::make_shared<MyTask>(1, 100000000));
        pool.submitTask(std::make_shared<MyTask>(1, 100000000));
        pool.submitTask(std::make_shared<MyTask>(1, 100000000));

        ULong sum1 = res1.getVal().cast_<ULong>();
        std::cout << sum1 << std::endl;
    }


    std::cout << "main over" << std::endl;
    getchar();
# if 0
    Threadpool pool;
    pool.setMode(PoolMode::MODE_CACHED);
    pool.start(4);
    Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
    Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
    Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
    pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
    pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
    pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
    ULong sum1 = res1.getVal().cast_<ULong>();
    ULong sum2 = res2.getVal().cast_<ULong>();
    ULong sum3 = res3.getVal().cast_<ULong>();
    std::cout << (sum1 + sum2 + sum3) << std::endl;
    //pool.submitTask(std::make_shared<MyTask>());
    //pool.submitTask(std::make_shared<MyTask>());
    //pool.submitTask(std::make_shared<MyTask>());
    //pool.submitTask(std::make_shared<MyTask>());
    //
    //pool.submitTask(std::make_shared<MyTask>());
    //pool.submitTask(std::make_shared<MyTask>());
    //pool.submitTask(std::make_shared<MyTask>());
    //pool.submitTask(std::make_shared<MyTask>());
    //pool.submitTask(std::make_shared<MyTask>());
    getchar();
#endif
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件

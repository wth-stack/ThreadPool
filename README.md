## ThreadPool
## 涉及技术
* C++、线程间通信、可变参数模板、lambda表达式
* g++编译动态库，GDB多线程调试
## 特性
* 支持任意数量参数的函数传递
* 哈希表和队列管理线程对象和任务
* 支持线程池双模式切换
## ThreadPool1.0
此目录下编译好的动态库。 需要用户继承任务基类，重写run方法。
## 编译
```cpp
git clone https://github.com/wth-stack/ThreadPool.git

cd ThreadPool/Threadpool1.0

g++ -fPIC -shared threadpool.cpp -o libryanpool.so -std=c++17
```
## 使用方法
```cpp
mv libryanpool.so /usr/local/lib/

mv threadpool.h /usr/local/include/

cd /etc/ld.so.conf.d/

vim ryanlib.conf # 在其中加入刚才动态库的路径

ldconfig # 将动态库刷新到 /etc/ld.so.cahce中

# 编译时连接动态库
g++ Threadpool1.0.cpp -std=c++17 -lryanpool -lpthread
```
## 使用示例
```cpp
using ULong = unsigned long long;

class MyTask : public Task {

public:
    MyTask(ULong begin, ULong end)
        :begin_(begin)
        , end_(end)
    {}
    Any run() {
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
}
```
## ThreadPool2.0
使用可变参数模板，支持任意数量参数的任务函数加入线程池任务队列
## 使用方法
Header only. 直接包含头文件即可
## 使用示例
```cpp
using namespace std;
int sum1(int a, int b) {
    return a + b;
}
int sum2(int a, int b, int c) {
    return a + b + c;
}
int main()
{
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
```

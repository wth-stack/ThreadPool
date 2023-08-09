#pragma once
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<vector>
#include<queue>
#include<memory>
#include<mutex>
#include<condition_variable>
#include <atomic>
#include<functional>
#include<unordered_map>

//Any类型 可以接收任意数据的类型
class Any {

public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	//这个构造函数可以让Any类型接收任意其他的数据
	template<typename T>
	Any(T data) : base_(std::make_unique<Derive<T>>(data)) {};

	//这个方法能把Any对象里面存储的data数据提取出来
	template<typename T>
	T cast_() {
		//怎么从base里面找到他指向的Derive对象，从它里面取出data成员变量
		//基类指针要转成派生类指针 RTTI
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr) {
			throw "type is unmatch!";
		}
		return pd->data_;
	}
private:
	//基类类型
	class Base {
	private:

	public:
		virtual ~Base() = default;
	};

	//派生类类型
	template<typename T>
	class Derive : public Base {
	private:
	public:
		Derive(T data) :data_(data) {};
		T data_;
	};
private:
	//定义一个基类指针
	std::unique_ptr<Base> base_;
};

//实现一个信号量类
class Semaphore {
public:
	Semaphore(int limit = 0)
		:resLimit_(limit)
		, isExit_(false) {


	}
	~Semaphore() {
		isExit_ = true;
	};

	//获得一个信号量资源
	void wait() {
		if (isExit_) {
			return;
		}
		std::unique_lock<std::mutex> lock(mtx_);
		//等待信号量有资源，没有资源的话，会阻塞当前线程
		cond_.wait(lock, [&]()-> bool {return resLimit_ > 0; });
		resLimit_--;
	};
	//增加一个信号量资源
	void post() {
		if (isExit_) {
			return;
		}
		std::unique_lock<std::mutex> lock(mtx_);
		resLimit_++;
		cond_.notify_all();
	};
private:
	std::atomic_bool isExit_;
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;
};

//Task类型的前置声明
class Task;
class Any;
//实现接收提交到线程池的task任务执行完成之后的返回值类型Result
class Result {
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	~Result() = default;

	//问题一： setval方法，获取任务执行完的返回值
	void setVal(Any any);
	//问题二： getval方法，用户调用这个方法获取task的返回值
	Any getVal();
private:
	Any any_; //存储任务的返回值
	Semaphore sem_; //线程通信的信号量
	std::shared_ptr<Task> task_; //指向对应获取返回值的任务对象
	std::atomic_bool isValid_;
};



//任务抽象基类
class Task {
public:
	Task();
	~Task() = default;
	void exec();
	void setResult(Result* result);
	//用户可以自定义任意任务类型，从Task继承，重写run方法，实现自定义任务处理
	virtual Any run() = 0;
private:
	Result* result_;
};

//线程池可支持的模式
enum class PoolMode {
	MODE_FIXED = 0, //固定数量的线程
	MODE_CACHED, //线程数量可动态增长
};

//线程类型
class Thread {
public:
	//线程函数对象类型
	using ThreadFunc = std::function<void(int)>;

	//线程构造
	Thread(ThreadFunc func);
	// 线程析构
	~Thread();
	//启动线程
	void start();

	//获取线程id
	int getId() const;
private:
	ThreadFunc func_;
	static int generateId;
	int threadId_; //保存线程id

};

//线程池类型
class Threadpool {
private:
	//std::vector<std::unique_ptr<Thread>> threads_; //线程列表
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;
	size_t initThreadSize_; //初始的线程数量
	std::atomic_int curThreadSize_;//记录当前线程池里线程的总数量
	int threadSizeThreshHode_;//线程数量上限阈值
	//记录空闲线程的数量
	std::atomic_int idleThreadSize_;

	std::queue<std::shared_ptr<Task>> taskQue_; //任务队列
	std::atomic_int taskSize_; //任务的数量
	int taskQueMaxThreshHold_; //任务队列数量上限的阈值

	std::mutex taskQueMtx_; //保证任务队列的线程安全
	std::condition_variable notFull_; //表示任务队列不满
	std::condition_variable notEmpty_;	//表示任务队列不空
	std::condition_variable exitCond_; //等待线程资源全部回收

	PoolMode poolMode_; // 当前线程池的工作模式
	//表示当前线程池的启动状态
	std::atomic_bool isPoolRunning_;



private:
	//定义线程函数
	void threadFunc(int threadid);

	//检查pool的运行状态
	bool checkRunningState() const;
public:
	//线程池构造
	Threadpool();
	//线程池析构
	~Threadpool();

	//设置task任务队列上限阈值
	void setTaskQueMaxThreshHold(int threshhold);

	//给线程池提交任务
	Result submitTask(std::shared_ptr<Task> sp);

	void start(int initThreadSize = std::thread::hardware_concurrency()); // 开启线程池
	void setMode(PoolMode poolmode); // 设置线程池的工作模式

	void setThreadSizeThreshHold(int size);

	Threadpool(const Threadpool&) = delete;
	Threadpool& operator=(const Threadpool&) = delete;
};

#endif
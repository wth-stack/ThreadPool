#pragma once
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
#include<thread>
#include<future>
#include<iostream>

const int TASK_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_THRESHHOLD = 100;
const int THREAD_MAX_IDIE_TIME = 60;



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
	Thread(ThreadFunc func)
		:func_(func)
		, threadId_(generateId_++) {

	}
	// 线程析构
	~Thread() = default;
	void start() {
		//创建一个线程来执行一个线程函数
		std::thread t(func_, threadId_);
		t.detach(); //设置分离线程
	}

	//获取线程id
	int getId() const {
		return threadId_;
	}
private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_; //保存线程id

};

int Thread::generateId_ = 0;

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


	// Task -> 函数对象
	using Task = std::function<void()>;
	std::queue<Task> taskQue_; //任务队列
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
	void threadFunc(int threadid) {
		auto lastTime = std::chrono::high_resolution_clock().now();
		for (;;) {
			//std::shared_ptr<Task> task;
			Task task;
			{
				//先获取锁
				std::unique_lock<std::mutex> lock(taskQueMtx_);
				std::cout << "tid: " << std::this_thread::get_id() << "尝试获取任务" << std::endl;
				//cache模式下，有可能已经创建了很多的线程，但是空闲时间超过60s，应该吧多余的线程回收掉(超过初始设置的线程数量)
				//每一秒钟返回一次 区分超时返回还是有任务待执行 返回
				while (taskQue_.size() == 0) {
					if (!isPoolRunning_) {
						threads_.erase(threadid);
						std::cout << "threadid:" << std::this_thread::get_id() << "exit." << std::endl;
						exitCond_.notify_all();
						return;
					}
					//条件变量 超时返回
					if (poolMode_ == PoolMode::MODE_CACHED) {
						if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1))) {
							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
							if (dur.count() >= THREAD_MAX_IDIE_TIME && curThreadSize_ > initThreadSize_) {
								//开始回收当前线程

								//记录线程数量的相关变量的值修改

								//把线程对象从线程列表容器中删除

								//threadid => thread对象 => 删除
								threads_.erase(threadid);
								curThreadSize_--;
								idleThreadSize_--;

								std::cout << "threadid:" << std::this_thread::get_id() << "exit." << std::endl;
								return;
							}
						}
					}
					else {
						//等待notEmpty条件
						notEmpty_.wait(lock);
					}
				}
				idleThreadSize_--;
				std::cout << "tid: " << std::this_thread::get_id() << "获取任务成功" << std::endl;
				//从任务队列中取一个任务出来
				task = taskQue_.front();
				taskQue_.pop();
				taskSize_--;

				//如果依然有剩余任务，继续通知其他线程执行任务
				if (taskQue_.size() > 0) {
					notEmpty_.notify_all();
				}
				//取出任务之后 进行通知
				notFull_.notify_all();

			} // 释放锁

			//当前线程负责执行这个任务
			if (task != nullptr) {
				//task->exec();
				//task->run();
				task();
			}
			lastTime = std::chrono::high_resolution_clock().now();
			idleThreadSize_++;
		}
	}

	//检查pool的运行状态
	bool checkRunningState() const {
		return isPoolRunning_;
	}
public:
	//线程池构造
	//线程池构造
	Threadpool()
		: initThreadSize_(0)
		, taskSize_(0)
		, idleThreadSize_(0)
		, curThreadSize_(0)
		, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
		, threadSizeThreshHode_(THREAD_MAX_THRESHHOLD)
		, poolMode_(PoolMode::MODE_FIXED)
		, isPoolRunning_(false)
	{}
	//线程池析构
	~Threadpool() {
		isPoolRunning_ = false;
		/*notEmpty_.notify_all();*/
		//等待线程池里面所有的线程返回， 有两种状态 阻塞 或者 正在执行任务中
		std::unique_lock<std::mutex> lock(taskQueMtx_);

		notEmpty_.notify_all();
		exitCond_.wait(lock, [&]()-> bool {return threads_.size() == 0; });
	}

	//设置task任务队列上限阈值
	void setTaskQueMaxThreshHold(int threshhold) {
		if (checkRunningState()) {
			return;
		}
		taskQueMaxThreshHold_ = threshhold;
	}

	template<typename Func, typename... Args>
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))> {
		//打包任务，放入任务队列中
		using RType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<RType()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		std::future<RType> result = task->get_future();


		//获取锁
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		//线程的通信  等待任务队列有空余
		// 用户提交任务， 最长不能阻塞超过1s，否则判断提交任务失败，返回
		if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]() ->bool {return taskQue_.size() < taskQueMaxThreshHold_; })) {
			std::cerr << "task queue is full, submit task fall." << std::endl;
			auto task = std::make_shared<std::packaged_task<RType()>>([]()->RType {return RType(); });
			(*task)();
			return task->get_future();
		}

		// 如果有空余把任务放入任务队列中
		//taskQue_.emplace(sp);
		//taskQue_.emplace([]() {(*task)();});
		taskQue_.emplace([task]() { (*task)(); });
		taskSize_++;

		//因为新放了任务 任务队列肯定不空  通知not_empty
		notEmpty_.notify_all();
		//cached模式 任务处理比较紧急 场景 小而快的任务 需要根据任务数量和空闲线程的数量 判断是否需要增加线程
		if (poolMode_ == PoolMode::MODE_CACHED && taskSize_ > idleThreadSize_ && curThreadSize_ < threadSizeThreshHode_) {

			std::cout << "create new thread" << std::endl;
			//创建新线程
			auto ptr = std::make_unique<Thread>(std::bind(&Threadpool::threadFunc, this, std::placeholders::_1));
			int threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
			//启动线程
			threads_[threadId]->start();
			//修改变量
			curThreadSize_++;
			idleThreadSize_++;
		}

		//返回任务的Result对象
		return result;
	}

	void start(int initThreadSize = std::thread::hardware_concurrency()) {
		//设置线程池的运行状态
		isPoolRunning_ = true;
		//记录初始线程个数
		initThreadSize_ = initThreadSize;
		curThreadSize_ = initThreadSize;

		//创建线程对象
		for (int i = 0; i < initThreadSize_; i++) {
			auto ptr = std::make_unique<Thread>(std::bind(&Threadpool::threadFunc, this, std::placeholders::_1));
			int threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
			//threads_.emplace_back(std::move(ptr));

		}
		//启动所有线程
		for (int i = 0; i < initThreadSize_; i++) {
			threads_[i]->start();
			idleThreadSize_++; // 记录初始空闲线程的数量
		}
	} // 开启线程池
	void setMode(PoolMode poolmode)
	{
		if (checkRunningState()) {
			return;
		}
		poolMode_ = poolmode;
	} // 设置线程池的工作模式

	void setThreadSizeThreshHold(int size) {
		if (checkRunningState()) {
			return;
		}
		if (poolMode_ == PoolMode::MODE_CACHED) {
			threadSizeThreshHode_ = size;
		}

	}

	Threadpool(const Threadpool&) = delete;
	Threadpool& operator=(const Threadpool&) = delete;
};

#endif
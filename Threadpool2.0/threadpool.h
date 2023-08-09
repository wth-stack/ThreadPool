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



//�̳߳ؿ�֧�ֵ�ģʽ
enum class PoolMode {
	MODE_FIXED = 0, //�̶��������߳�
	MODE_CACHED, //�߳������ɶ�̬����
};

//�߳�����
class Thread {
public:
	//�̺߳�����������
	using ThreadFunc = std::function<void(int)>;

	//�̹߳���
	Thread(ThreadFunc func)
		:func_(func)
		, threadId_(generateId_++) {

	}
	// �߳�����
	~Thread() = default;
	void start() {
		//����һ���߳���ִ��һ���̺߳���
		std::thread t(func_, threadId_);
		t.detach(); //���÷����߳�
	}

	//��ȡ�߳�id
	int getId() const {
		return threadId_;
	}
private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_; //�����߳�id

};

int Thread::generateId_ = 0;

//�̳߳�����
class Threadpool {
private:
	//std::vector<std::unique_ptr<Thread>> threads_; //�߳��б�
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;
	size_t initThreadSize_; //��ʼ���߳�����
	std::atomic_int curThreadSize_;//��¼��ǰ�̳߳����̵߳�������
	int threadSizeThreshHode_;//�߳�����������ֵ
	//��¼�����̵߳�����
	std::atomic_int idleThreadSize_;


	// Task -> ��������
	using Task = std::function<void()>;
	std::queue<Task> taskQue_; //�������
	std::atomic_int taskSize_; //���������
	int taskQueMaxThreshHold_; //��������������޵���ֵ

	std::mutex taskQueMtx_; //��֤������е��̰߳�ȫ
	std::condition_variable notFull_; //��ʾ������в���
	std::condition_variable notEmpty_;	//��ʾ������в���
	std::condition_variable exitCond_; //�ȴ��߳���Դȫ������

	PoolMode poolMode_; // ��ǰ�̳߳صĹ���ģʽ
	//��ʾ��ǰ�̳߳ص�����״̬
	std::atomic_bool isPoolRunning_;



private:
	//�����̺߳���
	void threadFunc(int threadid) {
		auto lastTime = std::chrono::high_resolution_clock().now();
		for (;;) {
			//std::shared_ptr<Task> task;
			Task task;
			{
				//�Ȼ�ȡ��
				std::unique_lock<std::mutex> lock(taskQueMtx_);
				std::cout << "tid: " << std::this_thread::get_id() << "���Ի�ȡ����" << std::endl;
				//cacheģʽ�£��п����Ѿ������˺ܶ���̣߳����ǿ���ʱ�䳬��60s��Ӧ�ðɶ�����̻߳��յ�(������ʼ���õ��߳�����)
				//ÿһ���ӷ���һ�� ���ֳ�ʱ���ػ����������ִ�� ����
				while (taskQue_.size() == 0) {
					if (!isPoolRunning_) {
						threads_.erase(threadid);
						std::cout << "threadid:" << std::this_thread::get_id() << "exit." << std::endl;
						exitCond_.notify_all();
						return;
					}
					//�������� ��ʱ����
					if (poolMode_ == PoolMode::MODE_CACHED) {
						if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1))) {
							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
							if (dur.count() >= THREAD_MAX_IDIE_TIME && curThreadSize_ > initThreadSize_) {
								//��ʼ���յ�ǰ�߳�

								//��¼�߳���������ر�����ֵ�޸�

								//���̶߳�����߳��б�������ɾ��

								//threadid => thread���� => ɾ��
								threads_.erase(threadid);
								curThreadSize_--;
								idleThreadSize_--;

								std::cout << "threadid:" << std::this_thread::get_id() << "exit." << std::endl;
								return;
							}
						}
					}
					else {
						//�ȴ�notEmpty����
						notEmpty_.wait(lock);
					}
				}
				idleThreadSize_--;
				std::cout << "tid: " << std::this_thread::get_id() << "��ȡ����ɹ�" << std::endl;
				//�����������ȡһ���������
				task = taskQue_.front();
				taskQue_.pop();
				taskSize_--;

				//�����Ȼ��ʣ�����񣬼���֪ͨ�����߳�ִ������
				if (taskQue_.size() > 0) {
					notEmpty_.notify_all();
				}
				//ȡ������֮�� ����֪ͨ
				notFull_.notify_all();

			} // �ͷ���

			//��ǰ�̸߳���ִ���������
			if (task != nullptr) {
				//task->exec();
				//task->run();
				task();
			}
			lastTime = std::chrono::high_resolution_clock().now();
			idleThreadSize_++;
		}
	}

	//���pool������״̬
	bool checkRunningState() const {
		return isPoolRunning_;
	}
public:
	//�̳߳ع���
	//�̳߳ع���
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
	//�̳߳�����
	~Threadpool() {
		isPoolRunning_ = false;
		/*notEmpty_.notify_all();*/
		//�ȴ��̳߳��������е��̷߳��أ� ������״̬ ���� ���� ����ִ��������
		std::unique_lock<std::mutex> lock(taskQueMtx_);

		notEmpty_.notify_all();
		exitCond_.wait(lock, [&]()-> bool {return threads_.size() == 0; });
	}

	//����task�������������ֵ
	void setTaskQueMaxThreshHold(int threshhold) {
		if (checkRunningState()) {
			return;
		}
		taskQueMaxThreshHold_ = threshhold;
	}

	template<typename Func, typename... Args>
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))> {
		//������񣬷������������
		using RType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<RType()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		std::future<RType> result = task->get_future();


		//��ȡ��
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		//�̵߳�ͨ��  �ȴ���������п���
		// �û��ύ���� �������������1s�������ж��ύ����ʧ�ܣ�����
		if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]() ->bool {return taskQue_.size() < taskQueMaxThreshHold_; })) {
			std::cerr << "task queue is full, submit task fall." << std::endl;
			auto task = std::make_shared<std::packaged_task<RType()>>([]()->RType {return RType(); });
			(*task)();
			return task->get_future();
		}

		// ����п��������������������
		//taskQue_.emplace(sp);
		//taskQue_.emplace([]() {(*task)();});
		taskQue_.emplace([task]() { (*task)(); });
		taskSize_++;

		//��Ϊ�·������� ������п϶�����  ֪ͨnot_empty
		notEmpty_.notify_all();
		//cachedģʽ ������ȽϽ��� ���� С��������� ��Ҫ�������������Ϳ����̵߳����� �ж��Ƿ���Ҫ�����߳�
		if (poolMode_ == PoolMode::MODE_CACHED && taskSize_ > idleThreadSize_ && curThreadSize_ < threadSizeThreshHode_) {

			std::cout << "create new thread" << std::endl;
			//�������߳�
			auto ptr = std::make_unique<Thread>(std::bind(&Threadpool::threadFunc, this, std::placeholders::_1));
			int threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
			//�����߳�
			threads_[threadId]->start();
			//�޸ı���
			curThreadSize_++;
			idleThreadSize_++;
		}

		//���������Result����
		return result;
	}

	void start(int initThreadSize = std::thread::hardware_concurrency()) {
		//�����̳߳ص�����״̬
		isPoolRunning_ = true;
		//��¼��ʼ�̸߳���
		initThreadSize_ = initThreadSize;
		curThreadSize_ = initThreadSize;

		//�����̶߳���
		for (int i = 0; i < initThreadSize_; i++) {
			auto ptr = std::make_unique<Thread>(std::bind(&Threadpool::threadFunc, this, std::placeholders::_1));
			int threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
			//threads_.emplace_back(std::move(ptr));

		}
		//���������߳�
		for (int i = 0; i < initThreadSize_; i++) {
			threads_[i]->start();
			idleThreadSize_++; // ��¼��ʼ�����̵߳�����
		}
	} // �����̳߳�
	void setMode(PoolMode poolmode)
	{
		if (checkRunningState()) {
			return;
		}
		poolMode_ = poolmode;
	} // �����̳߳صĹ���ģʽ

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
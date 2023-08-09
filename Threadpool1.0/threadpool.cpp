
#include<threadpool.h>

#include<functional>
#include<thread>
#include<iostream>

const int TASK_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_THRESHHOLD = 100;
const int THREAD_MAX_IDIE_TIME = 60;

//�̳߳ع���
Threadpool::Threadpool()
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
Threadpool::~Threadpool() {
	isPoolRunning_ = false;
	/*notEmpty_.notify_all();*/
	//�ȴ��̳߳��������е��̷߳��أ� ������״̬ ���� ���� ����ִ��������
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	notEmpty_.notify_all();
	exitCond_.wait(lock, [&]()-> bool {return threads_.size() == 0; });
}




//����task�������������ֵ
void Threadpool::setTaskQueMaxThreshHold(int threshhold) {
	if (checkRunningState()) {
		return;
	}
	taskQueMaxThreshHold_ = threshhold;
}

//���̳߳��ύ����  �û����øýӿڣ��������������������
Result Threadpool::submitTask(std::shared_ptr<Task> sp) {
	//��ȡ��
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	//�̵߳�ͨ��  �ȴ���������п���
	// �û��ύ���� �������������1s�������ж��ύ����ʧ�ܣ�����
	/*
	while (taskQue_.size() == taskQueMaxThreshHold_) {
		notFull_.wait(lock);
	}
	*/
	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]() ->bool {return taskQue_.size() < taskQueMaxThreshHold_; })) {
		std::cerr << "task queue is full, submit task fall." << std::endl;
		return Result(sp, false);
	}

	// ����п��������������������
	taskQue_.emplace(sp);
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
	return Result(sp);
}

// �����̳߳�
void Threadpool::start(int initThreadSize) {

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
}

//�����̺߳���   �̳߳ص������̴߳��������������������
void Threadpool::threadFunc(int threadid) {

	auto lastTime = std::chrono::high_resolution_clock().now();
	for (;;) {
		std::shared_ptr<Task> task;
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
				//�̳߳�Ҫ���� �����߳���Դ
				//if (!isPoolRunning_) {
				//	threads_.erase(threadid);
				//	std::cout << "threadid:" << std::this_thread::get_id() << "exit." << std::endl;
				//	exitCond_.notify_all();
				//	return;
				//}
			}
			//if (!isPoolRunning_) {
			//	break;
			//}
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
			task->exec();
			//task->run();
		}
		lastTime = std::chrono::high_resolution_clock().now();
		idleThreadSize_++;
	}

}

//���pool������״̬
bool Threadpool::checkRunningState() const {
	return isPoolRunning_;
}

void Threadpool::setMode(PoolMode poolmode)
{
	if (checkRunningState()) {
		return;
	}
	poolMode_ = poolmode;
} // �����̳߳صĹ���ģʽ

void Threadpool::setThreadSizeThreshHold(int size) {
	if (checkRunningState()) {
		return;
	}
	if (poolMode_ == PoolMode::MODE_CACHED) {
		threadSizeThreshHode_ = size;
	}

}

///////////////////�̷߳���ʵ��///////////////////

int Thread::generateId = 0;

//��ȡ�߳�id
int Thread::getId() const {
	return threadId_;
}

void Thread::start() {
	//����һ���߳���ִ��һ���̺߳���
	std::thread t(func_, threadId_);
	t.detach(); //���÷����߳�
}

//�̹߳���
Thread::Thread(ThreadFunc func)
	:func_(func)
	, threadId_(generateId++) {

}
// �߳�����
Thread::~Thread() {

}


/////////////////////////////////////// Result����ʵ��
Result::Result(std::shared_ptr<Task> task, bool isValid)
	:task_(task)
	, isValid_(isValid)
{
	task->setResult(this);
};

void Result::setVal(Any any) {
	this->any_ = std::move(any);
	sem_.post();
}

Any Result::getVal() {
	if (!isValid_) {
		return "";
	}
	sem_.wait();
	return std::move(any_);
}



/////////////////////////////////////// Task����ʵ��
void Task::exec() {
	if (result_ != nullptr) {
		result_->setVal(run());
	}
}


void Task::setResult(Result* result) {
	result_ = result;

}

Task::Task()
	:result_(nullptr)
{}
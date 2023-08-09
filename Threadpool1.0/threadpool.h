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

//Any���� ���Խ����������ݵ�����
class Any {

public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	//������캯��������Any���ͽ�����������������
	template<typename T>
	Any(T data) : base_(std::make_unique<Derive<T>>(data)) {};

	//��������ܰ�Any��������洢��data������ȡ����
	template<typename T>
	T cast_() {
		//��ô��base�����ҵ���ָ���Derive���󣬴�������ȡ��data��Ա����
		//����ָ��Ҫת��������ָ�� RTTI
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr) {
			throw "type is unmatch!";
		}
		return pd->data_;
	}
private:
	//��������
	class Base {
	private:

	public:
		virtual ~Base() = default;
	};

	//����������
	template<typename T>
	class Derive : public Base {
	private:
	public:
		Derive(T data) :data_(data) {};
		T data_;
	};
private:
	//����һ������ָ��
	std::unique_ptr<Base> base_;
};

//ʵ��һ���ź�����
class Semaphore {
public:
	Semaphore(int limit = 0)
		:resLimit_(limit)
		, isExit_(false) {


	}
	~Semaphore() {
		isExit_ = true;
	};

	//���һ���ź�����Դ
	void wait() {
		if (isExit_) {
			return;
		}
		std::unique_lock<std::mutex> lock(mtx_);
		//�ȴ��ź�������Դ��û����Դ�Ļ�����������ǰ�߳�
		cond_.wait(lock, [&]()-> bool {return resLimit_ > 0; });
		resLimit_--;
	};
	//����һ���ź�����Դ
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

//Task���͵�ǰ������
class Task;
class Any;
//ʵ�ֽ����ύ���̳߳ص�task����ִ�����֮��ķ���ֵ����Result
class Result {
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	~Result() = default;

	//����һ�� setval��������ȡ����ִ����ķ���ֵ
	void setVal(Any any);
	//������� getval�������û��������������ȡtask�ķ���ֵ
	Any getVal();
private:
	Any any_; //�洢����ķ���ֵ
	Semaphore sem_; //�߳�ͨ�ŵ��ź���
	std::shared_ptr<Task> task_; //ָ���Ӧ��ȡ����ֵ���������
	std::atomic_bool isValid_;
};



//����������
class Task {
public:
	Task();
	~Task() = default;
	void exec();
	void setResult(Result* result);
	//�û������Զ��������������ͣ���Task�̳У���дrun������ʵ���Զ���������
	virtual Any run() = 0;
private:
	Result* result_;
};

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
	Thread(ThreadFunc func);
	// �߳�����
	~Thread();
	//�����߳�
	void start();

	//��ȡ�߳�id
	int getId() const;
private:
	ThreadFunc func_;
	static int generateId;
	int threadId_; //�����߳�id

};

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

	std::queue<std::shared_ptr<Task>> taskQue_; //�������
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
	void threadFunc(int threadid);

	//���pool������״̬
	bool checkRunningState() const;
public:
	//�̳߳ع���
	Threadpool();
	//�̳߳�����
	~Threadpool();

	//����task�������������ֵ
	void setTaskQueMaxThreshHold(int threshhold);

	//���̳߳��ύ����
	Result submitTask(std::shared_ptr<Task> sp);

	void start(int initThreadSize = std::thread::hardware_concurrency()); // �����̳߳�
	void setMode(PoolMode poolmode); // �����̳߳صĹ���ģʽ

	void setThreadSizeThreshHold(int size);

	Threadpool(const Threadpool&) = delete;
	Threadpool& operator=(const Threadpool&) = delete;
};

#endif
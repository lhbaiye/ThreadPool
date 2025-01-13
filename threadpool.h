#pragma once
#include <iostream>
#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <map>
#include <thread>

class Any
{
public:
    Any() = default;
    ~Any() = default;
    Any(const Any &other) = delete;
    Any &operator=(const Any &other) = delete;
    Any(Any &&other) = default;
    Any &operator=(Any &&other) = default;

    template <typename T>
    Any(T data) : base_(std::make_unique<Derived<T>>(data)) {}

    // 把any对象里面的数据转换成T类型
    template <typename T>
    T cast_()
    {
        // 有base指针
        if (base_ == nullptr)
        {
            throw std::bad_cast();
        }
        auto derived = dynamic_cast<Derived<T> *>(base_.get());
        if (derived == nullptr)
        {
            throw std::bad_cast();
        }
        return derived->data_;
    }

private:
    // 基类类型
    class Base
    {
    public:
        virtual ~Base() = default;
    };
    // 派生类类型
    template <typename T>
    class Derived : public Base
    {
    public:
        Derived(T data) : data_(data) {}
        T data_;
    };

private:
    std::unique_ptr<Base> base_;
};

// 信号量类
class Semaphore
{
public:
    Semaphore(int resLimit = 0) : resLimit_(resLimit), isExit_(false) {}
    ~Semaphore()
    {
        isExit_ = true;
    }
    // 获取一个信号量资源
    void wait()
    {
        if (isExit_)
        {
            return;
        }
        std::unique_lock<std::mutex> lock(mtx_);
        // 等待信号量有资源，没有的话阻塞
        cond_.wait(lock, [this]()
                   { return resLimit_ > 0; });
        resLimit_--;
    }
    // 增加一个信号量资源
    void post()
    {
        if (isExit_)
        {
            return;
        }
        std::unique_lock<std::mutex> lock(mtx_);
        resLimit_++;
        cond_.notify_all();
    }

private:
    std::atomic_bool isExit_;
    std::mutex mtx_;
    std::condition_variable cond_;
    int resLimit_;
};

class Task;

// 实现接收提交到线程池的task执行完成后的返回值类型
class Result
{
public:
    Result(std::shared_ptr<Task> task, bool isValid = true);
    ~Result() = default;
    Result(const Result &other) = delete;
    Result &operator=(const Result &other) = delete;
    Result(Result &&other) = default;
    Result &operator=(Result &&other) = default;

    // setVal方法，设置
    void setVal(Any any);

    Any get();

private:
    Any any_;                    // 用于存储任意类型的返回值
    Semaphore sem_;              // 信号量，用于控制是否有返回值
    std::shared_ptr<Task> task_; // 任务指针
    std::atomic_bool isValid_;   // 返回值是否有效
};

// 线程类
class Thread
{
public:
    using ThreadFunc = std::function<void(int)>;
    Thread(ThreadFunc func);
    ~Thread();
    void start();

    int getThreadId() const;

private:
    ThreadFunc func_;
    static int generateId_;
    int threadId; // 保存线程ID
};

// 线程池模式
enum class PoolMode
{
    MODE_FIXED,
    MODE_CACHED,
};

// 任务基类
class Task
{
public:
    Task();
    virtual ~Task() = default;
    virtual Any run() = 0;
    void exec();

    void setResult(Result *result);

private:
    Result *result_;
};

// 线程池

class ThreadPool
{
public:
    ThreadPool();
    ~ThreadPool();

    void start(int initThreadSize = std::thread::hardware_concurrency());

    void setMode(PoolMode mode);

    void setMaxThreadSize(size_t maxThreadSize);

    void setTaskQueueMaxThreshHold(int taskQueueMaxThreshHold);

    Result submitTask(std::shared_ptr<Task> taskPtr);

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

private:
    void threadFunc(int threadId);

    // 检查pool的运行状态
    bool checkRunningState() const;

private:
    // std::vector<std::unique_ptr<Thread>> threads_;
    std::unordered_map<int, std::unique_ptr<Thread>> threads_;
    size_t initThreadSize_;           // 初始线程数量
    size_t maxThreadSize_;            // 线程数量上限阈值
    std::atomic_uint idleThreadSize_; // 记录空闲线程数量
    std::atomic_uint curThreadSize_;  // 当前线程池中的线程数量

    std::queue<std::shared_ptr<Task>> taskQueue_; // 任务队列
    std::atomic_uint taskSize_;                   // 任务数量
    int taskQueueMaxThreshHold_;                  // 任务队列最大阈值

    std::mutex taskQueueMtx_;          // 任务队列锁
    std::condition_variable notFull_;  // 任务队列满的条件变量
    std::condition_variable notEmpty_; // 任务队列空的条件变量

    PoolMode poolMode_; // 当前线程池状态
    // 表示当前线程池的启动状态
    std::atomic_bool isPoolRunning_;

    std::condition_variable exitCond_; // 等待线程资源全部回收
};

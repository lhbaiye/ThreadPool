#include <functional>
#include <thread>
#include "threadpool.h"

const int TASK_QUEUE_MAX_THRESH_HOLD = 1000000;
const int THREAD_MAX_SIZE = 10;
const int THREAD_MAX_IDLE_TIME = 60;

int Thread::generateId_ = 0;

int Thread::getThreadId() const
{
    return threadId;
}

ThreadPool::ThreadPool()
    : initThreadSize_(0),
      taskSize_(0),
      taskQueueMaxThreshHold_(TASK_QUEUE_MAX_THRESH_HOLD),
      poolMode_(PoolMode::MODE_FIXED),
      isPoolRunning_(false),
      idleThreadSize_(0),
      curThreadSize_(0),
      maxThreadSize_(THREAD_MAX_SIZE) {};

// 线程池析构
ThreadPool::~ThreadPool()
{
    isPoolRunning_ = false;
    // 等待线程池里面所有线程返回 有两种状态，一种是阻塞，因为没有任务，另外一种是正在执行任务中
    std::unique_lock<std::mutex> lock(taskQueueMtx_);
    notEmpty_.notify_all();
    exitCond_.wait(lock, [this]() -> bool
                   { return threads_.size() == 0; });
}

void ThreadPool::setMode(PoolMode mode)
{
    if (checkRunningState())
    {
        std::cerr << "thread pool is running, can't set mode" << std::endl;
        return;
    }
    poolMode_ = mode;
}

void ThreadPool::setTaskQueueMaxThreshHold(int threshHold)
{
    if (checkRunningState())
    {
        std::cerr << "thread pool is running, can't set mode" << std::endl;
        return;
    }
    taskQueueMaxThreshHold_ = threshHold;
}

bool ThreadPool::checkRunningState() const
{
    return isPoolRunning_;
}

void ThreadPool::setMaxThreadSize(size_t maxThreadSize)
{
    if (checkRunningState())
    {
        std::cerr << "thread pool is running, can't set max thread size" << std::endl;
        return;
    }
    if (poolMode_ == PoolMode::MODE_CACHED)
    {
        maxThreadSize_ = maxThreadSize;
    }
}

// 提交任务
Result ThreadPool::submitTask(std::shared_ptr<Task> taskPtr)
{
    // 获取锁
    std::unique_lock<std::mutex> lock(taskQueueMtx_);

    // 线程通信 等待任务队列有空余
    if (!notFull_.wait_for(lock, std::chrono::seconds(1),
                           [&]() -> bool
                           { return taskQueue_.size() < (size_t)(taskQueueMaxThreshHold_); }))
    {
        // 如果等待超时，直接返回
        std::cerr << "task queue is full, task is dropped" << std::endl;
        return Result(taskPtr, false);
    }

    // 如果有空余，把任务放入任务队列中
    taskQueue_.emplace(taskPtr);
    taskSize_++;

    // 因为新任务进来了，所以任务数量加1，在notEmpty_条件变量中通知线程
    notEmpty_.notify_all();

    // 任务处理比较紧急，场景：小而快的任务 需要根据任务数量和空闲线程数量，来决定是否创建新线程
    if (poolMode_ == PoolMode::MODE_CACHED && taskSize_ > idleThreadSize_ && curThreadSize_ < maxThreadSize_)
    {
        std::cout << "submitTask 创建新线程" << std::endl;
        // 创建新线程
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        int threadId = ptr->getThreadId();
        threads_.emplace(threadId, std::move(ptr));
        threads_[threadId]->start();
        // 修改线程数量
        curThreadSize_++;
        idleThreadSize_++;
    }
    return Result(taskPtr);
}

// 开始线程池
void ThreadPool::start(int initThreadSize)
{
    isPoolRunning_ = true;
    // 记录初始线程数量
    initThreadSize_ = initThreadSize;
    curThreadSize_ = initThreadSize;

    // 创建线程对象, 把线程函数给到线程对象
    for (size_t i = 0; i < initThreadSize_; ++i)
    {
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        int threadId = ptr->getThreadId();
        threads_.emplace(threadId, std::move(ptr));
    }

    // 启动所有线程
    for (auto &thread : threads_)
    {
        thread.second->start(); // 执行一个线程函数
        idleThreadSize_++;      // 记录空闲线程数量
    }
}

void ThreadPool::threadFunc(int threadId)
{
    auto lastTime = std::chrono::high_resolution_clock().now();
    while (true)
    {
        std::shared_ptr<Task> task;
        {
            // 先获取锁
            std::unique_lock<std::mutex> lock(taskQueueMtx_);

            std::cout << "threadFunc " << std::this_thread::get_id() << " 尝试获取任务" << std::endl;

            // 超过initThreadSize_的线程数量需要回收
            // 当前时间-上次任务执行时间 > 60s
            while (taskQueue_.size() == 0)
            {
                if (!isPoolRunning_)
                {
                    // 回收线程
                    threads_.erase(threadId);

                    std::cout << "threadFunc " << std::this_thread::get_id() << " 回收线程" << std::endl;
                    exitCond_.notify_all();
                    return;
                }
                if (poolMode_ == PoolMode::MODE_CACHED)
                {
                    // 条件变量超时返回
                    if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
                    {
                        auto now = std::chrono::high_resolution_clock().now();
                        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastTime).count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_)
                        {
                            // 回收线程
                            threads_.erase(threadId);
                            curThreadSize_--;
                            idleThreadSize_--;
                            std::cout << "threadFunc " << std::this_thread::get_id() << " 回收线程" << std::endl;
                            return;
                        }
                    }
                }
                else
                {
                    // 等待任务队列不为空
                    notEmpty_.wait(lock);
                }
                // 这里不需要判断了，因为外面while循环已经判断了，然后会进入到break，
                // if (!isPoolRunning_)
                // {
                //     // 回收线程
                //     threads_.erase(threadId);
                //     std::cout << "threadFunc " << std::this_thread::get_id() << " 回收线程" << std::endl;
                //     exitCond_.notify_all();
                //     return;
                // }
            }

            idleThreadSize_--;

            std::cout << "threadFunc " << std::this_thread::get_id() << " 获取到任务" << std::endl;

            // 取一个出来
            task = taskQueue_.front();
            taskQueue_.pop();
            taskSize_--;

            // 如果依然有剩余任务，通知其他线程
            if (taskQueue_.size() > 0)
            {
                notEmpty_.notify_all();
            }

            // 取出完一个任务，得进行通知
            notFull_.notify_all();

        } // 把锁释放掉

        // 当前线程负责执行这个任务
        if (task != nullptr)
        {
            task->exec();
            std::cout << "threadFunc " << std::this_thread::get_id() << " 执行完任务" << std::endl;
        }
        lastTime = std::chrono::high_resolution_clock().now(); // 更新线程执行完的时间
        idleThreadSize_++;
    }
}

/// @brief 线程函数

Thread::Thread(ThreadFunc func)
    : func_(func),
      threadId(generateId_++)
{
}

Thread::~Thread()
{
}

void Thread::start()
{
    // 创建一个线程来执行线程函数
    std::thread t(func_, threadId); // 对于C++11的标准，这里的func_是一个函数对象，可以直接作为线程函数
    t.detach();                     // 分离线程
}

Result::Result(std::shared_ptr<Task> taskPtr, bool isValid) : task_(taskPtr), isValid_(isValid)
{
    task_->setResult(this);
};

Any Result::get()
{
    // 如果返回值无效，直接返回
    if (!isValid_)
    {
        return "";
    }

    // 等待任务执行完毕
    sem_.wait();

    return std::move(any_);
};

void Result::setVal(Any any)
{
    // 存储task的返回值
    this->any_ = std::move(any);
    // 通知get方法，返回值已经设置完毕
    sem_.post();
};

Task::Task() : result_(nullptr) {
               };

void Task::exec()
{
    // 执行任务
    // 通知返回值
    if (result_ != nullptr)
    {
        result_->setVal(run());
    }
};

void Task::setResult(Result *result)
{
    result_ = result;
};

#include <iostream>
#include <thread>
#include "threadpool.h"

using uLong = unsigned long;

// 一个简单的具体 Task 类实现
class SimpleTask : public Task
{
public:
    SimpleTask(uLong begin, uLong end) : begin_(begin), end_(end) {}
    Any run() override
    {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        std::cout << "SimpleTask::run() begin = " << begin_ << ", end = " << end_ << std::endl;
        uLong sum = 0;
        for (uLong i = begin_; i <= end_; i++)
        {
            sum += i;
        }
        return sum;
        
    }

private:
    uLong begin_;
    uLong end_;
};

int main()
{
    {
        ThreadPool pool;
        pool.setMode(PoolMode::MODE_CACHED);
        pool.start(4);
        Result res1 = pool.submitTask(std::make_shared<SimpleTask>(1, 100000000));
        Result res2 = pool.submitTask(std::make_shared<SimpleTask>(100000001, 200000000));
        Result res3 = pool.submitTask(std::make_shared<SimpleTask>(200000001, 300000000));
        Result res4 = pool.submitTask(std::make_shared<SimpleTask>(300000001, 400000000));
        Result res5 = pool.submitTask(std::make_shared<SimpleTask>(400000001, 500000000));
        Result res6 = pool.submitTask(std::make_shared<SimpleTask>(500000001, 600000000));
        Result res7 = pool.submitTask(std::make_shared<SimpleTask>(600000001, 700000000));
        uLong r1 = res1.get().cast_<uLong>();
        uLong r2 = res2.get().cast_<uLong>();
        uLong r3 = res3.get().cast_<uLong>();
        uLong r4 = res4.get().cast_<uLong>();
        uLong r5 = res5.get().cast_<uLong>();
        uLong r6 = res6.get().cast_<uLong>();
        uLong r7 = res7.get().cast_<uLong>();

        std::cout << r1 + r2 + r3 + r4 + r5 + r6 + r7 << std::endl;
    }

    getchar();
    return 0;
}
#include <vector>
#include "FixedCirdularQueue.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

template<typename TArg, typename TReturn, size_t VSize>
class OrderedTaskQueue {
public:
    using WorkerFn = std::function<void(const TArg&, TReturn&)>;
    using PushFn = std::function<void(TArg*)>;
    using ResultReadFn = std::function<void(const TReturn&)>;

    OrderedTaskQueue(WorkerFn fn,
                     std::size_t threadCount = std::thread::hardware_concurrency())
        : workerFn(std::move(fn))
    {
        if (threadCount == 0) threadCount = 1;
        workers.reserve(threadCount);
        for (std::size_t i = 0; i < threadCount; ++i)
            workers.emplace_back([this]{ workerLoop(); });
    }

    ~OrderedTaskQueue() {
        finish();
        for (auto &t : workers)
            if (t.joinable()) t.join();
    }

    void push(const TArg& value)
    {
        tasks.pushNextWaitSpace([&value](auto innerVal){
            innerVal.arg = value;
        }, running);
    }

    void pushPtr(const PushFn& pushFn)
    {
        tasks.pushNextWaitSpace([&pushFn](auto innerVal){
            pushFn(&innerVal->arg);
        }, running);
    }

    void terminate() {
        {
            std::lock_guard<std::mutex> lock(taskMutex);
            running = false;
        }
        tasks.notifyWaiters();
        resultCv.notify_all();
    }

    /**
     * Returns true if value was given to callback, false if queue is terminated
     * Pointer to the value becomes invalid after callback is called
    */
    bool getNext(const ResultReadFn& resRead) {
        std::unique_lock<std::mutex> lock = tasks.lock();
        resultCv.wait(lock, [this]{
            std::lock_guard l{taskMutex};
            return tasks.atUnlocked(0).state == State::DONE || !running.load();
        });

        if(tasks.atUnlocked(0).state == State::DONE)
        {
            std::lock_guard<std::mutex> l{taskMutex};
            resRead(tasks.atUnlocked(0).value);
            tasks.popOldestUnlocked();
            return true;
        }
        else
        {
            return false;
        }
    }

private:
    enum class State
    {
        READY,
        WORKING,
        DONE,
        RETRIEVED
    };

    struct Task {
        std::size_t id;
        TArg arg;
        TReturn value;
        std::atomic<State> state;
    };

    FixedCircularQueue<Task, VSize> tasks;

    WorkerFn workerFn;

    std::vector<std::thread> workers;

    std::mutex taskMutex;
    std::condition_variable taskCv;
    std::condition_variable resultCv;

    std::atomic<bool> running = true;


    void workerLoop() {
        while(true)
        {
            Task* task = nullptr;
            {
                auto valMutex = tasks.waitNext(running);
                if(!running.load())
                {
                    return;
                }
                {
                    std::lock_guard<std::mutex> l{taskMutex};
                    for(size_t i=0; i<tasks.sizeUnlocked(); ++i)
                    {
                        if(tasks.atUnlocked(i).state.load() == State::READY)
                        {
                            task = &tasks.atUnlocked(i);
                            task->state.store(State::WORKING);
                            break;
                        }
                    }
                }
            }

            {
                workerFn(task->arg, task->value);
            }

            {
                std::lock_guard<std::mutex> l{taskMutex};
                task->state.store(State::DONE);
                tasks.notifyWaiters();
                resultCv.notify_all();
            }
        }
    }
};

#pragma once
#include <queue>
#include <utility>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <cassert>
#include <optional>
#include <list>

namespace nioev::lib {

enum class GenServerEnqueueResult {
    Success,
    Failed
};
/* A class that represents a similar concept to that of a GenServer in elixir - that's where the name comes frome. You put a request and it while get
 * handled by a second worker thread. This is a pattern that's used quite a lot and is very useful.
 */
template<typename TaskType>
class GenServer {
public:
    struct DelayedTaskType {
        std::chrono::steady_clock::time_point when;
        TaskType task;
        bool operator<(const DelayedTaskType& o) const {
            return when > o.when;
        }
    };
    explicit GenServer(std::string threadName)
    : mThreadName(std::move(threadName)) { }

    virtual ~GenServer() {
        stopThread();
    }
    [[nodiscard]] virtual GenServerEnqueueResult enqueue(TaskType&& task) {
        std::unique_lock<std::recursive_mutex> lock{mTasksMutex};
        if(!allowEnqueue(task)) {
            return GenServerEnqueueResult::Failed;
        }
        mTasks.emplace_back(std::move(task));
        mTasksCV.notify_all();
        return GenServerEnqueueResult::Success;
    }
    [[nodiscard]] virtual GenServerEnqueueResult enqueueDelayed(TaskType&& task, std::chrono::milliseconds delay) {
        auto when = std::chrono::steady_clock::now() + delay;
        std::unique_lock<std::recursive_mutex> lock{mTasksMutex};
        if(!allowEnqueue(task)) {
            return GenServerEnqueueResult::Failed;
        }
        mDelayedTasks.emplace(DelayedTaskType{when, std::move(task)});
        mTasksCV.notify_all();
        return GenServerEnqueueResult::Success;
    }

    template<typename Filter>
    void filterDelayedTasks(const Filter& filter) {
        std::unique_lock<std::recursive_mutex> lock{mTasksMutex};
        decltype(mDelayedTasks) newDelayedTasks;
        while(!mDelayedTasks.empty()) {
            if(filter(mDelayedTasks.top().task)) {
                newDelayedTasks.emplace(mDelayedTasks.top());
                mDelayedTasks.pop();
            }
        }
    }

protected:
    virtual bool allowEnqueue(const TaskType& task) {
        return true;
    }
    void startThread() {
        std::unique_lock<std::recursive_mutex> lock{ mTasksMutex };
        mWorkerThread.template emplace([this]{workerThreadFunc();});
        mShouldRun = true;
    }
    void stopThread() {
        std::unique_lock<std::recursive_mutex> lock{ mTasksMutex };
        mShouldRun = false;
        mTasksCV.notify_all();
        lock.unlock();
        if(!mWorkerThread)
            return;
        mWorkerThread->join();
        mWorkerThread.reset();
    }
    virtual void handleTask(TaskType&&) = 0;
    virtual void handleTaskHoldingLock(std::unique_lock<std::recursive_mutex> &lock, TaskType&& task) {
        lock.unlock();
        handleTask(std::move(task));
        lock.lock();
    }
    virtual const std::list<TaskType>& getTasks() const {
        // LOCK MUST BE HELD HERE
        return mTasks;
    }
    virtual void workerThreadEnter() {}
    virtual void workerThreadLeave() {}
private:
    void workerThreadFunc() {
        pthread_setname_np(pthread_self(), mThreadName.c_str());
        std::unique_lock<std::recursive_mutex> lock{mTasksMutex};
        workerThreadEnter();
        while(true) {
            if(mDelayedTasks.empty() && mTasks.empty()) {
                mTasksCV.wait(lock);
            } else if(!mDelayedTasks.empty() && mTasks.empty()) {
                mTasksCV.wait_until(lock, mDelayedTasks.top().when);
            }
            if(!mShouldRun) {
                workerThreadLeave();
                return;
            }
            while(!mTasks.empty()) {
                auto pub = std::move(mTasks.front());
                mTasks.erase(mTasks.begin());
                handleTaskHoldingLock(lock, std::move(pub));
            }
            if(!mShouldRun) {
                workerThreadLeave();
                return;
            }
            while(!mDelayedTasks.empty() && mDelayedTasks.top().when <= std::chrono::steady_clock::now()) {
                auto pub = std::move(mDelayedTasks.top().task);
                mDelayedTasks.pop();
                handleTaskHoldingLock(lock, std::move(pub));
            }
            if(!mShouldRun) {
                workerThreadLeave();
                return;
            }
        }
    }
    bool mShouldRun{true};
    std::recursive_mutex mTasksMutex;
    std::condition_variable_any mTasksCV;
    std::list<TaskType> mTasks;
    std::priority_queue<DelayedTaskType> mDelayedTasks;
    std::string mThreadName;
    std::optional<std::thread> mWorkerThread;
};

}
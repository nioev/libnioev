#pragma once
#include <queue>
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
    explicit GenServer(const char* threadName)
    : mThreadName(threadName) { }

    virtual ~GenServer() {
        std::unique_lock<std::mutex> lock{ mTasksMutex };
        mShouldRun = false;
        mTasksCV.notify_all();
        lock.unlock();
        mWorkerThread->join();
    }
    [[nodiscard]] virtual GenServerEnqueueResult enqueue(TaskType&& task) {
        assert(mWorkerThread);
        std::unique_lock<std::mutex> lock{mTasksMutex};
        if(!allowEnqueue(task)) {
            return GenServerEnqueueResult::Failed;
        }
        mTasks.emplace_back(std::move(task));
        mTasksCV.notify_all();
        return GenServerEnqueueResult::Success;
    }

protected:
    virtual bool allowEnqueue(const TaskType& task) {
        return true;
    }
    void startThread() {
        mWorkerThread.template emplace([this]{workerThreadFunc();});
    }
    virtual void handleTask(TaskType&&) = 0;
    virtual void handleTaskHoldingLock(std::unique_lock<std::mutex> &lock, TaskType&& task) {
        lock.unlock();
        handleTask(std::move(task));
        lock.lock();
    }
    virtual const std::list<TaskType>& getTasks() const {
        // LOCK MUST BE HELD HERE
        return mTasks;
    }
private:
    void workerThreadFunc() {
        pthread_setname_np(pthread_self(), mThreadName);
        std::unique_lock<std::mutex> lock{mTasksMutex};
        while(true) {
            mTasksCV.wait(lock);
            if(!mShouldRun)
                return;
            while(!mTasks.empty()) {
                auto pub = std::move(mTasks.front());
                mTasks.erase(mTasks.begin());
                handleTaskHoldingLock(lock, std::move(pub));
            }

        }
    }
    bool mShouldRun{true};
    std::mutex mTasksMutex;
    std::condition_variable mTasksCV;
    std::list<TaskType> mTasks;
    const char* mThreadName;
    std::optional<std::thread> mWorkerThread;
};

}
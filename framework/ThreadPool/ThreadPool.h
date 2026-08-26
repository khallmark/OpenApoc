#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "framework/logger.h"
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <typeinfo>
#include <vector>

class ThreadPool
{
  public:
	ThreadPool(size_t);
	void enqueue(std::function<void()> task);
	~ThreadPool();

  private:
	// need to keep track of threads so we can join them
	std::vector<std::thread> workers;
	// the task queue
	std::queue<std::function<void()>> tasks;

	// synchronization
	std::mutex queue_mutex;
	std::condition_variable condition;
	bool stop;
};

// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threads) : stop(false)
{
	// Having a zero-sized threadpool really doesn't make sense
	LogAssert(threads > 0);
	for (size_t i = 0; i < threads; ++i)
		workers.emplace_back(
		    [this]
		    {
			    for (;;)
			    {
				    std::function<void()> task;

				    {
					    std::unique_lock<std::mutex> lock(this->queue_mutex);
					    this->condition.wait(lock,
					                         [this] { return this->stop || !this->tasks.empty(); });
					    if (this->stop && this->tasks.empty())
						    return;
					    task = std::move(this->tasks.front());
					    this->tasks.pop();
				    }

				    try
				    {
					    task();
				    }
				    catch (std::exception &e)
				    {
					    // Report the TYPE as well as what(). A worker exception is the
					    // hardest failure in this engine to diagnose: the stack is gone, the
					    // stage it belonged to has usually moved on, and what() alone is
					    // frequently a single word. Three separate battle-generation faults
					    // this session presented as "vector", "map" and "mutex lock failed:
					    // Invalid argument", and each cost hours mostly spent working out
					    // which subsystem had even thrown.
					    LogError("Exception occurred in threadpool: {0}: {1}", typeid(e).name(),
					             e.what());
				    }
				    catch (...)
				    {
					    // A non-std::exception used to vanish here without a trace, so a
					    // worker could die in total silence and the only symptom was a stage
					    // that never transitioned.
					    LogError("Unknown (non-std::exception) thrown in threadpool - task "
					             "abandoned");
				    }
			    }
		    });
}

// add new work item to the pool
void ThreadPool::enqueue(std::function<void()> task)
{
	{
		std::unique_lock<std::mutex> lock(queue_mutex);

		// don't allow enqueueing after stopping the pool
		if (stop)
			throw std::runtime_error("enqueue on stopped ThreadPool");

		tasks.emplace(task);
	}
	condition.notify_one();
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool()
{
	{
		std::unique_lock<std::mutex> lock(queue_mutex);
		stop = true;
	}
	condition.notify_all();
	for (std::thread &worker : workers)
		worker.join();
}

#endif

/* Adapted by ZE.
 *
 * Copyright (c) 2012 Jakob Progsch, Václav Zeman
 * From: https://github.com/progschj/ThreadPool
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 *    1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 *
 *    2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 *    3. This notice may not be removed or altered from any source
 *    distribution.
 */

#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <glog/logging.h>

namespace ze {

class ThreadPool
{
public:
  ThreadPool(size_t);
  ~ThreadPool();

  //! Add task to threadpool. See for example usage in unit-test.
  template<class F, class... Args>
  auto enqueue(F&& f, Args&&... args)
  -> std::future<typename std::result_of<F(Args...)>::type>;

private:
  // need to keep track of threads so we can join them
  std::vector< std::thread > workers_;

  // the task queue
  std::queue< std::function<void()> > tasks_;

  // synchronization
  std::mutex queue_mutex_;
  std::condition_variable condition_;
  bool stop_;
};
 
// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threads)
  : stop_(false)
{
  for(size_t i = 0;i<threads;++i)
  {
    workers_.emplace_back(
      [this] {

        // Thread loop:
        while(true)
        {
          std::function<void()> task;

          // Wait for next task.
          {
            std::unique_lock<std::mutex> lock(this->queue_mutex_);
            this->condition_.wait(lock, [this]{ return this->stop_ || !this->tasks_.empty(); });
            if (this->stop_ && this->tasks_.empty())
            {
              return;
            }
            task = std::move(this->tasks_.front());
            this->tasks_.pop();
          }

          // Execute task.
          task();
        }
      }
    );
  }
}

// add new work item to the pool
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
  -> std::future<typename std::result_of<F(Args...)>::type>
{
  using return_type = typename std::result_of<F(Args...)>::type;

  auto task = std::make_shared<std::packaged_task<return_type()>>(
    std::bind(std::forward<F>(f), std::forward<Args>(args)...)
  );

  std::future<return_type> res = task->get_future();
  {
    std::unique_lock<std::mutex> lock(queue_mutex_);

    // don't allow enqueueing after stopping the pool
    if (stop_)
    {
      LOG(FATAL) << "Enqueue on stopped ThreadPool";
    }

    tasks_.emplace([task](){ (*task)(); });
  }
  condition_.notify_one();
  return res;
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool()
{
  {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    stop_ = true;
  }
  condition_.notify_all();
  for (std::thread& worker : workers_)
  {
    worker.join();
  }
}

} // namespace ze

/*
 * credit: WhozCraig on Stackoverflow
 * http://stackoverflow.com/questions/23896421/efficiently-waiting-for-all-m_tasks-in-a-threadpool-to-finish
 *
 * DOESN'T WORK :'(
 */

#include <deque>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <iostream>
#include <vector>

namespace matan {
  class ThreadPool {
  public:
    ThreadPool(const unsigned int n = std::thread::hardware_concurrency());
    ~ThreadPool();
    int numThreads() const { return m_workers.size(); };
    template<class F, class... Args> void enqueue(F &&f, Args&&... args);
    void waitFinished();

  private:
    std::vector<std::thread> m_workers;
    std::deque<std::function<void()>> m_tasks;
    std::mutex m_queueMutex;
    std::condition_variable m_cvTask;
    std::condition_variable m_cvFinished;
    int m_busy;
    bool stop;

    void threadProc();
  };

  ThreadPool::ThreadPool(unsigned int n) :
          m_busy(0), stop(false) {
    for (unsigned int i = 0; i < n; ++i) {
      m_workers.emplace_back([this](){this->threadProc();});
    }
  }

  ThreadPool::~ThreadPool() {
    std::unique_lock<std::mutex> lock(m_queueMutex);
    stop = true;
    m_cvTask.notify_all();
    lock.unlock();

    for (auto &t : m_workers) {
      t.join();
    }
  }

  template<class F, class... Args>
  void ThreadPool::enqueue(F&& f, Args&&... args) {
    std::unique_lock<std::mutex> lock(m_queueMutex);
    auto func = [&f, &args...](){ f(args...); };
    m_tasks.emplace_back(func);
    m_cvTask.notify_one();
  }

  void ThreadPool::waitFinished() {
    std::unique_lock<std::mutex> lock(m_queueMutex);
    m_cvFinished.wait(lock,
                      [this]() {
                        return m_tasks.empty() && (m_busy == 0);
                      });
    m_busy = 0;
  }

  void ThreadPool::threadProc() {
    while (true) {
      std::unique_lock<std::mutex> lock(m_queueMutex);
      m_cvTask.wait(lock, [this]() { return stop || !m_tasks.empty(); });
      if (!m_tasks.empty()) {
        ++m_busy;
        auto fn = m_tasks.front();
        m_tasks.pop_front();
        lock.unlock();

        //run the function without blocking the other threads
        fn();

        lock.lock();
        /*
         * Need to lock this section so that don't make 'final' call of
         * m_cvFinished.notify_one(), while another one is previously running.
         * This will cause the notify to go unnoticed, beacause we won't be
         * waiting at this point, and then will never again be notified.
         */
        --m_busy;
        m_cvFinished.notify_one();
        lock.unlock();
      } else if (stop) {
        break;
      }
    }
  }

} //namespace matan

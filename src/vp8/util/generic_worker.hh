#include <atomic>
#include <functional>
#include <thread>
#include "options.hh"

struct GenericWorker {
    std::atomic<int> new_work_exists_;
    std::atomic<int> work_done_;
    std::function<void()> work;
    
    GenericWorker() : new_work_exists_(0),
                      work_done_(0),
                      child_(std::bind(&GenericWorker::wait_for_work,
                                           this)) {
    }
    void activate_work() {
        ++new_work_exists_;
    }
    bool is_done() {
        if (work_done_.load(std::memory_order_relaxed) > 0) {
            return work_done_.load() != 0; // enforce memory ordering
        }
        return false;
    }
    void main_wait_for_done();
    void wait_for_work();
private:
    std::thread child_;
};

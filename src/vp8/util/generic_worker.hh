#include <atomic>
#include <functional>
#include <thread>
#include "nd_array.hh"
#include "options.hh"

struct GenericWorker {
    std::atomic<int> new_work_exists_;
    std::atomic<int> work_done_;
    std::function<void()> work;
    Sirikata::Array1d<int, 2> new_work_pipe;
    Sirikata::Array1d<int, 2> work_done_pipe;
    static Sirikata::Array1d<int, 2> initiate_pipe();
    GenericWorker() : new_work_exists_(0),
                      work_done_(0),
                      new_work_pipe(initiate_pipe()),
                      work_done_pipe(initiate_pipe()),
                      child_(std::bind(&GenericWorker::wait_for_work,
                                           this)) {
    }
    void activate_work();
    bool is_done();
    void main_wait_for_done();
    void wait_for_work();
private:
    std::thread child_; // this must come after other members, so items are initialized first
};

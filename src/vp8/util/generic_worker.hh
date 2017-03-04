#include <atomic>
#include <functional>
#include <thread>
#include "nd_array.hh"
#include "options.hh"
struct GenericWorker {
    bool child_begun;
    std::atomic<int> new_work_exists_;
    std::atomic<int> work_done_;
    std::function<void()> work;
    Sirikata::Array1d<int, 2> new_work_pipe;
    Sirikata::Array1d<int, 2> work_done_pipe;
    static Sirikata::Array1d<int, 2> initiate_pipe();
    void activate_work();
    bool is_done();
    void main_wait_for_done();
    void wait_for_work();
    bool has_ever_queued_work() {
        return new_work_exists_.load() != 0;
    }
    void join_via_syscall();
    int send_more_data(const void *data_ptr);
    std::pair<const void*, int> recv_data();
    struct DataBatch {
        typedef Sirikata::Array1d<void *, 15> DataType;
        DataType data;
        int32_t return_code;
        uint8_t count;
    };
    DataBatch batch_recv_data();
    void wait_for_child_to_begin();
    static GenericWorker *get_n_worker_threads(unsigned int num_workers);
private:
    GenericWorker(); // not safe since it doesn't wait for seccomp, use public constructor
    std::thread child_; // this must come after other members, so items are initialized first
    void _generic_wait(uint8_t expected_response);
    void _generic_respond_to_main(uint8_t arg);
};

#include <atomic>
#include <functional>
#include <thread>
#include "nd_array.hh"
#include "options.hh"
#ifdef USE_STANDARD_MEMORY_ALLOCATORS
#include <mutex>
class xatomic {
  int data;
  mutable std::mutex mut;
public:
  xatomic() {
    std::lock_guard<std::mutex> lok(mut);
    data = 0;
  }
  xatomic(int i) {
    std::lock_guard<std::mutex> lok(mut);
    data = i;
  }
  int load()const {
    std::lock_guard<std::mutex> lok(mut);
    return data;
  }
  template<class Sub> int load(Sub s)const {
    std::lock_guard<std::mutex> lok(mut);
    return data;
  }
  template<class Sub>void store(int dat, Sub s){
    std::lock_guard<std::mutex> lok(mut);
    data = dat;
  }
  void store(int dat){
    std::lock_guard<std::mutex> lok(mut);
    data = dat;
  }
  int operator +=(int i) {
    std::lock_guard<std::mutex> lok(mut);
    data += i;
    return data;
  }
  int operator ++() {
    std::lock_guard<std::mutex> lok(mut);
    data += 1;
    return data;
  }
  int operator ++(int ignored) {
    std::lock_guard<std::mutex> lok(mut);
    data += 1;
    return data - 1;
  }
  int operator -=(int i) {
    std::lock_guard<std::mutex> lok(mut);
    data -= i;
    return data;
  }
  int operator --() {
    std::lock_guard<std::mutex> lok(mut);
    data -= 1;
    return data;
  }
  int operator --(int ignored) {
    std::lock_guard<std::mutex> lok(mut);
    data -= 1;
    return data + 1;
  }
};

#else
typedef std::atomic<int> xatomic;
#endif
int make_pipe(int pipes[2]);
struct GenericWorker {
    bool child_begun;
    xatomic new_work_exists_;
    xatomic work_done_;
    std::function<void()> work;
    Sirikata::Array1d<int, 2> new_work_pipe;
    Sirikata::Array1d<int, 2> work_done_pipe;
    static Sirikata::Array1d<int, 2> initiate_pipe();
    void activate_work();
    bool is_done();
    void main_wait_for_done();
    void instruct_to_exit();
    void wait_for_work();
    bool has_ever_queued_work() {
        return new_work_exists_.load() != 0;
    }
    void join_via_syscall();
    int send_more_data(const void *data_ptr);
    std::pair<const void*, int> recv_data();
    struct DataBatch {
        enum {
            DATA_BATCH_SIZE = 15
        };
        typedef Sirikata::Array1d<void *, DATA_BATCH_SIZE> DataType;
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

#include <emmintrin.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include "generic_worker.hh"
const bool use_pipes = true;
void GenericWorker::wait_for_work() {
    char data = 0;
    if (use_pipes) {
        while (read(new_work_pipe[0], &data, 1) < 0 && errno == EINTR) {
        }
    }
    while(!new_work_exists_.load(std::memory_order_relaxed)) {
        _mm_pause();
    }
    if (new_work_exists_.load()) { // enforce memory ordering
        work();
    }else {
        assert(false);// invariant violated
    }
    work_done_++;
    data = 1;
    if (use_pipes) {
        while (write(work_done_pipe[1], &data, 1) < 0 && errno == EINTR) {
        
        }
    }
}

bool GenericWorker::is_done() {
        if (work_done_.load(std::memory_order_relaxed) > 0) {
            return work_done_.load() != 0; // enforce memory ordering
        }
        return false;
    }

void GenericWorker::activate_work() {
    ++new_work_exists_;
    char data = 0;
    while (write(new_work_pipe[1], &data, 1) < 0 && errno == EINTR) {
        
    }
}
Sirikata::Array1d<int, 2> GenericWorker::initiate_pipe(){
    int pipes[2] = {-1, -1};
    if (use_pipes) {
        while (pipe(pipes) != 0 && errno == EINTR){
        }
    }
    Sirikata::Array1d<int, 2> retval;
    retval.at(0) = pipes[0];
    retval.at(1) = pipes[1];
    return retval;
}
void GenericWorker::main_wait_for_done() {
    assert(new_work_exists_.load()); // make sure this has work to do
    if (use_pipes) {
        char data = 0;
        while (read(work_done_pipe[0], &data, 1) < 0 && errno == EINTR) {
        }
    }
    
    while(!is_done()) {
        _mm_pause();
    }
    work_done_.load();  // enforce memory ordering
}

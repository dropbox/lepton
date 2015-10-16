#include "memory.hh"
#include <emmintrin.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include "generic_worker.hh"
const bool use_pipes = true;
void GenericWorker::_generic_respond_to_main(uint8_t arg) {
    work_done_++;
    if (use_pipes) {
        while (write(work_done_pipe[1], &arg, 1) < 0 && errno == EINTR) {
        }
    }
}

void GenericWorker::wait_for_work() {
    //SETUP SECCOMP
    _generic_respond_to_main(0); // startup
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
    _generic_respond_to_main(1);
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
void GenericWorker::_generic_wait(uint8_t expected_arg) {
    if (use_pipes) {
        char data = 0;
        while (read(work_done_pipe[0], &data, 1) < 0 && errno == EINTR) {
        }
        if (data != expected_arg) {
            char err[] = "x worker protocol error";
            err[0] = '0' + expected_arg;
            while (write(2, err, strlen(err)) <0 && errno == EINTR) {

            }
            exit(5); //protocol error;
        }
    }
    
    while(!is_done()) {
        _mm_pause();
    }
    work_done_.load();  // enforce memory ordering
}
void GenericWorker::_wait_for_child_to_begin() {
    assert(!child_begun); // make sure this has work to do
    _generic_wait(0);
    --work_done_;
    child_begun = true;
}

void GenericWorker::main_wait_for_done() {
    assert(new_work_exists_.load()); // make sure this has work to do
    _generic_wait(1);
}

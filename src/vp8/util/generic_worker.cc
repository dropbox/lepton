#include <emmintrin.h>
#include <assert.h>
#include "generic_worker.hh"

void GenericWorker::wait_for_work() {
    while(!new_work_exists_.load(std::memory_order_relaxed)) {
        _mm_pause();
    }
    if (new_work_exists_.load()) { // enforce memory ordering
        work();
    }else {
        assert(false);// invariant violated
    }
    work_done_++;
}

void GenericWorker::main_wait_for_done() {
    assert(new_work_exists_.load()); // make sure this has work to do
    
    while(!is_done()) {
        _mm_pause();
    }
    work_done_.load();  // enforce memory ordering
}

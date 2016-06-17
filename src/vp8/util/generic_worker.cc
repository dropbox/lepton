#include "memory.hh"
#include <emmintrin.h>
#include <assert.h>
#ifdef _WIN32
#include <io.h>
#include <Windows.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif
#include <errno.h>
#ifdef __linux
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

#endif
#include <signal.h>
#include "generic_worker.hh"
#include "../../io/Seccomp.hh"

const bool use_pipes = true;
void GenericWorker::_generic_respond_to_main(uint8_t arg) {
    work_done_++;
    if (use_pipes) {
        while (write(work_done_pipe[1], &arg, 1) < 0 && errno == EINTR) {
        }
    }
}

void GenericWorker::wait_for_work() {
    bool sandbox_at_desired_level = true;
    if (g_use_seccomp) {
        Sirikata::installStrictSyscallFilter(true);
    }
    _generic_respond_to_main(0); // startup
    char data = 0;
    if (use_pipes) {
        int err = 0;
        while ((err = read(new_work_pipe[0], &data, 1)) < 0 && errno == EINTR) {
        }
        if (err <= 0) {
            set_close_thread_handle(work_done_pipe[1]);
            custom_terminate_this_thread(0);
            return;
        }
    }
    set_close_thread_handle(work_done_pipe[1]);
    while(!new_work_exists_.load(std::memory_order_relaxed)) {
        _mm_pause();
    }
    if (new_work_exists_.load()) { // enforce memory ordering
        if (sandbox_at_desired_level) {
            work();
        }
    }else {
        always_assert(false);// invariant violated
    }
    _generic_respond_to_main(sandbox_at_desired_level ? 1 : 2);
    reset_close_thread_handle();
    custom_terminate_this_thread(0); // cleanly exit the thread with an allowed syscall
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
#ifdef _WIN32
int make_pipe(int pipes[2]) {
    HANDLE read_pipe, write_pipe;
    if (CreatePipe(&read_pipe, &write_pipe, NULL, 65536)) {
        pipes[0] = _open_osfhandle((intptr_t)read_pipe, O_RDONLY);
        pipes[1] = _open_osfhandle((intptr_t)write_pipe, O_WRONLY);
        return 0;
    }
    errno = EINVAL;
    return -1;
}
#else
int make_pipe(int pipes[2]) {
    return pipe(pipes);
}
#endif
Sirikata::Array1d<int, 2> GenericWorker::initiate_pipe(){
    int pipes[2] = {-1, -1};
    if (use_pipes) {
        while (make_pipe(pipes) != 0 && errno == EINTR){
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
            char err[] = "x: Worker thread out of memory.\n";
            err[0] = '0' + expected_arg;
            while (write(2, err, strlen(err)) <0 && errno == EINTR) {

            }
            custom_exit(ExitCode::THREAD_PROTOCOL_ERROR);
        }
    }
    
    while(!is_done()) {
        _mm_pause();
    }
    work_done_.load();  // enforce memory ordering
}
void GenericWorker::_wait_for_child_to_begin() {
    always_assert(!child_begun); // make sure this has work to do
    _generic_wait(0);
    --work_done_;
    child_begun = true;
}
void GenericWorker::join_via_syscall() {
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif
    while (close(work_done_pipe.at(0)) && errno == EINTR) {
    }
    child_.join();
}
void GenericWorker::main_wait_for_done() {
    always_assert(new_work_exists_.load()); // make sure this has work to do
    _generic_wait(1);
}

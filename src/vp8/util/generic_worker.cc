#include "memory.hh"

#ifdef __aarch64__
#define USE_SCALAR 1
#endif

#ifndef USE_SCALAR
#include <emmintrin.h>
#endif

#include <assert.h>
#ifdef _WIN32
#include <io.h>
#include <Windows.h>
#include <fcntl.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif
#include <errno.h>
#ifdef __linux__
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

#endif
#include <signal.h>
#include "generic_worker.hh"
#include "../../io/Seccomp.hh"
/**
 * A Crossplatform-ish pause function.
 * Since we can't rely on the _mm_pause instrinsic being available
 * all the time we define a pause function that uses it if available and
 * falls back to platform specific sleep(0) otherwise
 */
void _cross_platform_pause() {
#if !defined(USE_SCALAR) && defined(__i386__)
        _mm_pause();
#else
#ifdef _WIN32 
        Sleep(0);
#else
        usleep(0);
#endif
#endif
}
static void gen_nop(){}
void kill_workers(void * workers, uint64_t num_workers) {
    GenericWorker * generic_workers = (GenericWorker*)workers;
    if (generic_workers) {
        for (uint64_t i = 0; i < num_workers; ++i){
            if (!generic_workers[i].has_ever_queued_work()){
                generic_workers[i].work = &gen_nop;
                generic_workers[i].activate_work();
            }
            generic_workers[i].instruct_to_exit();
        }
    }
}

GenericWorker * GenericWorker::get_n_worker_threads(unsigned int num_workers) {
    GenericWorker *retval = new GenericWorker[num_workers];
    //for (unsigned int i = 0;i < num_workers; ++i) {
    //    retval[i].wait_for_child_to_begin(); // setup security
    //}
    custom_atexit(&kill_workers, retval, num_workers);
    return retval;
}
namespace {
void sta_wait_for_work(void * gw) {
    GenericWorker * thus = (GenericWorker*)gw;
    thus->wait_for_work();
}
}

GenericWorker::GenericWorker() : child_begun(false),
                                new_work_exists_(0),
                                work_done_(0),
                                new_work_pipe(initiate_pipe()),
                                work_done_pipe(initiate_pipe()),
                                child_(std::bind(&sta_wait_for_work,
                                                 this)) {
  wait_for_child_to_begin(); // setup security
}
const bool use_pipes = true;
void GenericWorker::_generic_respond_to_main(uint8_t arg) {
    work_done_++;
    if (use_pipes) {
        while (write(work_done_pipe[1], &arg, 1) < 0 && errno == EINTR) {
            _cross_platform_pause();
        }
    }
}

#define THREAD_PACKET_SIZE (sizeof(void*) + 1)

void GenericWorker::wait_for_work() {
    if (g_use_seccomp) {
        Sirikata::installStrictSyscallFilter(true);
    }
    _generic_respond_to_main(0); // startup
    while(true) {
        if (use_pipes) {
            char new_work_buffer[THREAD_PACKET_SIZE] = {63};
            size_t amount_read = 0;
            while (amount_read < THREAD_PACKET_SIZE) {
                auto err = read(new_work_pipe[0], &new_work_buffer[amount_read], THREAD_PACKET_SIZE - amount_read);
                if (err <0 && errno == EINTR) {
                        continue;
                }
                if (err <= 0) {
                    set_close_thread_handle(work_done_pipe[1]);
                    custom_terminate_this_thread(0);
                    break;
                }
                amount_read += err;
            }
            if (new_work_buffer[0] == 127) {
                //fprintf(stderr,"BRAKE\n");
                break;
            }
            if (new_work_buffer[0] == 63) {
                // ignore excess work
                continue;
            }
        }
        set_close_thread_handle(work_done_pipe[1]);
        while(!new_work_exists_.load()) {
            
        }
        if (new_work_exists_.load()) { // enforce memory ordering
            work();
        }else {
            always_assert(false && "variable never decrements");
        }
        _generic_respond_to_main(1);
    }
    reset_close_thread_handle();
    custom_terminate_this_thread(0); // cleanly exit the thread with an allowed syscall
}
bool GenericWorker::is_done() {
        return work_done_.load() != 0; // enforce memory ordering
    }

void GenericWorker::activate_work() {
    ++new_work_exists_;
    char data[THREAD_PACKET_SIZE] = {0};
    size_t data_sent = 0;
    while (data_sent != THREAD_PACKET_SIZE) {
        auto ret = write(new_work_pipe[1], &data[data_sent], THREAD_PACKET_SIZE - data_sent);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            return; // nothing we can do here.
        }
        if (ret == 0) {
            return; // closed the pipe!?
        }
        data_sent += ret;
    }
}
void GenericWorker::instruct_to_exit() {
    ++new_work_exists_;
    char data[THREAD_PACKET_SIZE] = {0};
    memset(data, 127, THREAD_PACKET_SIZE);
    size_t data_sent = 0;
    while (data_sent != THREAD_PACKET_SIZE) {
        auto ret = write(new_work_pipe[1], &data[data_sent], THREAD_PACKET_SIZE - data_sent);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            return; // nothing we can do here.
        }
        if (ret == 0) {
            return; // closed the pipe!?
        }
        data_sent += ret;
    }
}
int GenericWorker::send_more_data(const void *data_ptr) {
    ++new_work_exists_;
    unsigned char buffer[THREAD_PACKET_SIZE];
    buffer[0] = 63;
    memcpy(&buffer[1], &data_ptr, sizeof(data_ptr));
    size_t size = THREAD_PACKET_SIZE;
    const unsigned char*ptr = &buffer[0];
    do {
        ssize_t ret = write(new_work_pipe[1], ptr, size);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }else {
                return ret;
            }
        }
        size -= ret;
        ptr += ret;
    }while(size > 0);
    return 0;
}

std::pair<const void*, int> GenericWorker::recv_data() {
    std::pair<const void*, int> retval = {NULL, -1};
    unsigned char buffer[THREAD_PACKET_SIZE];
    uint8_t *ptr = &buffer[0];
    size_t size = THREAD_PACKET_SIZE;
    do {
        ssize_t ret = read(new_work_pipe[0], ptr, size);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }else {
                retval.second = ret;
                return retval;
            }
        }
        size -= ret;
        ptr += ret;
    }while(size > 0);
    if (buffer[0] == 127) {
        reset_close_thread_handle();
        custom_terminate_this_thread(0); // cleanly exit the thread with an allowed syscall
        return retval;
    }
    if (buffer[0] != 63) {
        return retval;
    }
    memcpy(&retval.first, &buffer[1], sizeof(retval.first));
    auto val = new_work_exists_.load(); // lets allow our thread to see what retval.first points to
    always_assert(val != 0);
    retval.second = 0;
    return retval;
}

bool copy_to_data_batch(GenericWorker::DataBatch *output, unsigned char buffer[GenericWorker::DataBatch::DATA_BATCH_SIZE]) {
    size_t source_loc = 0;
    for (size_t i = 0;i< output->count; ++i) {
        if (buffer[source_loc] == 127) {
            reset_close_thread_handle();
            custom_terminate_this_thread(0); // cleanly exit the thread with an allowed syscall
        }
        if (buffer[source_loc] != 63) {
            output->count = i;
            return false;
        }
        static_assert(THREAD_PACKET_SIZE == sizeof(void*) + 1,
                      "We must send 1 byte of ddata over thread socket and size of pointer");
        memcpy(&output->data[i], &buffer[source_loc + 1], sizeof(void *));
        source_loc += THREAD_PACKET_SIZE;
    }
    return true;
}
GenericWorker::DataBatch GenericWorker::batch_recv_data() {
    DataBatch retval;
    unsigned char buffer[DataBatch::DATA_BATCH_SIZE * THREAD_PACKET_SIZE] = {0};
    retval.count = 0;
    retval.return_code = 0;
    uint8_t *ptr = &buffer[0];
    size_t size = sizeof(buffer);
    size_t amt_read = 0;
    //fprintf(stderr, "Start read %ld\n", size);
    do {
        ssize_t ret = read(new_work_pipe[0], ptr, size);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }else {
                retval.return_code = ret;
                copy_to_data_batch(&retval, buffer);
                return retval;
            }
        }
        size -= ret;
        ptr += ret;
        amt_read += ret;
        retval.count = amt_read / THREAD_PACKET_SIZE;
    }while(amt_read % THREAD_PACKET_SIZE);
    //fprintf(stderr, "End read %ld : %d\n", amt_read, retval.count);
    if (!copy_to_data_batch(&retval, buffer)) {
        retval.return_code = -1;
    }
    auto val = new_work_exists_.load(); // lets allow our thread to see what retval.first points to
    always_assert(val != 0);
    return retval;
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
        _cross_platform_pause();
    }
    work_done_.load();  // enforce memory ordering
}
void GenericWorker::wait_for_child_to_begin() {
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
    always_assert(child_begun);
    always_assert(new_work_exists_.load()); // make sure this has work to do
    _generic_wait(1);
//    instruct_to_exit();
}

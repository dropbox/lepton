#include "../vp8/util/memory.hh"
#ifdef _WIN32
#include <io.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif
#include <signal.h>
#include "../vp8/util/nd_array.hh"
#include "../io/MuxReader.hh"
#include "../io/ioutil.hh"
#include "validation.hh"
#include "generic_compress.hh"

ValidationContinuation validateAndCompress(int *reader,
                                           int *writer,
                                           Sirikata::Array1d<uint8_t, 2> header,
                                           size_t header_size,
                                           size_t start_byte,
                                           size_t end_byte,
                                           ExitCode *validation_exit_code,
                                           Sirikata::MuxReader::ResizableByteBuffer *lepton_data,
                                           int argc,
                                           const char ** argv,
                                           bool is_permissive,
                                           bool is_socket,
                                           std::vector<uint8_t> *permissive_jpeg_return) {
    if (is_permissive){
        always_assert(permissive_jpeg_return);
        if (header_size < header.size()) {
            permissive_jpeg_return->resize(header_size);
            if (header_size) {
                memcpy(permissive_jpeg_return->data(), header.data, header_size);
            }
            return ValidationContinuation::EVALUATE_AS_PERMISSIVE;
        }
    }
#ifdef _WIN32
    std::vector<const char*> args;
    args.push_back(argv[0]);
    args.push_back("-skiproundtrip");
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-'
            && strcmp(argv[i], "-")
            && strstr(argv[i], "-validat") != argv[i]
            && strstr(argv[i], "-verif") != argv[i]
            && strstr(argv[i], "-socket") != argv[i]
            && strstr(argv[i], "-fork") != argv[i]
			&& strstr(argv[i], "-listen") != argv[i]
			&& strstr(argv[i], "-permissive") != argv[i]
			&& strstr(argv[i], "-roundtrip") != argv[i]) {
            args.push_back(argv[i]);
        }
    }
    args.push_back("-"); // read from stdin, write to stdout
    auto encode_pipes = IOUtil::start_subprocess(args.size(), &args[0], false);
    lepton_data->reserve(4096 * 1024);
    size_t size = 0;
    Sirikata::Array1d<uint8_t, 16> md5 = IOUtil::transfer_and_md5(header,
        start_byte,
        end_byte,
        true,
        *reader, encode_pipes.pipe_stdin,
        encode_pipes.pipe_stdout,
        &size,
        lepton_data,
        permissive_jpeg_return,
        is_socket);
    auto decode_pipes = IOUtil::start_subprocess(args.size(), &args[0], false);
    size_t roundtrip_size = 0;
    Sirikata::Array1d<uint8_t, 16> rtmd5;
    if (header.size() <= lepton_data->size()) {
        // validate with decode
        rtmd5 = IOUtil::send_and_md5_result(
            lepton_data->data(),
            lepton_data->size(),
            decode_pipes.pipe_stdin,
            decode_pipes.pipe_stdout,
            &roundtrip_size);
    }
    if (roundtrip_size != size || memcmp(&md5[0], &rtmd5[0], md5.size()) != 0) {
        if (is_permissive) {
            return ValidationContinuation::EVALUATE_AS_PERMISSIVE;
        }
        fprintf(stderr, "Input Size %lu != Roundtrip Size %lu\n", (unsigned long)size, (unsigned long)roundtrip_size);
        for (size_t i = 0; i < md5.size(); ++i) {
            fprintf(stderr, "%02x", md5[i]);
        }
        fprintf(stderr, " != ");
        for (size_t i = 0; i < rtmd5.size(); ++i) {
            fprintf(stderr, "%02x", rtmd5[i]);
        }
        fprintf(stderr, "\n");
        custom_exit(ExitCode::UNSUPPORTED_JPEG);
    }
#else
    int jpeg_input_pipes[2] = {-1, -1};
    int lepton_output_pipes[2] = {-1, -1};
    int lepton_roundtrip_send[2] = {-1, -1};
    int jpeg_roundtrip_recv[2] = {-1, -1};
    //int err;
    while(pipe(jpeg_input_pipes) < 0 && errno == EINTR){}
    while(pipe(lepton_output_pipes) < 0 && errno == EINTR){}
    pid_t encode_pid;
    pid_t decode_pid;
    if ((encode_pid = fork()) == 0) { // could also fork/exec here
        // not yet open -- we will exit before accessed while(close(*fwriter) < 0 && errno == EINTR){}
        if (*writer != -1 && *writer != *reader) {
                while(close(*writer) < 0 && errno == EINTR){}
        }
        while(close(*reader) < 0 && errno == EINTR){}
        *reader = jpeg_input_pipes[0];
        *writer = lepton_output_pipes[1];
        while(close(jpeg_input_pipes[1]) < 0 && errno == EINTR){}
        while(close(lepton_output_pipes[0]) < 0 && errno == EINTR){}
        return ValidationContinuation::CONTINUE_AS_JPEG;
    }
    while(close(jpeg_input_pipes[0]) < 0 && errno == EINTR){}
    while(close(lepton_output_pipes[1]) < 0 && errno == EINTR){}

    while(pipe(lepton_roundtrip_send) < 0 && errno == EINTR){}
    while(pipe(jpeg_roundtrip_recv) < 0 && errno == EINTR){}
    // we wanna fork the decode here before we allocate 4096 * 1024 bytes here
    if ((decode_pid = fork()) == 0) { // could also fork/exec here
        if (*writer != -1 && *writer != *reader) {
                while(close(*writer) < 0 && errno == EINTR){}
        }

        while(close(*reader) < 0 && errno == EINTR){}
        // not yet open -- we will exit before accessed while(close(*fwriter) < 0 && errno == EINTR){}
        while(close(jpeg_input_pipes[1]) < 0 && errno == EINTR){}
        while(close(lepton_output_pipes[0]) < 0 && errno == EINTR){}

        *reader = lepton_roundtrip_send[0];
        *writer = jpeg_roundtrip_recv[1];
        while(close(lepton_roundtrip_send[1]) < 0 && errno == EINTR){}
        while(close(jpeg_roundtrip_recv[0]) < 0 && errno == EINTR){}

        return ValidationContinuation::CONTINUE_AS_LEPTON;
    }
    while(close(lepton_roundtrip_send[0]) < 0 && errno == EINTR){}
    while(close(jpeg_roundtrip_recv[1]) < 0 && errno == EINTR){}


    lepton_data->reserve(4096 * 1024);
    size_t size = 0;
    Sirikata::Array1d<uint8_t, 16> md5 = IOUtil::transfer_and_md5(header,
                                                                  start_byte,
                                                                  end_byte,
                                                                  false,
                                                                  *reader, jpeg_input_pipes[1],
                                                                  lepton_output_pipes[0],
                                                                  &size,
                                                                  lepton_data,
                                                                  permissive_jpeg_return,
                                                                  is_socket);
    int status = 0;
    while (waitpid(encode_pid, &status, 0) < 0 && errno == EINTR) {} // wait on encode
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code != 0) {
            if (is_permissive) {
                return ValidationContinuation::EVALUATE_AS_PERMISSIVE;
            }
            exit(exit_code);
        }
    } else if (WIFSIGNALED(status)) {
        if (is_permissive) {
            return ValidationContinuation::EVALUATE_AS_PERMISSIVE;
        }
        raise(WTERMSIG(status));
    }
    size_t roundtrip_size = 0;
    // validate with decode
    Sirikata::Array1d<uint8_t, 16> rtmd5;
    if (header.size() <= lepton_data->size()) {
        rtmd5 = IOUtil::send_and_md5_result(
            &(*lepton_data)[header.size()],
            lepton_data->size() - header.size(),
            lepton_roundtrip_send[1],
            jpeg_roundtrip_recv[0],
            &roundtrip_size);
    }
    if (roundtrip_size != size || memcmp(&md5[0], &rtmd5[0], md5.size()) != 0) {
        if (is_permissive) {
            return ValidationContinuation::EVALUATE_AS_PERMISSIVE;
        }
        fprintf(stderr, "Input Size %ld != Roundtrip Size %ld\n", size, roundtrip_size);
        for (size_t i = 0; i < md5.size(); ++i) {
            fprintf(stderr, "%02x", md5[i]);            
        }
        fprintf(stderr, " != ");
        for (size_t i = 0; i < rtmd5.size(); ++i) {
            fprintf(stderr, "%02x", rtmd5[i]);
        }
        fprintf(stderr, "\n");
        custom_exit(ExitCode::ROUNDTRIP_FAILURE);
    }
    
    status = 0;
    while (waitpid(decode_pid, &status, 0) < 0 && errno == EINTR) {} // wait on encode
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code != 0) {
            if (is_permissive) {
                return ValidationContinuation::EVALUATE_AS_PERMISSIVE;
            }
            exit(exit_code);
        }
    } else if (WIFSIGNALED(status)) {
        if (is_permissive) {
            return ValidationContinuation::EVALUATE_AS_PERMISSIVE;
        }
        raise(WTERMSIG(status));
    }
#endif
    *validation_exit_code = ExitCode::SUCCESS;
    return ValidationContinuation::ROUNDTRIP_OK;
}

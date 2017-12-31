enum class ValidationContinuation {
    ROUNDTRIP_OK,
    EVALUATE_AS_PERMISSIVE,
    BAD,
    CONTINUE_AS_JPEG,
    CONTINUE_AS_LEPTON,
};
ValidationContinuation generic_compress(const std::vector<uint8_t>*input,
                                        Sirikata::MuxReader::ResizableByteBuffer *lepton_data,
                                        ExitCode *validation_exit_code);

ValidationContinuation validateAndCompress(int *reader, int *writer,
                                           Sirikata::Array1d<uint8_t, 2> header,
                                           size_t header_size,
                                           size_t start_byte,
                                           size_t end_byte,
                                           ExitCode *validation_exit_code,
                                           Sirikata::MuxReader::ResizableByteBuffer *output,
                                           int argc, 
                                           const char** argv,
                                           bool is_permissive,
                                           bool is_socket,
                                           std::vector<uint8_t> *permissive_jpeg_return);

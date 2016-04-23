enum class ValidationContinuation {
    ROUNDTRIP_OK,
    BAD,
    CONTINUE_AS_JPEG,
    CONTINUE_AS_LEPTON,
};



ValidationContinuation validateAndCompress(int *reader, int*writer,
                                           Sirikata::Array1d<uint8_t, 2> header,
                                           size_t start_byte,
                                           size_t end_byte,
                                           ExitCode *validation_exit_code);

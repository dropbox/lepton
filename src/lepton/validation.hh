enum class ValidationContinuation {
    ROUNDTRIP_OK,
    BAD,
    CONTINUE_AS_JPEG,
    CONTINUE_AS_LEPTON,
};
int open_fdout(const char *ifilename,
               IOUtil::FileWriter *writer,
               Sirikata::Array1d<uint8_t, 2> fileid,
               bool force_compressed_output);


ValidationContinuation validateAndCompress(int *reader, int*writer,
                                           const char * ifilename,
                                           IOUtil::FileWriter *fwriter, // <-- may be null
                                           Sirikata::Array1d<uint8_t, 2> header,
                                           size_t start_byte,
                                           size_t end_byte,
                                           ExitCode *validation_exit_code,
                                           bool output_as_zlib0);

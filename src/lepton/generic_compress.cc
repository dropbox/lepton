#include "../vp8/util/memory.hh"
#include "../vp8/util/nd_array.hh"
#include "../io/MuxReader.hh"
#include "jpgcoder.hh"
#include "../io/Reader.hh"
#include "../io/MemReadWriter.hh"
#include "../io/ZlibCompression.hh"
#include "../io/BrotliCompression.hh"
#include "thread_handoff.hh"
#include "validation.hh"
#include "generic_compress.hh"
#include "../io/Seccomp.hh"
#include "../vp8/util/billing.hh"
#ifndef GIT_REVISION
#include "version.hh"
#ifndef GIT_REVISION
#define GIT_REVISION "unknown"
#endif
#endif
class ResizableBufferWriter : public Sirikata::DecoderWriter {
    Sirikata::MuxReader::ResizableByteBuffer * backing;
public:
    ResizableBufferWriter(Sirikata::MuxReader::ResizableByteBuffer * res) {
        backing = res;
    }
    void Close() {}
    std::pair<uint32_t, Sirikata::JpegError> Write(const uint8_t*data, unsigned int size) {
        using namespace Sirikata;
        size_t old_size = backing->size();
        backing->resize(old_size + size);
        memcpy(&(*backing)[old_size], data, size);
        return std::pair<uint32_t, JpegError>(size, JpegError::nil());
    }
};
static const unsigned char   lepton_header[] = { 0xcf, 0x84 }; // the tau symbol for a tau lepton in utf-8


unsigned char basic_header[] = {
    /* 0xff, 0xd8,*/ 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
  0x01, 0x02, 0x00, 0x1c, 0x00, 0x1c, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43,
  0x00, 0x03, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x02, 0x02, 0x02, 0x03,
  0x03, 0x03, 0x03, 0x04, 0x06, 0x04, 0x04, 0x04, 0x04, 0x04, 0x08, 0x06,
  0x06, 0x05, 0x06, 0x09, 0x08, 0x0a, 0x0a, 0x09, 0x08, 0x09, 0x09, 0x0a,
  0x0c, 0x0f, 0x0c, 0x0a, 0x0b, 0x0e, 0x0b, 0x09, 0x09, 0x0d, 0x11, 0x0d,
  0x0e, 0x0f, 0x10, 0x10, 0x11, 0x10, 0x0a, 0x0c, 0x12, 0x13, 0x12, 0x10,
  0x13, 0x0f, 0x10, 0x10, 0x10, 0xff, 0xc0, 0x00, 0x0b, 0x08, 0x00, 0x01,
  0x00, 0x01, 0x01, 0x01, 0x11, 0x00, 0xff, 0xc4, 0x00, 0x14, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x09, 0xff, 0xc4, 0x00, 0x14, 0x10, 0x01, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x3f, 0x00,
    0x54, 0xdd,
};

extern unsigned char ujgversion;
extern bool rebuild_header_jpg();
extern void uint32toLE(uint32_t value, uint8_t *retval);
extern bool hex_to_bin(unsigned char *output, const char *input, size_t output_size);
extern int ujgfilesize;
ValidationContinuation generic_compress(const std::vector<uint8_t>*input,
                                        Sirikata::MuxReader::ResizableByteBuffer *lepton_data,
                                        ExitCode *validation_exit_code){
    if (g_use_seccomp) {
        Sirikata::installStrictSyscallFilter(true);
    }
    lepton_data->resize(0);
    if (input->size() == 0) {
        custom_exit(ExitCode::UNSUPPORTED_JPEG);
    }
    ResizableBufferWriter ujg_out_backing(lepton_data);
    ResizableBufferWriter*ujg_out = &ujg_out_backing;
    unsigned char ujpg_mrk[ 64 ];
    Sirikata::JpegError err = Sirikata::JpegError::nil();

    // lepton-Header
    err = ujg_out->Write( lepton_header, 2 ).second;
    // store version number
    ujpg_mrk[ 0 ] = ujgversion;
    ujg_out->Write( ujpg_mrk, 1 );

    // discard meta information from header if needed
    if ( !rebuild_header_jpg() ){
        return ValidationContinuation::BAD;
    }
    Sirikata::MemReadWriter mrw((Sirikata::JpegAllocator<uint8_t>()));
    //uint32_t framebuffer_byte_size = 0;
    //uint8_t num_rows = 1;
    std::vector<ThreadHandoff> selected_splits(NUM_THREADS);
    std::vector<int> split_indices(NUM_THREADS);
    // write header to file
    // marker: "HDR" + [size of header]
    unsigned char hdr_mrk[] = {'H', 'D', 'R'};
    uint32_t hdrs = sizeof(basic_header);
    err = mrw.Write( hdr_mrk, sizeof(hdr_mrk) ).second;
    uint32toLE(hdrs, ujpg_mrk);
    err = mrw.Write( ujpg_mrk, 4).second;
    // data: data from header
    mrw.Write( basic_header, hdrs );
    // beginning here: recovery information (needed for exact JPEG recovery)

    unsigned char padbit = 0;
    // marker: P0D"
    unsigned char pad_mrk[] = {'P', '0', 'D'};
    err = mrw.Write( pad_mrk, sizeof(pad_mrk) ).second;
    // data: padbit
    err = mrw.Write( (unsigned char*) &padbit, 1 ).second;

    // write luma splits
    unsigned char luma_mrk[1] = {'H'};
    err = mrw.Write( luma_mrk, sizeof(luma_mrk) ).second;
    // data: serialized luma splits
    auto serialized_splits = ThreadHandoff::serialize(&selected_splits[0], selected_splits.size());
    err = mrw.Write(&serialized_splits[0], serialized_splits.size()).second;

    if (false) { // early eof encountered
        /*
        unsigned char early_eof[] = {'E', 'E', 'E'};
        err = mrw.Write( early_eof, sizeof(early_eof) ).second;
        uint32toLE(max_cmp, ujpg_mrk);
        uint32toLE(max_bpos, ujpg_mrk + 4);
        uint32toLE(max_sah, ujpg_mrk + 8);
        uint32toLE(max_dpos[0], ujpg_mrk + 12);
        uint32toLE(max_dpos[1], ujpg_mrk + 16);
        uint32toLE(max_dpos[2], ujpg_mrk + 20);
        uint32toLE(max_dpos[3], ujpg_mrk + 24);
        err = mrw.Write(ujpg_mrk, 28).second;
        */
    }
    // write garbage (data including and after EOI) (if any) to file
    // marker: "GRB" + [size of garbage]
    {
        unsigned char grb_mrk[] = {'P', 'G', 'E'};
        err = mrw.Write( grb_mrk, sizeof(grb_mrk) ).second;
        uint32_t prefix_grbs = input->size();
        uint32toLE(prefix_grbs, ujpg_mrk);
        err = mrw.Write( ujpg_mrk, 4 ).second;
        // data: garbage data
        err = mrw.Write( &(*input)[0], input->size()).second;
    }
// write garbage (data including and after EOI) (if any) to file
    // marker: "GRB" + [size of garbage]
    {
        unsigned char grb_mrk[] = {'G', 'R', 'B'};
        err = mrw.Write( grb_mrk, sizeof(grb_mrk) ).second;
        uint32_t grbs = 0;
        uint32toLE(grbs, ujpg_mrk);
        err = mrw.Write( ujpg_mrk, 4 ).second;
        // data: no garbage data
        //err = mrw.Write(&(*input)[input->size() - 2], 2).second;
    }
    std::vector<uint8_t, Sirikata::JpegAllocator<uint8_t> > compressed_header;
    if (ujgversion == 1) {
        compressed_header =
            Sirikata::ZlibDecoderCompressionWriter::Compress(mrw.buffer().data(),
                                                             mrw.buffer().size(),
                                                             Sirikata::JpegAllocator<uint8_t>());
    } else {
        compressed_header = Sirikata::BrotliCodec::Compress(mrw.buffer().data(),
                                                            mrw.buffer().size(),
                                                            Sirikata::JpegAllocator<uint8_t>());
    }
    write_byte_bill(Billing::HEADER, false, 2 + hdrs + input->size());
    static_assert(MAX_NUM_THREADS <= 255, "We only have a single byte for num threads");
    always_assert(NUM_THREADS <= 255);
    unsigned char zed[] = {'Y'};
    err =  ujg_out->Write(zed, sizeof(zed)).second;
    unsigned char num_threads[] = {(unsigned char)NUM_THREADS};
    err =  ujg_out->Write(num_threads, sizeof(num_threads)).second;
    unsigned char zero3[3] = {};
    err =  ujg_out->Write(zero3, sizeof(zero3)).second;
    unsigned char git_revision[12] = {0}; // we only have 12 chars in the header for this
    hex_to_bin(git_revision, GIT_REVISION, sizeof(git_revision));
    err = ujg_out->Write(git_revision, sizeof(git_revision) ).second;
    uint32_t jpgfilesize = input->size();
    uint32toLE(jpgfilesize, ujpg_mrk);
    err = ujg_out->Write( ujpg_mrk, 4).second;
    write_byte_bill(Billing::HEADER, true, 24);
    uint32toLE((uint32_t)compressed_header.size(), ujpg_mrk);
    err = ujg_out->Write( ujpg_mrk, 4).second;
    write_byte_bill(Billing::HEADER, true, 4);
    auto err2 = ujg_out->Write(compressed_header.data(),
                               compressed_header.size());
    write_byte_bill(Billing::HEADER, true, compressed_header.size());
    //uint32_t zlib_hdrs = compressed_header.size();
    if (err != Sirikata::JpegError::nil() || err2.second != Sirikata::JpegError::nil()) {
        fprintf( stderr, "write error, possibly drive is full" );
        return ValidationContinuation::BAD;
    }
    unsigned char cmp_mrk[] = {'C', 'M', 'P'};
    err = ujg_out->Write( cmp_mrk, sizeof(cmp_mrk) ).second;
    // errormessage if write error
    if ( err != Sirikata::JpegError::nil() ) {
        fprintf( stderr, "write error, possibly drive is full" );
        return ValidationContinuation::BAD;
    }
    Sirikata::MuxWriter mux_writer(ujg_out, Sirikata::JpegAllocator<uint8_t>(), ujgversion);
    write_byte_bill(Billing::DELIMITERS, true, mux_writer.getOverhead());
    mux_writer.Close();

    write_byte_bill(Billing::HEADER, true, 3);
    /*while (g_encoder->encode_chunk(&colldata, ujg_out,
                                   &selected_splits[0], selected_splits.size()) == CODING_PARTIAL) {
                                   }*/
    uint32_t out_file_size = lepton_data->size() + 4; // gotta include the final uint32_t
    uint8_t out_buffer[sizeof(out_file_size)] = {};
    for (uint8_t i = 0; i < sizeof(out_file_size); ++i) {
        out_buffer[i] = out_file_size & 0xff;
        out_file_size >>= 8;
    }
    err = ujg_out->Write(out_buffer, sizeof(out_file_size)).second;
        // errormessage if write error
    if ( err != Sirikata::JpegError::nil() ) {
        fprintf( stderr, "write error, possibly drive is full" );
        return ValidationContinuation::BAD;
    }
    // get filesize, if avail

    ujgfilesize = lepton_data->size();


    *validation_exit_code = ExitCode::SUCCESS;
    return ValidationContinuation::ROUNDTRIP_OK;
}

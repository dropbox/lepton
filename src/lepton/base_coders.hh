#ifndef _BASE_CODERS_
#define _BASE_CODERS_
#include "../vp8/util/nd_array.hh"
#include "../vp8/util/generic_worker.hh"
#include "../vp8/util/block_based_image.hh"
#include "thread_handoff.hh"
struct GenericWorker;
enum CodingReturnValue {
    CODING_ERROR,
    CODING_DONE,
    CODING_PARTIAL, // run it again
};
class UncompressedComponents;

namespace Sirikata {
class SwitchableXZBase;
class DecoderCompressionWriter;
class DecoderReader;
class DecoderWriter;
template <class T> class SwitchableDecompressionReader;
template <class T> class SwitchableCompressionWriter;
}
namespace IOUtil {
class FileWriter;
}
class BaseDecoder {
 public:
    virtual ~BaseDecoder(){}
    virtual void initialize(Sirikata::DecoderReader *input,
                                const std::vector<ThreadHandoff>& thread_transition_info) = 0;
    virtual CodingReturnValue decode_chunk(UncompressedComponents*dst) = 0;
    virtual void registerWorkers(Sirikata::Array1d<GenericWorker, (NUM_THREADS - 1)>* workers) = 0;
    virtual GenericWorker* getWorker(int i) = 0;
    virtual std::vector<ThreadHandoff> initialize_baseline_decoder(const UncompressedComponents * const colldata,
                                             Sirikata::Array1d<BlockBasedImagePerChannel<true>,
                                                               NUM_THREADS>& framebuffer) = 0;
    virtual void decode_row(int thread_state_id,
                            BlockBasedImagePerChannel<true>& image_data, // FIXME: set image_data to true
                            Sirikata::Array1d<uint32_t,
                                              (uint32_t)ColorChannel::
                                              NumBlockTypes> component_size_in_blocks,
                            int component,
                            int curr_y) = 0;
    virtual void clear_thread_state(int thread_id, int target_thread_state, BlockBasedImagePerChannel<true>& framebuffer) = 0;
};


class BaseEncoder {
 public:
    virtual ~BaseEncoder(){}
    virtual void registerWorkers(Sirikata::Array1d<GenericWorker,
                                                   (NUM_THREADS - 1)>* workers) = 0;

    virtual CodingReturnValue encode_chunk(const UncompressedComponents *input,
                                           IOUtil::FileWriter *,
                                           Sirikata::Array1d<ThreadHandoff,
                                                             NUM_THREADS> selected_splits) = 0;
};

#endif

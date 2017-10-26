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
    virtual void registerWorkers(GenericWorker * workers, unsigned int num_workers) = 0;
    virtual GenericWorker* getWorker(unsigned int i) = 0;
    virtual unsigned int getNumWorkers()const = 0;
    virtual std::vector<ThreadHandoff> initialize_baseline_decoder(const UncompressedComponents * const colldata,
                                             Sirikata::Array1d<BlockBasedImagePerChannel<true>,
                                                               MAX_NUM_THREADS>& framebuffer) = 0;
    virtual void decode_row(int thread_state_id,
                            BlockBasedImagePerChannel<true>& image_data, // FIXME: set image_data to true
                            Sirikata::Array1d<uint32_t,
                                              (uint32_t)ColorChannel::
                                              NumBlockTypes> component_size_in_blocks,
                            int component,
                            int curr_y) = 0;
    virtual size_t get_model_memory_usage() const = 0;
    virtual size_t get_model_worker_memory_usage() const = 0;
    virtual void flush() = 0;
    virtual void map_logical_thread_to_physical_thread(int thread_id, int target_thread_state) = 0;
    virtual void clear_thread_state(int thread_id, int target_thread_state, BlockBasedImagePerChannel<true>& framebuffer) = 0;
    virtual void reset_all_comm_buffers() = 0;
};


class BaseEncoder {
 public:
    virtual ~BaseEncoder(){}
    virtual void registerWorkers(GenericWorker * workers, unsigned int num_workers) = 0;

    virtual CodingReturnValue encode_chunk(const UncompressedComponents *input,
                                           IOUtil::FileWriter *,
                                           const ThreadHandoff * selected_splits,
                                           unsigned int num_selected_splits) = 0;
    virtual size_t get_decode_model_memory_usage() const = 0;
    virtual size_t get_decode_model_worker_memory_usage() const = 0;
};

#endif

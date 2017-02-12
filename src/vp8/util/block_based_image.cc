#include <vector>
#include <atomic>
#include "block_based_image.hh"

std::atomic<std::vector<NeighborSummary> *>gNopNeighbor;
uint8_t custom_nop_storage[sizeof(AlignedBlock) * 6 + 31] = {0};

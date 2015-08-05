#ifndef _BLOCK_CONTEXT_HH_
#define _BLOCK_CONTEXT_HH_

// in raytracing we usually find that having 32 bit offsets to pointers ends up being more
// efficient in our datastructures, since array offseting instructions are so fast.
struct BlockContext {
    AlignedBlock * cur;
    int32_t is_left_avail; // 0 is unavail, other values indicate neighbor is -1
    int32_t up_offset; //offset from cur; 0 for unavail
    constexpr bool has_left()const {
        return is_left_avail != 0;
    }
    constexpr bool has_above()const {
        return up_offset != 0;
    }
    constexpr const AlignedBlock* left() const {
        return is_left_avail ? &cur[-1] : nullptr;
    }
    constexpr const AlignedBlock* above() const {
        return up_offset ? &cur[up_offset] : nullptr;
    }
    constexpr const AlignedBlock* above_left() const {
        return up_offset && is_left_avail ? &cur[up_offset - 1] : nullptr;
    }
    AlignedBlock* left() {
        return is_left_avail ? &cur[-1] : nullptr;
    }
    AlignedBlock* above() {
        return up_offset ? &cur[up_offset] : nullptr;
    }
    AlignedBlock* above_left() {
        return up_offset && is_left_avail ? &cur[up_offset - 1] : nullptr;
    }
    constexpr const AlignedBlock& here() const {
        return *cur;
    }
    constexpr const AlignedBlock& left_unchecked() const {
        return cur[-1];
    }
    constexpr const AlignedBlock& above_unchecked() const {
        return cur[up_offset];
    }
    constexpr const AlignedBlock& above_left_unchecked() const {
        return cur[up_offset - 1];
    }
    static BlockContext nil() {
        return {nullptr, 0, 0};
    }
    bool isNil() {
        return cur == nullptr && is_left_avail == 0 && up_offset == 0;
    }
    AlignedBlock& here() {
        return *cur;
    }
    AlignedBlock& left_unchecked() {
        return cur[-1];
    }
    AlignedBlock& above_unchecked() {
        return cur[up_offset];
    }
    AlignedBlock& above_left_unchecked() {
        return cur[up_offset - 1];
    }
};

struct VContext {
    BlockContext context;
    int y;
    VContext(): context(BlockContext::nil()), y(0) {}
};
#endif

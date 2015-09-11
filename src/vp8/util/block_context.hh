#ifndef _BLOCK_CONTEXT_HH_
#define _BLOCK_CONTEXT_HH_

// in raytracing we usually find that having 32 bit offsets to pointers ends up being more
// efficient in our datastructures, since array offseting instructions are so fast.
template <class ABlock> struct MBlockContext {
    ABlock * cur;
    int32_t is_left_avail; // 0 is unavail, other values indicate neighbor is -1
    int32_t up_offset; //offset from cur; 0 for unavail
    MBlockContext<const AlignedBlock> copy() const {
        return {cur, is_left_avail, up_offset};
    }
    constexpr bool has_left()const {
        return is_left_avail != 0;
    }
    constexpr bool has_above()const {
        return up_offset != 0;
    }
    constexpr const ABlock* left() const {
        return is_left_avail ? &cur[-1] : nullptr;
    }
    constexpr const ABlock* above() const {
        return up_offset ? &cur[up_offset] : nullptr;
    }
    constexpr const ABlock* above_left() const {
        return up_offset && is_left_avail ? &cur[up_offset - 1] : nullptr;
    }
    ABlock* left() {
        return is_left_avail ? &cur[-1] : nullptr;
    }
    ABlock* above() {
        return up_offset ? &cur[up_offset] : nullptr;
    }
    ABlock* above_left() {
        return up_offset && is_left_avail ? &cur[up_offset - 1] : nullptr;
    }
    constexpr const ABlock& here() const {
        return *cur;
    }
    constexpr const ABlock& left_unchecked() const {
        return cur[-1];
    }
    constexpr const ABlock& above_unchecked() const {
        return cur[up_offset];
    }
    constexpr const ABlock& above_left_unchecked() const {
        return cur[up_offset - 1];
    }
    static MBlockContext nil() {
        return {nullptr, 0, 0};
    }
    bool isNil() {
        return cur == nullptr && is_left_avail == 0 && up_offset == 0;
    }
    ABlock& here() {
        return *cur;
    }
    ABlock& left_unchecked() {
        return cur[-1];
    }
    ABlock& above_unchecked() {
        return cur[up_offset];
    }
    ABlock& above_left_unchecked() {
        return cur[up_offset - 1];
    }
};
typedef MBlockContext<AlignedBlock> BlockContext;
typedef MBlockContext<const AlignedBlock> ConstBlockContext;

struct VContext {
    BlockContext context;
    int y;
    VContext(): context(BlockContext::nil()), y(0) {}
};
struct KVContext {
    ConstBlockContext context;
    int y;
    KVContext(): context(ConstBlockContext::nil()), y(0) {}
};
#endif

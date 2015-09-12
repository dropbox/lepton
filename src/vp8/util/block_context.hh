#ifndef _BLOCK_CONTEXT_HH_
#define _BLOCK_CONTEXT_HH_

// in raytracing we usually find that having 32 bit offsets to pointers ends up being more
// efficient in our datastructures, since array offseting instructions are so fast.
template <class ABlock> struct MBlockContext {
    ABlock * cur;
    ABlock * above; //offset from cur; 0 for unavail
    MBlockContext<const AlignedBlock> copy() const {
        return {cur, above};
    }
    constexpr const ABlock& here() const {
        return cur[0];
    }
    constexpr const ABlock& left_unchecked() const {
        return cur[-1];
    }
    constexpr const ABlock& above_unchecked() const {
        return above[0];
    }
    constexpr const ABlock& above_left_unchecked() const {
        return above[-1];
    }
    static MBlockContext nil() {
        return {nullptr, nullptr};
    }
    bool isNil() {
        return cur == nullptr && above == nullptr;
    }
    ABlock& here() {
        return cur[0];
    }
    ABlock& left_unchecked() {
        return cur[-1];
    }
    ABlock& above_unchecked() {
        return above[0];
    }
    ABlock& above_left_unchecked() {
        return above[-1];
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

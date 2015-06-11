class CollData {
    signed short *cmpoffset_[4]; // pointers to the beginning of each component
    int bch_[4];
    int bcv_[4];
    signed short *colldata_; // we may want to swizzle this for locality
    
    std::atomic<int> bit_progress_; // right now we assume baseline ordering--in the future we may want an array of scans
    int allocated_;
    int last_worker_target_; // only accessed from main thread
    CollData(const CollData&);// not implemented
    CollData&operator=(const CollData&);// not implemented
public:
    CollData() : bit_progress_(0) {
        colldata_ = NULL;
        allocated_ = 0;
        memset(bch_, 0, sizeof(int) * 4);
        memset(bcv_, 0, sizeof(int) * 4);
        memset(cmpoffset_, 0, sizeof(signed short*) * 4);
        last_worker_target_ = 0;
    }
    void worker_update_progress(int new_bit_progress) {
        atomic_thread_fence(std::memory_order_release);
        bit_progress_ += new_bit_progress;
    }
    void init(componentInfo cmpinfo[ 4 ], int cmpc) {
        allocated_ = 0;
        for (int cmp = 0; cmp < cmpc; cmp++) {
            bch_[cmp] = cmpinfo[cmp].bch;
            bcv_[cmp] = cmpinfo[cmp].bcv;
            allocated_ += cmpinfo[cmp].bc * 64;
        }
        colldata_ = new signed short[allocated_];
        int total = 0;
        for (int cmp = 0; cmp < 4; cmp++) {
            cmpoffset_[cmp] = colldata_ + total;
            if (cmp < cmpc) {
                total += cmpinfo[cmp].bc * 64;
            }
        }
    }
    int coordinate_to_bit_progress(int cmp, int bpos, int dpos) {
        return ((cmpoffset_[cmp] - colldata_) + dpos * 64 + bpos) << 3;
    }
    void wait_for_worker(int cmp, int bpos, int dpos) {
        int worker_target = coordinate_to_bit_progress(cmp, bpos, dpos);
        if (last_worker_target_ > worker_target) {
            return;
        }
        int cur_worker_progress = bit_progress_.load(std::memory_order_relaxed);
        last_worker_target_ = cur_worker_progress;        
        if (cur_worker_progress > worker_target ) {
            std::atomic_thread_fence(std::memory_order_acquire);
            return;
        }
    }
    unsigned int component_size_in_bytes(int cmp) {
        return sizeof(short) * bch_[cmp] * bcv_[cmp] * 64;
    }
    unsigned int component_size_in_shorts(int cmp) {
        return bch_[cmp] * bcv_[cmp] * 64;
    }
    signed short&operator()(int cmp, int bpos, int x, int y) {
        return cmpoffset_[cmp][64 * (y * bch_[cmp] + x) + bpos]; // fixme: do we care bout nch?
    }
    signed short* full_component(int cmp) {
        return cmpoffset_[cmp];
    }
    const signed short* full_component(int cmp) const{
        return cmpoffset_[cmp];
    }
    signed short operator()(int cmp, int bpos, int x, int y) const {
        return cmpoffset_[cmp][64 * (y * bch_[cmp] + x) + bpos]; // fixme: do we care bout nch?
    }
    signed short&operator()(int cmp, int bpos, int dpos) {
        return cmpoffset_[cmp][dpos * 64 + bpos];
    }
    signed short operator()(int cmp, int bpos, int dpos) const{
        return cmpoffset_[cmp][dpos * 64 + bpos];
    }
/*
    signed short* block(int cmp, int x, int y) {
        return &cmpoffset_[cmp][(y * bch_[cmp] + x) * 64]; // fixme: do we care bout nch?
    }
    const signed short* block(int cmp, int x, int y) const {
        return &cmpoffset_[cmp][(y * bch_[cmp] + x) * 64]; // fixme: do we care bout nch?
    }
    const signed short* block(int cmp, int dpos) const{
        return &cmpoffset_[cmp][dpos * 64];
    }
    signed short* block(int cmp, int dpos) {
        return &cmpoffset_[cmp][dpos * 64];
    }
*/
    void reset() {
        if (colldata_) {
            delete []colldata_;
        }
        bit_progress_.store(0);
        colldata_ = NULL;
    }
    ~CollData() {
        reset();
    }
};

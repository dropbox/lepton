#include "../util/options.hh"
#include "../../ans/rans64.hh"
#include "../model/branch.hh"
struct Symbol {
    Probability start;
    Probability prob;
};

union UnionSymbolPair {
    struct {
        Symbol first;
        Symbol second;
    } sym;
    uint32_t data;
};
class ANSBoolWriter
{
    static uint32_t compute_bill(uint8_t prob) {
#if defined(ENABLE_BILLING) || !defined(NDEBUG)
        double val = log2((prob + 0.5) / 256.);
        double whole_number = (int)val;
        double del = val - whole_number;
        return (rand() < RAND_MAX *del ? 1 : 0) + whole_number;
#else
        return 0;
#endif
    }
    std::vector<UnionSymbolPair> symbol_buffer;
    std::vector<uint32_t> output;
    std::pair<Rans64State, Rans64State> rans_pair;
    bool odd;
    ANSBoolWriter() {
        Rans64EncInit(&rans_pair.first);
        Rans64EncInit(&rans_pair.second);
        odd = false;
    }
    void init () {
        
    }
    void put( const bool value, Branch & branch, Billing bill) {
        Symbol sym;
        Probability prob =  branch.prob();
        if (prob < 128) {
            prob += 1;
        }
        sym.start = value ? 255 ^ prob: 0;
        sym.prob = prob;
        if (odd) {
            symbol_buffer.back().sym.second = sym;
        }else {
            symbol_buffer.push_back(UnionSymbolPair());
            symbol_buffer.back().sym.first = sym;
        }
        odd = !odd;
        write_bit_bill(bill, true, compute_bill(value ? sym.prob : (255 ^ sym.prob)));
        branch.record_obs_and_update(value);
    }
    void finish(Sirikata::MuxReader::ResizableByteBuffer &final_buffer) {
        static_assert(sizeof(uint32_t) == sizeof(UnionSymbolPair), "Union must pack neatly into uint32_t array");
        UnionSymbolPair nop_sym;
        nop_sym.sym.first.start = 0;
        nop_sym.sym.first.prob = 128;
        nop_sym.sym.second.start = 0;
        nop_sym.sym.second.prob = 128;
        symbol_buffer.push_back(nop_sym); // two extra bits
        symbol_buffer.push_back(nop_sym);
        std::vector<UnionSymbolPair>::reverse_iterator i = symbol_buffer.rbegin();
        ++i; // ignore 2 extra bits
        ++i;
        uint32_t *pptr = &symbol_buffer.back().data;
        uint32_t *finish = pptr;
        for (std::vector<UnionSymbolPair>::reverse_iterator ie = symbol_buffer.rend();
             i!=ie; ++i) {
            always_assert(pptr + 2 >= &i->data && "we can't have a 16:1 expansion ratio due to 8 bit probability ranges");
            Rans64EncPut(&rans_pair.first, &pptr, i->sym.first.start, i->sym.first.prob, 8);
            Rans64EncPut(&rans_pair.second, &pptr, i->sym.second.start, i->sym.second.prob, 8);
        }
        final_buffer.resize(finish - pptr + 16);
        memcpy(final_buffer.data() + sizeof(uint64_t) + sizeof(uint64_t), pptr, finish - pptr);
        memcpy(final_buffer.data(), &rans_pair.first, sizeof(uint64_t));
        memcpy(final_buffer.data() + sizeof(uint64_t), &rans_pair.second, sizeof(uint64_t));
    }

};

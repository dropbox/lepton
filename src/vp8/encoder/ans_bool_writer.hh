#ifndef ENABLE_ANS_EXPERIMENTAL
#error "Need to enable ANS compile flag to include ANS"
#endif

#include "../util/options.hh"
#include "../../ans/rans64.hh"
#include "../model/branch.hh"
struct Symbol {
    bool val;
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
    public:
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
        always_assert(prob);
        sym.val = value;
        sym.prob = prob;
        if (odd) {
            symbol_buffer.back().sym.first = sym;
        }else {
            symbol_buffer.push_back(UnionSymbolPair());
            symbol_buffer.back().sym.first.prob = 1;
            symbol_buffer.back().sym.first.val = true;
            symbol_buffer.back().sym.second = sym;
        }
        odd = !odd;
        write_bit_bill(bill, true, compute_bill(value ? sym.prob : (255 ^ sym.prob)));
        branch.adv_record_obs_and_update(value);
    }
    void finish(Sirikata::MuxReader::ResizableByteBuffer &final_buffer) {
        static_assert(sizeof(uint32_t) == sizeof(UnionSymbolPair), "Union must pack neatly into uint32_t array");
        UnionSymbolPair nop_sym;
        nop_sym.sym.first.val = false;
        nop_sym.sym.first.prob = 128;
        nop_sym.sym.second.val = false;
        nop_sym.sym.second.prob = 128;
        symbol_buffer.push_back(nop_sym); // four extra bits
        symbol_buffer.push_back(nop_sym);
        symbol_buffer.push_back(nop_sym);
        symbol_buffer.push_back(nop_sym);
        symbol_buffer.push_back(nop_sym); // four extra bits
        symbol_buffer.push_back(nop_sym);
        symbol_buffer.push_back(nop_sym);
        symbol_buffer.push_back(nop_sym);
        std::vector<UnionSymbolPair>::reverse_iterator i = symbol_buffer.rbegin();
        ++i; // ignore 2 extra bits
        ++i;
        ++i; // ignore 2 extra bits
        ++i;
        uint32_t *pptr = &symbol_buffer.back().data;
        uint32_t *finish = pptr;
        for (std::vector<UnionSymbolPair>::reverse_iterator ie = symbol_buffer.rend();
             i!=ie; ++i) {
            always_assert(pptr + 2 >= &i->data && "we can't have a 16:1 expansion ratio due to 9 bit probability ranges");
            uint32_t first_prob_from_512 = i->sym.first.prob;
            uint32_t first_start = i->sym.first.val ? first_prob_from_512 : 0;
            uint32_t first_freq = i->sym.first.val ? 256 - first_prob_from_512: first_prob_from_512;
            uint32_t second_prob_from_512 = i->sym.second.prob;
            uint32_t second_start = i->sym.second.val ? second_prob_from_512 : 0;
            uint32_t second_freq = i->sym.second.val ? 256 - second_prob_from_512: second_prob_from_512;
            //fprintf(stderr, "Going to encode %d %d (%d) [%d]: stateA = %ld\n", first_start, first_freq, i->sym.first.prob, i->sym.first.val, rans_pair.first);
            Rans64EncPut(&rans_pair.first, &pptr, first_start, first_freq, 8);
            //fprintf(stderr, "Encoded         %d %d (%d) [%d]: stateA = %ld\n", first_start, first_freq, i->sym.first.prob, i->sym.first.val, rans_pair.first);

            //fprintf(stderr, "Going to encode %d %d (%d) [%d]: stateB = %ld\n", second_start, second_freq, i->sym.first.prob, i->sym.second.val, rans_pair.second);
            Rans64EncPut(&rans_pair.second, &pptr, second_start, second_freq, 8);
            //fprintf(stderr, "Encoded         %d %d (%d) [%d]: stateB = %ld\n", second_start, second_freq, i->sym.first.prob, i->sym.second.val, rans_pair.second);
        }
        Rans64EncFlush(&rans_pair.first, &pptr);
        Rans64EncFlush(&rans_pair.second, &pptr);
        final_buffer.resize((finish - pptr + 1) * sizeof(uint32_t));
        memcpy(final_buffer.data(), pptr, (finish - pptr + 1) * sizeof(uint32_t));
    }

};

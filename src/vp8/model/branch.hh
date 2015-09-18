#ifndef _BRANCH_HH_
#define _BRANCH_HH_
#include "numeric.hh"
#include <cmath>
typedef uint8_t Probability;

//#define VP8_ENCODER 1

//#define JPEG_ENCODER
// ^^^ if we want to try to use the JPEG spec arithmetic coder, uncomment above
//#define USE_COUNT_FREE_UPDATE
class Branch
{
private:
  uint8_t counts_[2] = {1, 1};
  Probability probability_ = 128;
#ifdef USE_COUNT_FREE_UPDATE
  uint8_t lprob = 128;
#endif
  friend class JpegBoolDecoder;
  friend class JpegBoolEncoder;
public:
  Probability prob() const { return probability_; }

  uint32_t true_count() const { return counts_[1]; }
  uint32_t false_count() const { return counts_[0]; }

    struct ProbUpdate {
        uint8_t next_prob_true;
        uint8_t next_prob_false;
        uint8_t log_prob_true;
        uint8_t log_prob_false;
    };
    static constexpr double log_base = .9575;
    
    static uint8_t compute_prob_from_log_prob(uint8_t log_prob) {
        uint8_t pval = log_prob >= 128 ? log_prob - 128 : 127 - log_prob;
        uint8_t retval = (uint8_t)(128 * pow(log_base, pval));
        if (log_prob >= 128) {
            return 255 - retval;
        } else {
            return retval;
        }
    }
    static ProbUpdate update_from_log_prob(uint8_t log_prob) {
        ProbUpdate retval;
        int limit = 120;
        if (log_prob == 127) {
            retval.log_prob_false = 128;
        } else if (log_prob > 127){
            if (log_prob < 128 + 40) {
                retval.log_prob_false = log_prob + 1;
            } else if (log_prob < 128 + 96) {
                retval.log_prob_false = log_prob + 2;
            } else {
                retval.log_prob_false = log_prob + 3;
            }
            if (retval.log_prob_false > 127 + limit) retval.log_prob_false = 127 + limit;
        }
        if (log_prob == 128) {
            retval.log_prob_true = 127;
        } else if (log_prob < 128){
            if (log_prob > 127 - 40) {
                retval.log_prob_true = log_prob - 1;
            } else if (log_prob > 127 - 96) {
                retval.log_prob_true = log_prob - 2;
            } else {
                retval.log_prob_true = log_prob - 3;
            }
            if (retval.log_prob_true < 128 - limit) {
                retval.log_prob_true = 128 - limit;
            }
        }
        if (log_prob != 127 && log_prob != 128) {
            bool obs = log_prob > 128; // we're assuming a counteraction
            uint8_t pval = obs ? log_prob - 128 : 127 - log_prob;
            double prob = 0.5 * pow(log_base, (double)pval);
            double new_prob = log_base * prob + 1 - log_base;
            int search_result = pval - 1;
            double best_search_dist = fabs(0.5 * pow(log_base, (double)search_result) - new_prob);
            for (int search = search_result - 1; search > 0; --search) {
                double search_prob = 0.5 * pow(log_base, (double)search);
                if (fabs(search_prob - new_prob) < best_search_dist) {
                    search_result =  search;
                    best_search_dist = fabs(search_prob - new_prob);
                } else break;
            }
            if (obs) {
                retval.log_prob_true = search_result + 128;
            } else {
                retval.log_prob_false = 127 - search_result;
            }
        }
        retval.next_prob_true = compute_prob_from_log_prob(retval.log_prob_true);
        retval.next_prob_false = compute_prob_from_log_prob(retval.log_prob_false);
        return retval;
    }
  __attribute__((always_inline))
  void record_obs_and_update(bool obs) {
      unsigned int fcount = counts_[0];
      unsigned int tcount = counts_[1];
      bool overflow = (counts_[obs]++ == 0xff);
      if (__builtin_expect(overflow, 0)) { // check less than 512
          counts_[0] = ((1 + (unsigned int)fcount) >> 1);
          counts_[1] = ((1 + (unsigned int)tcount) >> 1);
          counts_[obs] = 129;
          probability_ = optimize(counts_[0] + counts_[1]);
      } else {
          probability_ = optimize(fcount + tcount + 1);
      }
#ifdef USE_COUNT_FREE_UPDATE
      auto ret = update_from_log_prob(lprob);
      if (obs) {
          lprob = ret.log_prob_true;
          probability_ = ret.next_prob_true;
      } else {
          lprob = ret.log_prob_false;
          probability_ = ret.next_prob_false;
      }
#endif
  }
  void normalize() {
      counts_[0] = ((1 + (unsigned int)counts_[0]) >> 1);
      counts_[1] = ((1 + (unsigned int)counts_[1]) >> 1);
      
  }
  __attribute__((always_inline))
  Probability optimize(int sum) const
  {
    assert(false_count() && true_count());
#if 0
      const int prob = (false_count() << 8) / sum;
#else
      const int prob = fast_divide10bit(false_count() << 8,
                                        sum);
#endif
      assert( prob >= 0 );
      assert( prob <= 255 );
      
      return (Probability)prob;

#ifdef JPEG_ENCODER
#error needs to be updated
#endif
  }

  Branch(){}
};
#endif

#ifndef BRANCH_HH_
#define BRANCH_HH_
#include "numeric.hh"
#include <cmath>
typedef uint8_t Probability;

//#define VP8_ENCODER 1

//#define JPEG_ENCODER
// ^^^ if we want to try to use the JPEG spec arithmetic coder, uncomment above
class Branch
{
private:
  uint8_t counts_[2];
  Probability probability_;
  friend class JpegBoolDecoder;
  friend class JpegBoolEncoder;
public:
    static Branch update_lookup[256][256][2];
  Probability prob() const { return probability_; }
    static Branch set_particular_value(int false_count, int true_count) {
        Branch retval;
        retval.counts_[0] = false_count;
        retval.counts_[1] = true_count;
        retval.probability_ = retval.optimize(false_count + true_count + 1);
        return retval;
    }
  void set_identity() {
    counts_[0] = 1;
    counts_[1] = 1;
    probability_ = 128;
  }
  bool is_identity() const {
    return counts_[0] == 1 && counts_[1] == 1 && probability_ == 128;
  }
  static Branch identity() {
    Branch retval;
    retval.set_identity();
    return retval;
  }
  uint32_t true_count() const { return counts_[1]; }
  uint32_t false_count() const { return counts_[0]; }
    struct ProbUpdate {
        struct ProbOutcome {
            uint8_t log_prob;
        };
        uint8_t prob;
        ProbOutcome next[2];
        uint8_t& log_prob_false() {
            return next[0].log_prob;
        }
        uint8_t& log_prob_true() {
            return next[1].log_prob;
        }
    };

#ifndef _WIN32
  __attribute__((always_inline))
#endif
  void adv_record_obs_and_update(bool obs) {
      /*
        bool overflow = (counts_[obs]++ == 0xff);
        if (__builtin_expect(overflow, 0)) { // check less than 512
            counts_[!obs] = ((1 + (unsigned int)counts_[!obs]) >> 1);
            counts_[obs] = 129;
        }
        probability_ = optimize(counts_[0] + counts_[1]) | 1;
      */
      unsigned int val = counts_[obs]++;
      if (__builtin_expect(val == 0xff, 0)) {
          unsigned int other_in = counts_[!obs];
          unsigned int other = (other_in + 1) >> 1;
          counts_[obs] = 129;
          counts_[!obs] = other;
      }
      probability_ = optimize(counts_[0] + counts_[1]) | 1;        
  }
  void tbl_record_obs_and_update(bool obs) {
      *this = update_lookup[counts_[0]][counts_[1]][(int)obs];
      this->probability_ |= 1;
  }
    void record_obs_and_update(bool obs) {
        unsigned int fcount = counts_[0];
        unsigned int tcount = counts_[1];
        bool overflow = (counts_[obs]++ == 0xff);
        if (__builtin_expect(overflow, 0)) { // check less than 512
            bool neverseen = counts_[!obs] == 1;
            if (neverseen) {
                counts_[obs] = 0xff;
                probability_ = obs ? 0 : 255;
            } else {
                counts_[0] = ((1 + (unsigned int)fcount) >> 1);
                counts_[1] = ((1 + (unsigned int)tcount) >> 1);
                counts_[obs] = 129;
                probability_ = optimize(counts_[0] + counts_[1]);
            }
        } else {
            probability_ = optimize(fcount + tcount + 1);
        }
    }
    void normalize() {
      counts_[0] = ((1 + (unsigned int)counts_[0]) >> 1);
      counts_[1] = ((1 + (unsigned int)counts_[1]) >> 1);
  }
#ifndef _WIN32
  __attribute__((always_inline))
#endif
  Probability optimize(int sum) const
  {
    dev_assert(false_count() && true_count());
#if 0
      const int prob = (false_count() << 8) / sum;
#else
      const int prob = fast_divide18bit_by_10bit(false_count() << 8,
                                        sum);
#endif
      dev_assert( prob >= 0 );
      dev_assert( prob <= 255 );
      
      return (Probability)prob;

#ifdef JPEG_ENCODER
#error needs to be updated
#endif
  }

  Branch(){}
};
#endif

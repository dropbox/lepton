#ifndef _BRANCH_HH_
#define _BRANCH_HH_
#include "numeric.hh"
typedef uint8_t Probability;

//#define VP8_ENCODER 1

//#define JPEG_ENCODER
// ^^^ if we want to try to use the JPEG spec arithmetic coder, uncomment above

class Branch
{
private:
  uint8_t counts_[2] = {1, 1};
    Probability probability_ = 128;
  friend class JpegBoolDecoder;
  friend class JpegBoolEncoder;
public:
  Probability prob() const { return probability_; }

  uint32_t true_count() const { return counts_[1]; }
  uint32_t false_count() const { return counts_[0]; }
  
  void record_obs_and_update(bool obs) {
      uint32_t fcount = counts_[0];
      uint32_t tcount = counts_[1];

      if (__builtin_expect(counts_[obs] == 0xff, 0)) { // check less than 512
          counts_[0] = (((uint32_t)fcount + 1) >> 1);
          counts_[1] = (((uint32_t)tcount + 1) >> 1);
      }
      counts_[obs] += 1;
      probability_ = optimize(tcount + fcount + 1);
  }
  void normalize() {
      counts_[0] = (((uint32_t)counts_[0] + 1) >> 1);
      counts_[1] = (((uint32_t)counts_[1] + 1) >> 1);
      
  }
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

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
  }
  void normalize() {
      counts_[0] = ((1 + (unsigned int)counts_[0]) >> 1);
      counts_[1] = ((1 + (unsigned int)counts_[1]) >> 1);
      
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

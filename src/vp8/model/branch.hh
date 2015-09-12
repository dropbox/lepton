#ifndef _BRANCH_HH_
#define _BRANCH_HH_
#include "numeric.hh"
typedef uint8_t Probability;
//#define JPEG_ENCODER
// ^^^ if we want to try to use the JPEG spec arithmetic coder, uncomment above

class Branch
{
private:
  Probability probability_ = 128;
  uint8_t false_count_ = 1, true_count_ = 1;
  friend class JpegBoolDecoder;
  friend class JpegBoolEncoder;
public:
  Probability prob() const { return probability_; }

  uint32_t true_count() const { return true_count_; }
  uint32_t false_count() const { return false_count_; }
  
  void record_true_and_update( void ) {
      if (true_count_ == 0xff) { // check less than 512
          normalize();
      }
      true_count_ += 1;
      probability_ = optimize();
  }
  void record_false_and_update( void ) {
      if (false_count_ == 0xff) {// 2x the size of prob
          normalize();
      }
      false_count_ += 1;
      probability_ = optimize();
  }
  void normalize() {
      true_count_ = (true_count_ >> 1) + (true_count_ & 1);
      false_count_ = (false_count_ >> 1) + (false_count_ & 1);
      
  }
  Probability optimize() const
  {
    assert(false_count() && true_count());
#ifndef JPEG_ENCODER
#if 0
    const int prob = (false_count() << 8) / (false_count() + true_count());
#else
    const int prob = fast_divide10bit(false_count() << 8, (false_count() + true_count()));
#endif
    assert( prob >= 0 );
    assert( prob <= 255 );

    return (Probability)prob;
#endif
  }

  Branch(){}
};
#endif
